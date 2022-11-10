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
#include "composite.h"
#include "core/output.h"
#include "core/renderlayer.h"
#include "core/renderloop.h"
#include "deleted.h"
#include "effects.h"
#include "internalwindow.h"
#include "shadow.h"
#include "shadowitem.h"
#include "surfaceitem.h"
#include "unmanaged.h"
#include "wayland/surface_interface.h"
#include "wayland_server.h"
#include "waylandwindow.h"
#include "windowitem.h"
#include "workspace.h"
#include "x11window.h"

#include <QtMath>

namespace KWin
{

SceneDelegate::SceneDelegate(Scene *scene, QObject *parent)
    : RenderLayerDelegate(parent)
    , m_scene(scene)
{
    m_scene->addDelegate(this);
}

SceneDelegate::SceneDelegate(Scene *scene, Output *output, QObject *parent)
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
    return m_scene->damage().translated(-viewport().topLeft());
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

void SceneDelegate::paint(RenderTarget *renderTarget, const QRegion &region)
{
    m_scene->paint(renderTarget, region.translated(viewport().topLeft()));
}

QRect SceneDelegate::viewport() const
{
    return m_output ? m_output->geometry() : m_scene->geometry();
}

//****************************************
// Scene
//****************************************

Scene::Scene() = default;

Scene::~Scene()
{
}

void Scene::initialize()
{
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
        const QRect viewport = delegate->viewport();
        QRegion dirtyRegion = region & viewport;
        dirtyRegion.translate(-viewport.topLeft());
        if (!dirtyRegion.isEmpty()) {
            delegate->layer()->addRepaint(dirtyRegion);
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
            WindowItem *windowItem = stacking_order[i];
            Window *window = windowItem->window();
            if (window->isOnOutput(painted_screen) && window->opacity() > 0) {
                if (!window->isClient() || !window->isFullScreen() || window->opacity() != 1.0) {
                    break;
                }
                if (!windowItem->surfaceItem()) {
                    break;
                }
                SurfaceItem *topMost = findTopMostSurface(windowItem->surfaceItem());
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

void Scene::prePaint(Output *output)
{
    createStackingOrder();

    if (kwinApp()->operationMode() == Application::OperationModeX11) {
        painted_screen = workspace()->outputs().constFirst();
        setRenderTargetRect(geometry());
        setRenderTargetScale(1);
    } else {
        painted_screen = output;
        setRenderTargetRect(painted_screen->fractionalGeometry());
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

    effects->makeOpenGLContextCurrent();
    Q_EMIT preFrameRender();

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

static void resetRepaintsHelper(Item *item, Output *output)
{
    item->resetRepaints(output);

    const auto childItems = item->childItems();
    for (Item *childItem : childItems) {
        resetRepaintsHelper(childItem, output);
    }
}

static void accumulateRepaints(Item *item, Output *output, QRegion *repaints)
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
    for (WindowItem *windowItem : std::as_const(stacking_order)) {
        resetRepaintsHelper(windowItem, painted_screen);

        WindowPrePaintData data;
        data.mask = m_paintContext.mask;
        data.paint = infiniteRegion(); // no clipping, so doesn't really matter

        effects->prePaintWindow(windowItem->window()->effectWindow(), data, m_expectedPresentTimestamp);
        m_paintContext.phase2Data.append(Phase2Data{
            .item = windowItem,
            .region = infiniteRegion(),
            .opaque = data.opaque,
            .mask = data.mask,
        });
    }

    m_paintContext.damage = renderTargetRect();
}

void Scene::preparePaintSimpleScreen()
{
    for (WindowItem *windowItem : std::as_const(stacking_order)) {
        Window *window = windowItem->window();
        WindowPrePaintData data;
        data.mask = m_paintContext.mask;
        accumulateRepaints(windowItem, painted_screen, &data.paint);

        // Clip out the decoration for opaque windows; the decoration is drawn in the second pass.
        if (window->opacity() == 1.0) {
            const SurfaceItem *surfaceItem = windowItem->surfaceItem();
            if (Q_LIKELY(surfaceItem)) {
                data.opaque = surfaceItem->mapToGlobal(surfaceItem->opaque());
            }

            const DecorationItem *decorationItem = windowItem->decorationItem();
            if (decorationItem) {
                data.opaque |= decorationItem->mapToGlobal(decorationItem->opaque());
            }
        }

        effects->prePaintWindow(window->effectWindow(), data, m_expectedPresentTimestamp);
        m_paintContext.phase2Data.append(Phase2Data{
            .item = windowItem,
            .region = data.paint,
            .opaque = data.opaque,
            .mask = data.mask,
        });
    }

    // Perform an occlusion cull pass, remove surface damage occluded by opaque windows.
    QRegion opaque;
    for (int i = m_paintContext.phase2Data.size() - 1; i >= 0; --i) {
        const auto &paintData = m_paintContext.phase2Data.at(i);
        m_paintContext.damage += paintData.region - opaque;
        if (!(paintData.mask & (PAINT_WINDOW_TRANSLUCENT | PAINT_WINDOW_TRANSFORMED))) {
            opaque += paintData.opaque;
        }
    }
}

void Scene::postPaint()
{
    for (WindowItem *w : std::as_const(stacking_order)) {
        effects->postPaintWindow(w->window()->effectWindow());
    }

    effects->postPaintScreen();

    if (waylandServer()) {
        const std::chrono::milliseconds frameTime =
            std::chrono::duration_cast<std::chrono::milliseconds>(painted_screen->renderLoop()->lastPresentationTimestamp());

        for (WindowItem *windowItem : std::as_const(stacking_order)) {
            Window *window = windowItem->window();
            if (!window->isOnOutput(painted_screen)) {
                continue;
            }
            if (auto surface = window->surface()) {
                surface->frameRendered(frameTime.count());
            }
        }
    }

    clearStackingOrder();
}

static QMatrix4x4 createProjectionMatrix(const QRectF &rect, qreal scale)
{
    QMatrix4x4 ret;
    ret.ortho(QRectF(rect.left() * scale, rect.top() * scale, rect.width() * scale, rect.height() * scale));
    return ret;
}

QMatrix4x4 Scene::renderTargetProjectionMatrix() const
{
    return m_renderTargetProjectionMatrix;
}

QRect Scene::renderTargetRect() const
{
    return m_renderTargetRect.toRect();
}

void Scene::setRenderTargetRect(const QRectF &rect)
{
    if (rect == m_renderTargetRect) {
        return;
    }

    m_renderTargetRect = rect;
    m_renderTargetProjectionMatrix = createProjectionMatrix(rect, m_renderTargetScale);
}

qreal Scene::renderTargetScale() const
{
    return m_renderTargetScale;
}

void Scene::setRenderTargetScale(qreal scale)
{
    if (qFuzzyCompare(scale, m_renderTargetScale)) {
        return;
    }

    m_renderTargetScale = scale;
    m_renderTargetProjectionMatrix = createProjectionMatrix(m_renderTargetRect, scale);
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
        paintWindow(paintData.item, paintData.mask, paintData.region);
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
            data->region &= data->item->mapToGlobal(data->item->boundingRect()).toAlignedRect();

            if (!(data->mask & PAINT_WINDOW_TRANSLUCENT)) {
                visible -= data->opaque;
            }
        }
    }

    paintBackground(visible);

    for (const Phase2Data &paintData : std::as_const(m_paintContext.phase2Data)) {
        paintWindow(paintData.item, paintData.mask, paintData.region);
    }
}

void Scene::createStackingOrder()
{
    // Create a list of all windows in the stacking order
    QList<Window *> windows = workspace()->stackingOrder();

    // Move elevated windows to the top of the stacking order
    const QList<EffectWindow *> elevatedList = static_cast<EffectsHandlerImpl *>(effects)->elevatedWindows();
    for (EffectWindow *c : elevatedList) {
        Window *t = static_cast<EffectWindowImpl *>(c)->window();
        windows.removeAll(t);
        windows.append(t);
    }

    // Skip windows that are not yet ready for being painted and if screen is locked skip windows
    // that are neither lockscreen nor inputmethod windows.
    //
    // TODO? This cannot be used so carelessly - needs protections against broken clients, the
    // window should not get focus before it's displayed, handle unredirected windows properly and
    // so on.
    for (Window *window : std::as_const(windows)) {
        if (!window->readyForPainting()) {
            continue;
        }
        if (!window->windowItem()->isVisible()) {
            continue;
        }
        stacking_order.append(window->windowItem());
    }
}

void Scene::clearStackingOrder()
{
    stacking_order.clear();
}

void Scene::paintWindow(WindowItem *item, int mask, const QRegion &region)
{
    if (region.isEmpty()) { // completely clipped
        return;
    }

    WindowPaintData data(renderTargetProjectionMatrix());
    effects->paintWindow(item->window()->effectWindow(), mask, region, data);
}

// the function that'll be eventually called by paintWindow() above
void Scene::finalPaintWindow(EffectWindowImpl *w, int mask, const QRegion &region, WindowPaintData &data)
{
    effects->drawWindow(w, mask, region, data);
}

// will be eventually called from drawWindow()
void Scene::finalDrawWindow(EffectWindowImpl *w, int mask, const QRegion &region, WindowPaintData &data)
{
    render(w->windowItem(), mask, region, data);
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

QPainter *Scene::scenePainter() const
{
    return nullptr;
}

QVector<QByteArray> Scene::openGLPlatformInterfaceExtensions() const
{
    return QVector<QByteArray>{};
}

std::unique_ptr<SurfaceTexture> Scene::createSurfaceTextureInternal(SurfacePixmapInternal *pixmap)
{
    return nullptr;
}

std::unique_ptr<SurfaceTexture> Scene::createSurfaceTextureX11(SurfacePixmapX11 *pixmap)
{
    return nullptr;
}

std::unique_ptr<SurfaceTexture> Scene::createSurfaceTextureWayland(SurfacePixmapWayland *pixmap)
{
    return nullptr;
}

} // namespace
