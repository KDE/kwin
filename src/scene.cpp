/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2006 Lubos Lunak <l.lunak@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

/*
 Design:

 When compositing is turned on, XComposite extension is used to redirect
 drawing of windows to pixmaps and XDamage extension is used to get informed
 about damage (changes) to window contents. This code is mostly in composite.cpp .

 Compositor::performCompositing() starts one painting pass. Painting is done
 by painting the screen, which in turn paints every window. Painting can be affected
 using effects, which are chained. E.g. painting a screen means that actually
 paintScreen() of the first effect is called, which possibly does modifications
 and calls next effect's paintScreen() and so on, until Scene::finalPaintScreen()
 is called.

 There are 3 phases of every paint (not necessarily done together):
 The pre-paint phase, the paint phase and the post-paint phase.

 The pre-paint phase is used to find out about how the painting will be actually
 done (i.e. what the effects will do). For example when only a part of the screen
 needs to be updated and no effect will do any transformation it is possible to use
 an optimized paint function. How the painting will be done is controlled
 by the mask argument, see PAINT_WINDOW_* and PAINT_SCREEN_* flags in scene.h .
 For example an effect that decides to paint a normal windows as translucent
 will need to modify the mask in its prePaintWindow() to include
 the PAINT_WINDOW_TRANSLUCENT flag. The paintWindow() function will then get
 the mask with this flag turned on and will also paint using transparency.

 The paint pass does the actual painting, based on the information collected
 using the pre-paint pass. After running through the effects' paintScreen()
 either paintGenericScreen() or optimized paintSimpleScreen() are called.
 Those call paintWindow() on windows (not necessarily all), possibly using
 clipping to optimize performance and calling paintWindow() first with only
 PAINT_WINDOW_OPAQUE to paint the opaque parts and then later
 with PAINT_WINDOW_TRANSLUCENT to paint the transparent parts. Function
 paintWindow() again goes through effects' paintWindow() until
 finalPaintWindow() is called, which calls the window's performPaint() to
 do the actual painting.

 The post-paint can be used for cleanups and is also used for scheduling
 repaints during the next painting pass for animations. Effects wanting to
 repaint certain parts can manually damage them during post-paint and repaint
 of these parts will be done during the next paint pass.

*/

#include "scene.h"
#include "abstract_output.h"
#include "internal_client.h"
#include "platform.h"
#include "renderlayer.h"
#include "shadowitem.h"
#include "surfaceitem.h"
#include "unmanaged.h"
#include "waylandclient.h"
#include "windowitem.h"
#include "workspace.h"
#include "x11client.h"

#include <QQuickWindow>
#include <QVector2D>

#include "composite.h"
#include "deleted.h"
#include "effects.h"
#include "renderloop.h"
#include "shadow.h"
#include "wayland_server.h"
#include "x11client.h"
#include <QtMath>

#include <KWaylandServer/surface_interface.h>

namespace KWin
{

SceneDelegate::SceneDelegate(Scene *scene, QObject *parent)
    : RenderLayerDelegate(parent)
    , m_scene(scene)
{
    m_scene->addDelegate(this);
}

SceneDelegate::SceneDelegate(Scene *scene, AbstractOutput *output, QObject *parent)
    : RenderLayerDelegate(parent)
    , m_scene(scene)
    , m_output(output)
{
    m_scene->addDelegate(this);
}

SceneDelegate::~SceneDelegate()
{
    m_scene->removeDelegate(this);
}

QRegion SceneDelegate::repaints() const
{
    return m_scene->damage();
}

SurfaceItem *SceneDelegate::scanoutCandidate() const
{
    return m_scene->scanoutCandidate();
}

void SceneDelegate::prePaint()
{
    m_scene->prePaint(m_output);
}

void SceneDelegate::postPaint()
{
    m_scene->postPaint();
}

void SceneDelegate::paint(const QRegion &region)
{
    m_scene->paint(region);
}

//****************************************
// Scene
//****************************************

Scene::Scene(QObject *parent)
    : QObject(parent)
{
}

Scene::~Scene()
{
    Q_ASSERT(m_windows.isEmpty());
}

void Scene::initialize()
{
    connect(workspace(), &Workspace::deletedRemoved, this, &Scene::removeToplevel);

    connect(workspace(), &Workspace::currentActivityChanged, this, &Scene::addRepaintFull);
    connect(workspace(), &Workspace::currentDesktopChanged, this, &Scene::addRepaintFull);
    connect(workspace(), &Workspace::stackingOrderChanged, this, &Scene::addRepaintFull);

    setGeometry(workspace()->geometry());
    connect(workspace(), &Workspace::geometryChanged, this, [this]() {
        setGeometry(workspace()->geometry());
    });
}

void Scene::addRepaintFull()
{
    addRepaint(geometry());
}

void Scene::addRepaint(int x, int y, int width, int height)
{
    addRepaint(QRegion(x, y, width, height));
}

void Scene::addRepaint(const QRegion &region)
{
    for (const auto &delegate : std::as_const(m_delegates)) {
        const QRegion dirtyRegion = region & delegate->layer()->geometry();
        if (!dirtyRegion.isEmpty()) {
            delegate->layer()->addRepaint(delegate->layer()->mapFromGlobal(dirtyRegion));
        }
    }
}

QRegion Scene::damage() const
{
    return m_paintContext.damage;
}

QRect Scene::geometry() const
{
    return m_geometry;
}

void Scene::setGeometry(const QRect &rect)
{
    if (m_geometry != rect) {
        m_geometry = rect;
        addRepaintFull();
    }
}

QList<SceneDelegate *> Scene::delegates() const
{
    return m_delegates;
}

void Scene::addDelegate(SceneDelegate *delegate)
{
    m_delegates.append(delegate);
}

void Scene::removeDelegate(SceneDelegate *delegate)
{
    m_delegates.removeOne(delegate);
}

static SurfaceItem *findTopMostSurface(SurfaceItem *item)
{
    const QList<Item *> children = item->childItems();
    if (children.isEmpty()) {
        return item;
    } else {
        return findTopMostSurface(static_cast<SurfaceItem *>(children.constLast()));
    }
}

SurfaceItem *Scene::scanoutCandidate() const
{
    if (!waylandServer()) {
        return nullptr;
    }
    SurfaceItem *candidate = nullptr;
    if (!static_cast<EffectsHandlerImpl *>(effects)->blocksDirectScanout()) {
        for (int i = stacking_order.count() - 1; i >= 0; i--) {
            Window *window = stacking_order[i];
            Toplevel *toplevel = window->window();
            if (toplevel->isOnOutput(painted_screen) && window->isVisible() && toplevel->opacity() > 0) {
                AbstractClient *c = dynamic_cast<AbstractClient *>(toplevel);
                if (!c || !c->isFullScreen() || c->opacity() != 1.0) {
                    break;
                }
                if (!window->surfaceItem()) {
                    break;
                }
                SurfaceItem *topMost = findTopMostSurface(window->surfaceItem());
                auto pixmap = topMost->pixmap();
                if (!pixmap) {
                    break;
                }
                pixmap->update();
                // the subsurface has to be able to cover the whole window
                if (topMost->position() != QPoint(0, 0)) {
                    break;
                }
                // and it has to be completely opaque
                if (pixmap->hasAlphaChannel() && !topMost->opaque().contains(QRect(0, 0, window->width(), window->height()))) {
                    break;
                }
                candidate = topMost;
                break;
            }
        }
    }
    return candidate;
}

void Scene::prePaint(AbstractOutput *output)
{
    createStackingOrder();

    if (kwinApp()->operationMode() == Application::OperationModeX11) {
        painted_screen = kwinApp()->platform()->enabledOutputs().constFirst();
        setRenderTargetRect(geometry());
        setRenderTargetScale(1);
    } else {
        painted_screen = output;
        setRenderTargetRect(painted_screen->geometry());
        setRenderTargetScale(painted_screen->scale());
    }

    const RenderLoop *renderLoop = painted_screen->renderLoop();
    const std::chrono::milliseconds presentTime =
        std::chrono::duration_cast<std::chrono::milliseconds>(renderLoop->nextPresentationTimestamp());

    if (Q_UNLIKELY(presentTime < m_expectedPresentTimestamp)) {
        qCDebug(KWIN_CORE,
                "Provided presentation timestamp is invalid: %lld (current: %lld)",
                static_cast<long long>(presentTime.count()),
                static_cast<long long>(m_expectedPresentTimestamp.count()));
    } else {
        m_expectedPresentTimestamp = presentTime;
    }

    // preparation step
    auto effectsImpl = static_cast<EffectsHandlerImpl *>(effects);
    effectsImpl->startPaint();

    ScreenPrePaintData prePaintData;
    prePaintData.mask = 0;
    prePaintData.screen = EffectScreenImpl::get(painted_screen);

    effects->prePaintScreen(prePaintData, m_expectedPresentTimestamp);
    m_paintContext.damage = prePaintData.paint;
    m_paintContext.mask = prePaintData.mask;
    m_paintContext.phase2Data.clear();

    if (m_paintContext.mask & (PAINT_SCREEN_TRANSFORMED | PAINT_SCREEN_WITH_TRANSFORMED_WINDOWS)) {
        preparePaintGenericScreen();
    } else {
        preparePaintSimpleScreen();
    }
}

static void resetRepaintsHelper(Item *item, AbstractOutput *output)
{
    item->resetRepaints(output);

    const auto childItems = item->childItems();
    for (Item *childItem : childItems) {
        resetRepaintsHelper(childItem, output);
    }
}

static void accumulateRepaints(Item *item, AbstractOutput *output, QRegion *repaints)
{
    *repaints += item->repaints(output);
    item->resetRepaints(output);

    const auto childItems = item->childItems();
    for (Item *childItem : childItems) {
        accumulateRepaints(childItem, output, repaints);
    }
}

void Scene::preparePaintGenericScreen()
{
    for (Window *sceneWindow : std::as_const(stacking_order)) {
        resetRepaintsHelper(sceneWindow->windowItem(), painted_screen);

        WindowPrePaintData data;
        data.mask = m_paintContext.mask;
        data.paint = infiniteRegion(); // no clipping, so doesn't really matter

        sceneWindow->resetPaintingEnabled();
        effects->prePaintWindow(effectWindow(sceneWindow), data, m_expectedPresentTimestamp);
        if (sceneWindow->isPaintingEnabled()) {
            m_paintContext.phase2Data.append(Phase2Data{
                .window = sceneWindow,
                .region = infiniteRegion(),
                .opaque = data.opaque,
                .mask = data.mask,
            });
        }
    }

    m_paintContext.damage = QRect(QPoint(0, 0), renderTargetRect().size());
}

void Scene::preparePaintSimpleScreen()
{
    for (Window *sceneWindow : std::as_const(stacking_order)) {
        const Toplevel *toplevel = sceneWindow->window();
        WindowPrePaintData data;
        data.mask = m_paintContext.mask;
        accumulateRepaints(sceneWindow->windowItem(), painted_screen, &data.paint);

        // Clip out the decoration for opaque windows; the decoration is drawn in the second pass.
        if (toplevel->opacity() == 1.0) {
            const SurfaceItem *surfaceItem = sceneWindow->surfaceItem();
            if (Q_LIKELY(surfaceItem)) {
                data.opaque = surfaceItem->mapToGlobal(surfaceItem->opaque());
            }

            const AbstractClient *client = dynamic_cast<const AbstractClient *>(toplevel);
            if (client && !client->decorationHasAlpha()) {
                data.opaque |= sceneWindow->decorationShape().translated(sceneWindow->pos());
            }
        }

        sceneWindow->resetPaintingEnabled();
        effects->prePaintWindow(effectWindow(sceneWindow), data, m_expectedPresentTimestamp);
        if (sceneWindow->isPaintingEnabled()) {
            m_paintContext.phase2Data.append(Phase2Data{
                .window = sceneWindow,
                .region = data.paint,
                .opaque = data.opaque,
                .mask = data.mask,
            });
        }
    }

    // Perform an occlusion cull pass, remove surface damage occluded by opaque windows.
    QRegion surfaceDamage;
    QRegion opaque;
    for (int i = m_paintContext.phase2Data.size() - 1; i >= 0; --i) {
        const auto &paintData = m_paintContext.phase2Data.at(i);
        surfaceDamage += paintData.region - opaque;
        if (!(paintData.mask & (PAINT_WINDOW_TRANSLUCENT | PAINT_WINDOW_TRANSFORMED))) {
            opaque += paintData.opaque;
        }
    }

    m_paintContext.damage += surfaceDamage & renderTargetRect();
    m_paintContext.damage.translate(-renderTargetRect().topLeft());
}

void Scene::postPaint()
{
    for (Window *w : std::as_const(stacking_order)) {
        effects->postPaintWindow(effectWindow(w));
    }

    effects->postPaintScreen();

    if (waylandServer()) {
        const std::chrono::milliseconds frameTime =
            std::chrono::duration_cast<std::chrono::milliseconds>(painted_screen->renderLoop()->lastPresentationTimestamp());

        for (Window *window : std::as_const(m_windows)) {
            Toplevel *toplevel = window->window();
            if (!toplevel->isOnOutput(painted_screen)) {
                continue;
            }
            if (auto surface = toplevel->surface()) {
                surface->frameRendered(frameTime.count());
            }
        }
    }

    clearStackingOrder();
}

static QMatrix4x4 createProjectionMatrix(const QRect &rect)
{
    // Create a perspective projection with a 60° field-of-view,
    // and an aspect ratio of 1.0.
    QMatrix4x4 ret;
    ret.setToIdentity();
    const float fovY = std::tan(qDegreesToRadians(60.0f) / 2);
    const float aspect = 1.0f;
    const float zNear = 0.1f;
    const float zFar = 100.0f;

    const float yMax = zNear * fovY;
    const float yMin = -yMax;
    const float xMin = yMin * aspect;
    const float xMax = yMax * aspect;

    ret.frustum(xMin, xMax, yMin, yMax, zNear, zFar);

    const float scaleFactor = 1.1 * fovY / yMax;
    ret.translate(xMin * scaleFactor, yMax * scaleFactor, -1.1);
    ret.scale((xMax - xMin) * scaleFactor / rect.width(),
              -(yMax - yMin) * scaleFactor / rect.height(),
              0.001);
    ret.translate(-rect.x(), -rect.y());
    return ret;
}

QMatrix4x4 Scene::renderTargetProjectionMatrix() const
{
    return m_renderTargetProjectionMatrix;
}

QRect Scene::renderTargetRect() const
{
    return m_renderTargetRect;
}

void Scene::setRenderTargetRect(const QRect &rect)
{
    m_renderTargetRect = rect;
    m_renderTargetProjectionMatrix = createProjectionMatrix(rect);
}

qreal Scene::renderTargetScale() const
{
    return m_renderTargetScale;
}

void Scene::setRenderTargetScale(qreal scale)
{
    m_renderTargetScale = scale;
}

QRegion Scene::mapToRenderTarget(const QRegion &region) const
{
    QRegion result;
    for (const QRect &rect : region) {
        result += QRect((rect.x() - m_renderTargetRect.x()) * m_renderTargetScale,
                        (rect.y() - m_renderTargetRect.y()) * m_renderTargetScale,
                        rect.width() * m_renderTargetScale,
                        rect.height() * m_renderTargetScale);
    }
    return result;
}

void Scene::paintScreen(const QRegion &region)
{
    ScreenPaintData data(m_renderTargetProjectionMatrix, EffectScreenImpl::get(painted_screen));
    effects->paintScreen(m_paintContext.mask, region, data);

    m_paintScreenCount = 0;
    Q_EMIT frameRendered();
}

// the function that'll be eventually called by paintScreen() above
void Scene::finalPaintScreen(int mask, const QRegion &region, ScreenPaintData &data)
{
    m_paintScreenCount++;
    if (mask & (PAINT_SCREEN_TRANSFORMED | PAINT_SCREEN_WITH_TRANSFORMED_WINDOWS)) {
        paintGenericScreen(mask, data);
    } else {
        paintSimpleScreen(mask, region);
    }
}

// The generic painting code that can handle even transformations.
// It simply paints bottom-to-top.
void Scene::paintGenericScreen(int, const ScreenPaintData &)
{
    if (m_paintContext.mask & PAINT_SCREEN_BACKGROUND_FIRST) {
        if (m_paintScreenCount == 1) {
            paintBackground(infiniteRegion());
        }
    } else {
        paintBackground(infiniteRegion());
    }

    for (const Phase2Data &paintData : std::as_const(m_paintContext.phase2Data)) {
        paintWindow(paintData.window, paintData.mask, paintData.region);
    }
}

// The optimized case without any transformations at all.
// It can paint only the requested region and can use clipping
// to reduce painting and improve performance.
void Scene::paintSimpleScreen(int, const QRegion &region)
{
    // This is the occlusion culling pass
    QRegion visible = region;
    for (int i = m_paintContext.phase2Data.size() - 1; i >= 0; --i) {
        Phase2Data *data = &m_paintContext.phase2Data[i];
        data->region = visible;

        if (!(data->mask & PAINT_WINDOW_TRANSFORMED)) {
            const Item *item = data->window->windowItem();
            data->region &= item->mapToGlobal(item->boundingRect());

            if (!(data->mask & PAINT_WINDOW_TRANSLUCENT)) {
                visible -= data->opaque;
            }
        }
    }

    paintBackground(visible);

    for (const Phase2Data &paintData : std::as_const(m_paintContext.phase2Data)) {
        paintWindow(paintData.window, paintData.mask, paintData.region);
    }
}

void Scene::addToplevel(Toplevel *c)
{
    Q_ASSERT(!m_windows.contains(c));
    Scene::Window *w = createWindow(c);
    m_windows[c] = w;

    connect(c, &Toplevel::windowClosed, this, &Scene::windowClosed);

    c->effectWindow()->setSceneWindow(w);
}

void Scene::removeToplevel(Toplevel *toplevel)
{
    Q_ASSERT(m_windows.contains(toplevel));
    delete m_windows.take(toplevel);
    toplevel->effectWindow()->setSceneWindow(nullptr);
}

void Scene::windowClosed(Toplevel *toplevel, Deleted *deleted)
{
    if (!deleted) {
        removeToplevel(toplevel);
        return;
    }

    Q_ASSERT(m_windows.contains(toplevel));
    Window *window = m_windows.take(toplevel);
    window->updateToplevel(deleted);
    m_windows[deleted] = window;
}

void Scene::createStackingOrder()
{
    // Create a list of all windows in the stacking order
    QList<Toplevel *> windows = Workspace::self()->xStackingOrder();

    // Move elevated windows to the top of the stacking order
    const QList<EffectWindow *> elevatedList = static_cast<EffectsHandlerImpl *>(effects)->elevatedWindows();
    for (EffectWindow *c : elevatedList) {
        Toplevel *t = static_cast<EffectWindowImpl *>(c)->window();
        windows.removeAll(t);
        windows.append(t);
    }

    // Skip windows that are not yet ready for being painted and if screen is locked skip windows
    // that are neither lockscreen nor inputmethod windows.
    //
    // TODO? This cannot be used so carelessly - needs protections against broken clients, the
    // window should not get focus before it's displayed, handle unredirected windows properly and
    // so on.
    for (Toplevel *win : windows) {
        if (!win->readyForPainting()) {
            windows.removeAll(win);
        }
        if (waylandServer() && waylandServer()->isScreenLocked()) {
            if (!win->isLockScreen() && !win->isInputMethod()) {
                windows.removeAll(win);
            }
        }
    }

    // TODO: cache the stacking_order in case it has not changed
    for (Toplevel *c : std::as_const(windows)) {
        Q_ASSERT(m_windows.contains(c));
        stacking_order.append(m_windows[c]);
    }
}

void Scene::clearStackingOrder()
{
    stacking_order.clear();
}

void Scene::paintWindow(Window *w, int mask, const QRegion &region)
{
    if (region.isEmpty()) { // completely clipped
        return;
    }

    WindowPaintData data(w->window()->effectWindow(), screenProjectionMatrix());
    effects->paintWindow(effectWindow(w), mask, region, data);
}

void Scene::paintDesktop(int desktop, int mask, const QRegion &region, ScreenPaintData &data)
{
    static_cast<EffectsHandlerImpl *>(effects)->paintDesktop(desktop, mask, region, data);
}

// the function that'll be eventually called by paintWindow() above
void Scene::finalPaintWindow(EffectWindowImpl *w, int mask, const QRegion &region, WindowPaintData &data)
{
    effects->drawWindow(w, mask, region, data);
}

// will be eventually called from drawWindow()
void Scene::finalDrawWindow(EffectWindowImpl *w, int mask, const QRegion &region, WindowPaintData &data)
{
    if (waylandServer() && waylandServer()->isScreenLocked() && !w->window()->isLockScreen() && !w->window()->isInputMethod()) {
        return;
    }
    w->sceneWindow()->performPaint(mask, region, data);
}

bool Scene::makeOpenGLContextCurrent()
{
    return false;
}

void Scene::doneOpenGLContextCurrent()
{
}

bool Scene::supportsNativeFence() const
{
    return false;
}

QMatrix4x4 Scene::screenProjectionMatrix() const
{
    return QMatrix4x4();
}

QPainter *Scene::scenePainter() const
{
    return nullptr;
}

QImage *Scene::qpainterRenderBuffer(AbstractOutput *output) const
{
    Q_UNUSED(output)
    return nullptr;
}

QVector<QByteArray> Scene::openGLPlatformInterfaceExtensions() const
{
    return QVector<QByteArray>{};
}

SurfaceTexture *Scene::createSurfaceTextureInternal(SurfacePixmapInternal *pixmap)
{
    Q_UNUSED(pixmap)
    return nullptr;
}

SurfaceTexture *Scene::createSurfaceTextureX11(SurfacePixmapX11 *pixmap)
{
    Q_UNUSED(pixmap)
    return nullptr;
}

SurfaceTexture *Scene::createSurfaceTextureWayland(SurfacePixmapWayland *pixmap)
{
    Q_UNUSED(pixmap)
    return nullptr;
}

//****************************************
// Scene::Window
//****************************************

Scene::Window::Window(Toplevel *client, QObject *parent)
    : QObject(parent)
    , toplevel(client)
    , disable_painting(0)
{
    if (qobject_cast<WaylandClient *>(client)) {
        m_windowItem.reset(new WindowItemWayland(toplevel));
    } else if (qobject_cast<X11Client *>(client) || qobject_cast<Unmanaged *>(client)) {
        m_windowItem.reset(new WindowItemX11(toplevel));
    } else if (auto internalClient = qobject_cast<InternalClient *>(client)) {
        m_windowItem.reset(new WindowItemInternal(internalClient));
    } else {
        Q_UNREACHABLE();
    }

    connect(toplevel, &Toplevel::frameGeometryChanged, this, &Window::updateWindowPosition);
    updateWindowPosition();
}

Scene::Window::~Window()
{
}

void Scene::Window::updateToplevel(Deleted *deleted)
{
    toplevel = deleted;
}

void Scene::Window::referencePreviousPixmap()
{
    if (surfaceItem()) {
        referencePreviousPixmap_helper(surfaceItem());
    }
}

void Scene::Window::referencePreviousPixmap_helper(SurfaceItem *item)
{
    item->referencePreviousPixmap();

    const QList<Item *> children = item->childItems();
    for (Item *child : children) {
        referencePreviousPixmap_helper(static_cast<SurfaceItem *>(child));
    }
}

void Scene::Window::unreferencePreviousPixmap()
{
    if (surfaceItem()) {
        unreferencePreviousPixmap_helper(surfaceItem());
    }
}

void Scene::Window::unreferencePreviousPixmap_helper(SurfaceItem *item)
{
    item->unreferencePreviousPixmap();

    const QList<Item *> children = item->childItems();
    for (Item *child : children) {
        unreferencePreviousPixmap_helper(static_cast<SurfaceItem *>(child));
    }
}

QRegion Scene::Window::decorationShape() const
{
    const QRect decorationInnerRect = toplevel->rect() - toplevel->frameMargins();
    return QRegion(toplevel->rect()) - decorationInnerRect;
}

bool Scene::Window::isVisible() const
{
    if (toplevel->isDeleted()) {
        return false;
    }
    if (!toplevel->isOnCurrentDesktop()) {
        return false;
    }
    if (!toplevel->isOnCurrentActivity()) {
        return false;
    }
    if (AbstractClient *c = dynamic_cast<AbstractClient *>(toplevel)) {
        return c->isShown();
    }
    return true; // Unmanaged is always visible
}

bool Scene::Window::isPaintingEnabled() const
{
    return !disable_painting;
}

void Scene::Window::resetPaintingEnabled()
{
    disable_painting = 0;
    if (toplevel->isDeleted()) {
        disable_painting |= PAINT_DISABLED_BY_DELETE;
    }
    if (static_cast<EffectsHandlerImpl *>(effects)->isDesktopRendering()) {
        if (!toplevel->isOnDesktop(static_cast<EffectsHandlerImpl *>(effects)->currentRenderedDesktop())) {
            disable_painting |= PAINT_DISABLED_BY_DESKTOP;
        }
    } else {
        if (!toplevel->isOnCurrentDesktop()) {
            disable_painting |= PAINT_DISABLED_BY_DESKTOP;
        }
    }
    if (!toplevel->isOnCurrentActivity()) {
        disable_painting |= PAINT_DISABLED_BY_ACTIVITY;
    }
    if (AbstractClient *c = dynamic_cast<AbstractClient *>(toplevel)) {
        if (c->isMinimized()) {
            disable_painting |= PAINT_DISABLED_BY_MINIMIZE;
        }
        if (c->isHiddenInternal()) {
            disable_painting |= PAINT_DISABLED;
        }
    }
}

void Scene::Window::enablePainting(int reason)
{
    disable_painting &= ~reason;
}

void Scene::Window::disablePainting(int reason)
{
    disable_painting |= reason;
}

WindowItem *Scene::Window::windowItem() const
{
    return m_windowItem.data();
}

SurfaceItem *Scene::Window::surfaceItem() const
{
    return m_windowItem->surfaceItem();
}

ShadowItem *Scene::Window::shadowItem() const
{
    return m_windowItem->shadowItem();
}

void Scene::Window::updateWindowPosition()
{
    m_windowItem->setPosition(pos());
}

//****************************************
// Scene::EffectFrame
//****************************************
Scene::EffectFrame::EffectFrame(EffectFrameImpl *frame)
    : m_effectFrame(frame)
{
}

Scene::EffectFrame::~EffectFrame()
{
}

} // namespace
