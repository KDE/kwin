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
#include "shadowitem.h"
#include "surfaceitem.h"
#include "unmanaged.h"
#include "waylandclient.h"
#include "windowitem.h"
#include "workspace.h"
#include "x11client.h"

#include <QQuickWindow>
#include <QVector2D>

#include "x11client.h"
#include "deleted.h"
#include "effects.h"
#include "renderloop.h"
#include "shadow.h"
#include "wayland_server.h"
#include "composite.h"
#include <QtMath>

namespace KWin
{

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
    connect(kwinApp()->platform(), &Platform::outputDisabled, this, &Scene::removeRepaints);

    connect(workspace(), &Workspace::deletedRemoved, this, &Scene::removeToplevel);

    connect(workspace(), &Workspace::currentActivityChanged, this, &Scene::addRepaintFull);
    connect(workspace(), &Workspace::currentDesktopChanged, this, &Scene::addRepaintFull);
    connect(workspace(), &Workspace::stackingOrderChanged, this, &Scene::addRepaintFull);

    setGeometry(workspace()->geometry());
    connect(workspace(), &Workspace::geometryChanged, this, [this]() {
        setGeometry(workspace()->geometry());
    });

    connect(Cursors::self(), &Cursors::currentCursorChanged, this, &Scene::addCursorRepaints);
    connect(Cursors::self(), &Cursors::positionChanged, this, &Scene::addCursorRepaints);
}

void Scene::addCursorRepaints()
{
    const auto outputs = kwinApp()->platform()->enabledOutputs();
    QRegion repaintRegion = Cursors::self()->currentCursor()->geometry();
    repaintRegion |= m_lastCursorGeometry;
    for (const auto &output : outputs) {
        auto intersection = repaintRegion.intersected(output->geometry());
        if (!intersection.isEmpty() && output->usesSoftwareCursor()) {
            addRepaint(intersection);
        }
    }
    m_lastCursorGeometry = Cursors::self()->currentCursor()->geometry();
}

void Scene::addRepaintFull()
{
    addRepaint(geometry());
}

void Scene::addRepaint(int x, int y, int width, int height)
{
    addRepaint(QRegion(x, y, width, height));
}

void Scene::addRepaint(const QRect &rect)
{
    addRepaint(QRegion(rect));
}

void Scene::addRepaint(const QRegion &region)
{
    if (kwinApp()->platform()->isPerScreenRenderingEnabled()) {
        const QVector<AbstractOutput *> outputs = kwinApp()->platform()->enabledOutputs();
        for (const auto &output : outputs) {
            const QRegion dirtyRegion = region & output->geometry();
            if (!dirtyRegion.isEmpty()) {
                m_repaints[output] += dirtyRegion;
                output->renderLoop()->scheduleRepaint();
            }
        }
    } else {
        m_repaints[0] += region;
        kwinApp()->platform()->renderLoop()->scheduleRepaint();
    }
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

QRegion Scene::repaints(AbstractOutput *output) const
{
    return m_repaints.value(output, infiniteRegion());
}

void Scene::resetRepaints(AbstractOutput *output)
{
    m_repaints.insert(output, QRegion());
}

void Scene::removeRepaints(AbstractOutput *output)
{
    m_repaints.remove(output);
}


QMatrix4x4 Scene::createProjectionMatrix(const QRect &rect)
{
    // Create a perspective projection with a 60° field-of-view,
    // and an aspect ratio of 1.0.
    QMatrix4x4 ret;
    ret.setToIdentity();
    const float fovY   =   std::tan(qDegreesToRadians(60.0f) / 2);
    const float aspect =    1.0f;
    const float zNear  =    0.1f;
    const float zFar   =  100.0f;

    const float yMax   =  zNear * fovY;
    const float yMin   = -yMax;
    const float xMin   =  yMin * aspect;
    const float xMax   =  yMax * aspect;

    ret.frustum(xMin, xMax, yMin, yMax, zNear, zFar);

    const float scaleFactor = 1.1 * fovY / yMax;
    ret.translate(xMin * scaleFactor, yMax * scaleFactor, -1.1);
    ret.scale( (xMax - xMin) * scaleFactor / rect.width(),
                             -(yMax - yMin) * scaleFactor / rect.height(),
                              0.001);
    ret.translate(-rect.x(), -rect.y());
    return ret;
}

QRect Scene::renderTargetRect() const
{
    return m_renderTargetRect;
}

void Scene::setRenderTargetRect(const QRect &rect)
{
    m_renderTargetRect = rect;
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

void Scene::paintScreen(AbstractOutput *output, const QList<Toplevel *> &toplevels)
{
    createStackingOrder(toplevels);
    painted_screen = output;

    setRenderTargetRect(output->geometry());
    setRenderTargetScale(output->scale());

    QRegion update, valid;
    paintScreen(renderTargetRect(), QRect(), &update, &valid, output->renderLoop(), createProjectionMatrix(renderTargetRect()));
    clearStackingOrder();
}

// returns mask and possibly modified region
void Scene::paintScreen(const QRegion &damage, const QRegion &repaint,
                        QRegion *updateRegion, QRegion *validRegion, RenderLoop *renderLoop,
                        const QMatrix4x4 &projection)
{
    const QRegion displayRegion(geometry());

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

    QRegion region = damage;

    auto screen = painted_screen ? EffectScreenImpl::get(painted_screen) : nullptr;
    ScreenPrePaintData pdata;
    pdata.mask = (damage == displayRegion) ? 0 : PAINT_SCREEN_REGION;
    pdata.paint = region;
    pdata.screen = screen;

    effects->prePaintScreen(pdata, m_expectedPresentTimestamp);
    region = pdata.paint;

    int mask = pdata.mask;
    if (mask & (PAINT_SCREEN_TRANSFORMED | PAINT_SCREEN_WITH_TRANSFORMED_WINDOWS)) {
        // Region painting is not possible with transformations,
        // because screen damage doesn't match transformed positions.
        mask &= ~PAINT_SCREEN_REGION;
        region = infiniteRegion();
    } else if (mask & PAINT_SCREEN_REGION) {
        // make sure not to go outside visible screen
        region &= displayRegion;
    } else {
        // whole screen, not transformed, force region to be full
        region = displayRegion;
    }

    painted_region = region;
    repaint_region = repaint;

    ScreenPaintData data(projection, screen);
    effects->paintScreen(mask, region, data);

    Q_EMIT frameRendered();

    for (Window *w : qAsConst(stacking_order)) {
        effects->postPaintWindow(effectWindow(w));
    }

    effects->postPaintScreen();

    // make sure not to go outside of the screen area
    *updateRegion = damaged_region;
    *validRegion = (region | painted_region) & displayRegion;

    repaint_region = QRegion();
    damaged_region = QRegion();

    m_paintScreenCount = 0;
}

// the function that'll be eventually called by paintScreen() above
void Scene::finalPaintScreen(int mask, const QRegion &region, ScreenPaintData& data)
{
    m_paintScreenCount++;
    if (mask & (PAINT_SCREEN_TRANSFORMED | PAINT_SCREEN_WITH_TRANSFORMED_WINDOWS))
        paintGenericScreen(mask, data);
    else
        paintSimpleScreen(mask, region);
}

static void resetRepaintsHelper(Item *item, AbstractOutput *output)
{
    item->resetRepaints(output);

    const auto childItems = item->childItems();
    for (Item *childItem : childItems) {
        resetRepaintsHelper(childItem, output);
    }
}

// The generic painting code that can handle even transformations.
// It simply paints bottom-to-top.
void Scene::paintGenericScreen(int orig_mask, const ScreenPaintData &)
{
    QVector<Phase2Data> phase2;
    phase2.reserve(stacking_order.size());
    for (Window * w : qAsConst(stacking_order)) { // bottom to top
        // Reset the repaint_region.
        // This has to be done here because many effects schedule a repaint for
        // the next frame within Effects::prePaintWindow.
        resetRepaintsHelper(w->windowItem(), painted_screen);

        WindowPrePaintData data;
        data.mask = orig_mask | (w->isOpaque() ? PAINT_WINDOW_OPAQUE : PAINT_WINDOW_TRANSLUCENT);
        w->resetPaintingEnabled();
        data.paint = infiniteRegion(); // no clipping, so doesn't really matter
        data.clip = QRegion();
        // preparation step
        effects->prePaintWindow(effectWindow(w), data, m_expectedPresentTimestamp);
        if (!w->isPaintingEnabled()) {
            continue;
        }
        phase2.append({w, infiniteRegion(), data.clip, data.mask,});
    }

    damaged_region = geometry();
    if (m_paintScreenCount == 1) {
        aboutToStartPainting(painted_screen, damaged_region);

        if (orig_mask & PAINT_SCREEN_BACKGROUND_FIRST) {
            paintBackground(infiniteRegion());
        }
    }

    if (!(orig_mask & PAINT_SCREEN_BACKGROUND_FIRST)) {
        paintBackground(infiniteRegion());
    }
    for (const Phase2Data &d : qAsConst(phase2)) {
        paintWindow(d.window, d.mask, d.region);
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

// The optimized case without any transformations at all.
// It can paint only the requested region and can use clipping
// to reduce painting and improve performance.
void Scene::paintSimpleScreen(int orig_mask, const QRegion &region)
{
    Q_ASSERT((orig_mask & (PAINT_SCREEN_TRANSFORMED
                         | PAINT_SCREEN_WITH_TRANSFORMED_WINDOWS)) == 0);
    QVector<Phase2Data> phase2data;
    phase2data.reserve(stacking_order.size());

    QRegion dirtyArea = region;
    bool opaqueFullscreen = false;

    // Traverse the scene windows from bottom to top.
    for (int i = 0; i < stacking_order.count(); ++i) {
        Window *window = stacking_order[i];
        Toplevel *toplevel = window->window();
        WindowPrePaintData data;
        data.mask = orig_mask | (window->isOpaque() ? PAINT_WINDOW_OPAQUE : PAINT_WINDOW_TRANSLUCENT);
        window->resetPaintingEnabled();
        data.paint = region;
        accumulateRepaints(window->windowItem(), painted_screen, &data.paint);

        // Clip out the decoration for opaque windows; the decoration is drawn in the second pass
        opaqueFullscreen = false; // TODO: do we care about unmanged windows here (maybe input windows?)
        AbstractClient *client = dynamic_cast<AbstractClient *>(toplevel);
        if (window->isOpaque()) {
            if (client) {
                opaqueFullscreen = client->isFullScreen();
            }

            const SurfaceItem *surfaceItem = window->surfaceItem();
            if (surfaceItem) {
                data.clip |= surfaceItem->mapToGlobal(surfaceItem->shape());
            }
        } else if (toplevel->hasAlpha() && toplevel->opacity() == 1.0) {
            const SurfaceItem *surfaceItem = window->surfaceItem();
            if (surfaceItem) {
                const QRegion shape = surfaceItem->shape();
                const QRegion opaque = surfaceItem->opaque();
                data.clip = surfaceItem->mapToGlobal(shape & opaque);

                if (opaque == shape) {
                    data.mask = orig_mask | PAINT_WINDOW_OPAQUE;
                }
            }
        } else {
            data.clip = QRegion();
        }

        if (client && !client->decorationHasAlpha() && toplevel->opacity() == 1.0) {
            data.clip |= window->decorationShape().translated(window->pos());
        }

        // preparation step
        effects->prePaintWindow(effectWindow(window), data, m_expectedPresentTimestamp);
        if (!window->isPaintingEnabled()) {
            continue;
        }
        dirtyArea |= data.paint;
        // Schedule the window for painting
        phase2data.append({ window, data.paint, data.clip, data.mask, });
    }

    // Save the part of the repaint region that's exclusively rendered to
    // bring a reused back buffer up to date. Then union the dirty region
    // with the repaint region.
    const QRegion repaintClip = repaint_region - dirtyArea;
    dirtyArea |= repaint_region;

    const QRegion displayRegion(geometry());
    bool fullRepaint(dirtyArea == displayRegion); // spare some expensive region operations
    if (!fullRepaint) {
        extendPaintRegion(dirtyArea, opaqueFullscreen);
        fullRepaint = (dirtyArea == displayRegion);
    }

    QRegion allclips, upperTranslucentDamage;
    upperTranslucentDamage = repaint_region;

    // This is the occlusion culling pass
    for (int i = phase2data.count() - 1; i >= 0; --i) {
        Phase2Data *data = &phase2data[i];

        if (fullRepaint) {
            data->region = displayRegion;
        } else {
            data->region |= upperTranslucentDamage;
        }

        // subtract the parts which will possibly been drawn as part of
        // a higher opaque window
        data->region -= allclips;

        // Here we rely on WindowPrePaintData::setTranslucent() to remove
        // the clip if needed.
        if (!data->clip.isEmpty() && !(data->mask & PAINT_WINDOW_TRANSLUCENT)) {
            // clip away the opaque regions for all windows below this one
            allclips |= data->clip;
            // extend the translucent damage for windows below this by remaining (translucent) regions
            if (!fullRepaint) {
                upperTranslucentDamage |= data->region - data->clip;
            }
        } else if (!fullRepaint) {
            upperTranslucentDamage |= data->region;
        }
    }

    QRegion paintedArea;
    // Fill any areas of the root window not covered by opaque windows
    if (m_paintScreenCount == 1) {
        aboutToStartPainting(painted_screen, dirtyArea);

        if (orig_mask & PAINT_SCREEN_BACKGROUND_FIRST) {
            paintBackground(infiniteRegion());
        }
    }
    if (!(orig_mask & PAINT_SCREEN_BACKGROUND_FIRST)) {
        paintedArea = dirtyArea - allclips;
        paintBackground(paintedArea);
    }

    // Now walk the list bottom to top and draw the windows.
    for (int i = 0; i < phase2data.count(); ++i) {
        Phase2Data *data = &phase2data[i];

        // add all regions which have been drawn so far
        paintedArea |= data->region;
        data->region = paintedArea;

        paintWindow(data->window, data->mask, data->region);
    }

    if (fullRepaint) {
        painted_region = displayRegion;
        damaged_region = displayRegion - repaintClip;
    } else {
        painted_region |= paintedArea;

        // Clip the repainted region from the damaged region.
        // It's important that we don't add the union of the damaged region
        // and the repainted region to the damage history. Otherwise the
        // repaint region will grow with every frame until it eventually
        // covers the whole back buffer, at which point we're always doing
        // full repaints.
        damaged_region = paintedArea - repaintClip;
    }
}

void Scene::addToplevel(Toplevel *c)
{
    Q_ASSERT(!m_windows.contains(c));
    Scene::Window *w = createWindow(c);
    m_windows[ c ] = w;

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

void Scene::createStackingOrder(const QList<Toplevel *> &toplevels)
{
    // TODO: cache the stacking_order in case it has not changed
    for (Toplevel *c : toplevels) {
        Q_ASSERT(m_windows.contains(c));
        stacking_order.append(m_windows[ c ]);
    }
}

void Scene::clearStackingOrder()
{
    stacking_order.clear();
}

void Scene::paintWindow(Window* w, int mask, const QRegion &_region)
{
    // no painting outside visible screen (and no transformations)
    const QRegion region = _region & geometry();
    if (region.isEmpty())  // completely clipped
        return;

    WindowPaintData data(w->window()->effectWindow(), screenProjectionMatrix());
    effects->paintWindow(effectWindow(w), mask, region, data);
}

void Scene::paintDesktop(int desktop, int mask, const QRegion &region, ScreenPaintData &data)
{
    static_cast<EffectsHandlerImpl*>(effects)->paintDesktop(desktop, mask, region, data);
}

void Scene::aboutToStartPainting(AbstractOutput *output, const QRegion &damage)
{
    Q_UNUSED(output)
    Q_UNUSED(damage)
}

// the function that'll be eventually called by paintWindow() above
void Scene::finalPaintWindow(EffectWindowImpl* w, int mask, const QRegion &region, WindowPaintData& data)
{
    effects->drawWindow(w, mask, region, data);
}

// will be eventually called from drawWindow()
void Scene::finalDrawWindow(EffectWindowImpl* w, int mask, const QRegion &region, WindowPaintData& data)
{
    if (waylandServer() && waylandServer()->isScreenLocked() && !w->window()->isLockScreen() && !w->window()->isInputMethod()) {
        return;
    }
    w->sceneWindow()->performPaint(mask, region, data);
}

void Scene::extendPaintRegion(QRegion &region, bool opaqueFullscreen)
{
    Q_UNUSED(region);
    Q_UNUSED(opaqueFullscreen);
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
    if (toplevel->isDeleted())
        return false;
    if (!toplevel->isOnCurrentDesktop())
        return false;
    if (!toplevel->isOnCurrentActivity())
        return false;
    if (AbstractClient *c = dynamic_cast<AbstractClient*>(toplevel))
        return c->isShown();
    return true; // Unmanaged is always visible
}

bool Scene::Window::isOpaque() const
{
    return toplevel->opacity() == 1.0 && !toplevel->hasAlpha();
}

bool Scene::Window::isPaintingEnabled() const
{
    return !disable_painting;
}

void Scene::Window::resetPaintingEnabled()
{
    disable_painting = 0;
    if (toplevel->isDeleted())
        disable_painting |= PAINT_DISABLED_BY_DELETE;
    if (static_cast<EffectsHandlerImpl*>(effects)->isDesktopRendering()) {
        if (!toplevel->isOnDesktop(static_cast<EffectsHandlerImpl*>(effects)->currentRenderedDesktop())) {
            disable_painting |= PAINT_DISABLED_BY_DESKTOP;
        }
    } else {
        if (!toplevel->isOnCurrentDesktop())
            disable_painting |= PAINT_DISABLED_BY_DESKTOP;
    }
    if (!toplevel->isOnCurrentActivity())
        disable_painting |= PAINT_DISABLED_BY_ACTIVITY;
    if (AbstractClient *c = dynamic_cast<AbstractClient*>(toplevel)) {
        if (c->isMinimized())
            disable_painting |= PAINT_DISABLED_BY_MINIMIZE;
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
Scene::EffectFrame::EffectFrame(EffectFrameImpl* frame)
    : m_effectFrame(frame)
{
}

Scene::EffectFrame::~EffectFrame()
{
}

} // namespace
