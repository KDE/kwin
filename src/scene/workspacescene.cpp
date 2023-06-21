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

#include "scene/workspacescene.h"
#include "compositor.h"
#include "core/output.h"
#include "core/renderbackend.h"
#include "core/renderlayer.h"
#include "core/renderloop.h"
#include "core/renderviewport.h"
#include "effect/effecthandler.h"
#include "internalwindow.h"
#include "scene/decorationitem.h"
#include "scene/dndiconitem.h"
#include "scene/itemrenderer.h"
#include "scene/rootitem.h"
#include "scene/shadowitem.h"
#include "scene/surfaceitem.h"
#include "scene/windowitem.h"
#include "shadow.h"
#include "wayland/seat.h"
#include "wayland/surface.h"
#include "wayland_server.h"
#include "waylandwindow.h"
#include "window.h"
#include "workspace.h"
#if KWIN_BUILD_X11
#include "x11window.h"
#endif

#include <QtMath>

namespace KWin
{

//****************************************
// Scene
//****************************************

WorkspaceScene::WorkspaceScene(std::unique_ptr<ItemRenderer> renderer)
    : Scene(std::move(renderer))
    , m_containerItem(std::make_unique<RootItem>(this))
    , m_overlayItem(std::make_unique<RootItem>(this))
{
    setGeometry(workspace()->geometry());
    connect(workspace(), &Workspace::geometryChanged, this, [this]() {
        setGeometry(workspace()->geometry());
    });

    if (waylandServer()) {
        connect(waylandServer()->seat(), &SeatInterface::dragStarted, this, &WorkspaceScene::createDndIconItem);
        connect(waylandServer()->seat(), &SeatInterface::dragEnded, this, &WorkspaceScene::destroyDndIconItem);
    }
}

WorkspaceScene::~WorkspaceScene()
{
}

void WorkspaceScene::createDndIconItem()
{
    DragAndDropIcon *dragIcon = waylandServer()->seat()->dragIcon();
    if (!dragIcon) {
        return;
    }
    m_dndIcon = std::make_unique<DragAndDropIconItem>(dragIcon, m_overlayItem.get());
    if (waylandServer()->seat()->isDragPointer()) {
        auto updatePosition = [this]() {
            const auto pointerPos = waylandServer()->seat()->pointerPos();
            m_dndIcon->setPosition(pointerPos);
            m_dndIcon->setOutput(workspace()->outputAt(pointerPos));
        };

        updatePosition();
        connect(waylandServer()->seat(), &SeatInterface::pointerPosChanged, m_dndIcon.get(), updatePosition);
    } else if (waylandServer()->seat()->isDragTouch()) {
        auto updatePosition = [this]() {
            auto seat = waylandServer()->seat();
            const auto touchPos = seat->firstTouchPointPosition(seat->dragSurface());
            m_dndIcon->setPosition(touchPos);
            m_dndIcon->setOutput(workspace()->outputAt(touchPos));
        };

        updatePosition();
        connect(waylandServer()->seat(), &SeatInterface::touchMoved, m_dndIcon.get(), updatePosition);
    }
}

void WorkspaceScene::destroyDndIconItem()
{
    m_dndIcon.reset();
}

Item *WorkspaceScene::containerItem() const
{
    return m_containerItem.get();
}

Item *WorkspaceScene::overlayItem() const
{
    return m_overlayItem.get();
}

static bool addCandidates(SurfaceItem *item, QList<SurfaceItem *> &candidates, ssize_t maxCount, QRegion &occluded)
{
    const QList<Item *> children = item->sortedChildItems();
    auto it = children.rbegin();
    for (; it != children.rend(); it++) {
        Item *const child = *it;
        if (child->z() < 0) {
            break;
        }
        if (child->isVisible() && !occluded.contains(child->mapToScene(child->boundingRect()).toAlignedRect())) {
            if (!addCandidates(static_cast<SurfaceItem *>(child), candidates, maxCount, occluded)) {
                return false;
            }
        }
    }
    if (candidates.size() >= maxCount || item->hasEffects()) {
        return false;
    }
    if (occluded.contains(item->mapToScene(item->boundingRect()).toAlignedRect())) {
        return true;
    }
    candidates.push_back(item);
    occluded += item->mapToScene(item->opaque());
    for (; it != children.rend(); it++) {
        Item *const child = *it;
        if (child->isVisible() && !occluded.contains(child->mapToScene(child->boundingRect()).toAlignedRect())) {
            if (!addCandidates(static_cast<SurfaceItem *>(child), candidates, maxCount, occluded)) {
                return false;
            }
        }
    }
    return true;
}

QList<SurfaceItem *> WorkspaceScene::scanoutCandidates(ssize_t maxCount) const
{
    if (!waylandServer()) {
        return {};
    }
    QList<SurfaceItem *> ret;
    if (!effects->blocksDirectScanout()) {
        QRegion occlusion;
        for (int i = stacking_order.count() - 1; i >= 0; i--) {
            WindowItem *windowItem = stacking_order[i];
            Window *window = windowItem->window();
            if (window->isOnOutput(painted_screen) && window->opacity() > 0 && windowItem->isVisible()) {
                if (!window->isClient() || window->opacity() != 1.0 || !window->isFullScreen() || window->windowItem()->hasEffects()) {
                    return {};
                }

                SurfaceItem *surfaceItem = window->surfaceItem();
                if (!surfaceItem || !surfaceItem->isVisible()) {
                    continue;
                }

                if (!addCandidates(surfaceItem, ret, maxCount, occlusion)) {
                    return {};
                }
                if (occlusion.contains(painted_screen->geometry())) {
                    return ret;
                }
            }
        }
    }
    return ret;
}

void WorkspaceScene::frame(SceneDelegate *delegate, OutputFrame *frame)
{
    if (waylandServer()) {
        Output *output = delegate->output();
        const std::chrono::milliseconds frameTime =
            std::chrono::duration_cast<std::chrono::milliseconds>(output->renderLoop()->lastPresentationTimestamp());

        const QList<Item *> items = m_containerItem->sortedChildItems();
        for (Item *item : items) {
            if (!item->isVisible()) {
                continue;
            }
            Window *window = static_cast<WindowItem *>(item)->window();
            if (!window->isOnOutput(output)) {
                continue;
            }
            if (auto surface = window->surface()) {
                surface->traverseTree([&frameTime, &frame, &output](SurfaceInterface *surface) {
                    surface->frameRendered(frameTime.count());
                    if (auto feedback = surface->takePresentationFeedback(output)) {
                        frame->addFeedback(std::move(feedback));
                    }
                });
            }
        }

        if (m_dndIcon) {
            if (auto surface = m_dndIcon->surface()) {
                surface->traverseTree([&frameTime, &frame, &output](SurfaceInterface *surface) {
                    surface->frameRendered(frameTime.count());
                    if (auto feedback = surface->takePresentationFeedback(output)) {
                        frame->addFeedback(std::move(feedback));
                    }
                });
            }
        }
    }
}

QRegion WorkspaceScene::prePaint(SceneDelegate *delegate)
{
    createStackingOrder();

    painted_delegate = delegate;
    if (kwinApp()->operationMode() == Application::OperationModeX11) {
        painted_screen = workspace()->outputs().constFirst();
    } else {
        painted_screen = painted_delegate->output();
    }

    const RenderLoop *renderLoop = painted_screen->renderLoop();
    const std::chrono::milliseconds presentTime =
        std::chrono::duration_cast<std::chrono::milliseconds>(renderLoop->nextPresentationTimestamp());

    if (presentTime > m_expectedPresentTimestamp) {
        m_expectedPresentTimestamp = presentTime;
    }

    // preparation step
    effects->startPaint();

    ScreenPrePaintData prePaintData;
    prePaintData.mask = 0;
    prePaintData.screen = painted_screen;

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

    return m_paintContext.damage.translated(-delegate->viewport().topLeft());
}

static void resetRepaintsHelper(Item *item, SceneDelegate *delegate)
{
    item->resetRepaints(delegate);

    const auto childItems = item->childItems();
    for (Item *childItem : childItems) {
        resetRepaintsHelper(childItem, delegate);
    }
}

static void accumulateRepaints(Item *item, SceneDelegate *delegate, QRegion *repaints)
{
    *repaints += item->repaints(delegate);
    item->resetRepaints(delegate);

    const auto childItems = item->childItems();
    for (Item *childItem : childItems) {
        accumulateRepaints(childItem, delegate, repaints);
    }
}

void WorkspaceScene::preparePaintGenericScreen()
{
    for (WindowItem *windowItem : std::as_const(stacking_order)) {
        resetRepaintsHelper(windowItem, painted_delegate);

        WindowPrePaintData data;
        data.mask = m_paintContext.mask;
        data.paint = infiniteRegion(); // no clipping, so doesn't really matter

        effects->prePaintWindow(windowItem->effectWindow(), data, m_expectedPresentTimestamp);
        m_paintContext.phase2Data.append(Phase2Data{
            .item = windowItem,
            .region = infiniteRegion(),
            .opaque = data.opaque,
            .mask = data.mask,
        });
    }

    resetRepaintsHelper(m_overlayItem.get(), painted_delegate);
    m_paintContext.damage = infiniteRegion();
}

void WorkspaceScene::preparePaintSimpleScreen()
{
    for (WindowItem *windowItem : std::as_const(stacking_order)) {
        Window *window = windowItem->window();
        WindowPrePaintData data;
        data.mask = m_paintContext.mask;
        accumulateRepaints(windowItem, painted_delegate, &data.paint);

        // Clip out the decoration for opaque windows; the decoration is drawn in the second pass.
        if (window->opacity() == 1.0) {
            const SurfaceItem *surfaceItem = windowItem->surfaceItem();
            if (Q_LIKELY(surfaceItem)) {
                data.opaque = surfaceItem->mapToScene(surfaceItem->opaque());
            }

            const DecorationItem *decorationItem = windowItem->decorationItem();
            if (decorationItem) {
                data.opaque += decorationItem->mapToScene(decorationItem->opaque());
            }
        }

        effects->prePaintWindow(windowItem->effectWindow(), data, m_expectedPresentTimestamp);
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

    accumulateRepaints(m_overlayItem.get(), painted_delegate, &m_paintContext.damage);
}

void WorkspaceScene::postPaint()
{
    for (WindowItem *w : std::as_const(stacking_order)) {
        effects->postPaintWindow(w->effectWindow());
    }

    effects->postPaintScreen();

    clearStackingOrder();
}

void WorkspaceScene::paint(const RenderTarget &renderTarget, const QRegion &region)
{
    Output *output = kwinApp()->operationMode() == Application::OperationMode::OperationModeX11 ? nullptr : painted_screen;
    RenderViewport viewport(output ? output->geometryF() : workspace()->geometry(), output ? output->scale() : 1, renderTarget);

    m_renderer->beginFrame(renderTarget, viewport);

    effects->paintScreen(renderTarget, viewport, m_paintContext.mask, region, painted_screen);
    m_paintScreenCount = 0;

    if (m_overlayItem) {
        const QRegion repaint = region & m_overlayItem->mapToScene(m_overlayItem->boundingRect()).toRect();
        if (!repaint.isEmpty()) {
            m_renderer->renderItem(renderTarget, viewport, m_overlayItem.get(), PAINT_SCREEN_TRANSFORMED, repaint, WindowPaintData{});
        }
    }

    Q_EMIT frameRendered();
    m_renderer->endFrame(renderTarget);
}

// the function that'll be eventually called by paintScreen() above
void WorkspaceScene::finalPaintScreen(const RenderTarget &renderTarget, const RenderViewport &viewport, int mask, const QRegion &region, Output *screen)
{
    m_paintScreenCount++;
    if (mask & (PAINT_SCREEN_TRANSFORMED | PAINT_SCREEN_WITH_TRANSFORMED_WINDOWS)) {
        paintGenericScreen(renderTarget, viewport, mask, screen);
    } else {
        paintSimpleScreen(renderTarget, viewport, mask, region);
    }
}

// The generic painting code that can handle even transformations.
// It simply paints bottom-to-top.
void WorkspaceScene::paintGenericScreen(const RenderTarget &renderTarget, const RenderViewport &viewport, int, Output *screen)
{
    if (m_paintContext.mask & PAINT_SCREEN_BACKGROUND_FIRST) {
        if (m_paintScreenCount == 1) {
            m_renderer->renderBackground(renderTarget, viewport, infiniteRegion());
        }
    } else {
        m_renderer->renderBackground(renderTarget, viewport, infiniteRegion());
    }

    for (const Phase2Data &paintData : std::as_const(m_paintContext.phase2Data)) {
        paintWindow(renderTarget, viewport, paintData.item, paintData.mask, paintData.region);
    }
}

// The optimized case without any transformations at all.
// It can paint only the requested region and can use clipping
// to reduce painting and improve performance.
void WorkspaceScene::paintSimpleScreen(const RenderTarget &renderTarget, const RenderViewport &viewport, int, const QRegion &region)
{
    // This is the occlusion culling pass
    QRegion visible = region;
    for (int i = m_paintContext.phase2Data.size() - 1; i >= 0; --i) {
        Phase2Data *data = &m_paintContext.phase2Data[i];
        data->region = visible;

        if (!(data->mask & PAINT_WINDOW_TRANSFORMED)) {
            data->region &= data->item->mapToScene(data->item->boundingRect()).toAlignedRect();

            if (!(data->mask & PAINT_WINDOW_TRANSLUCENT)) {
                visible -= data->opaque;
            }
        }
    }

    m_renderer->renderBackground(renderTarget, viewport, visible);

    for (const Phase2Data &paintData : std::as_const(m_paintContext.phase2Data)) {
        paintWindow(renderTarget, viewport, paintData.item, paintData.mask, paintData.region);
    }
}

void WorkspaceScene::createStackingOrder()
{
    QList<Item *> items = m_containerItem->sortedChildItems();

    // NOTE: this ugly chunk of code is to help us debug a random crash that is extremely difficult
    // to reproduce and that affects many users. Debugging in production builds is absolutely terrible
    // and you should never ever do it, but we are out of options. :(
    {
        const QList<Window *> mapped = workspace()->windows();
        const QList<Window *> closed = workspace()->closed();

        QList<WindowItem *> aliveWindowItems;
        for (Window *window : mapped) {
            if (auto windowItem = window->windowItem()) {
                aliveWindowItems << windowItem;
            }
        }
        for (Window *window : closed) {
            if (auto windowItem = window->windowItem()) {
                aliveWindowItems << windowItem;
            }
        }
        std::sort(aliveWindowItems.begin(), aliveWindowItems.end());

        QList<WindowItem *> sortedWindowItems;
        for (Item *item : std::as_const(items)) {
            sortedWindowItems << static_cast<WindowItem *>(item);
        }
        std::sort(sortedWindowItems.begin(), sortedWindowItems.end());

        QList<WindowItem *> windowItems;
        for (Item *item : m_containerItem->childItems()) {
            windowItems << static_cast<WindowItem *>(item);
        }
        std::sort(windowItems.begin(), windowItems.end());

        if (aliveWindowItems != sortedWindowItems) {
            qFatal("workspaceWindowItems != sortedWindowItems");
        }
        if (sortedWindowItems != windowItems) {
            qFatal("sortedWindowItems != windowItems");
        }
    }

    for (Item *item : std::as_const(items)) {
        WindowItem *windowItem = static_cast<WindowItem *>(item);
        if (windowItem->isVisible()) {
            stacking_order.append(windowItem);
        }
    }
}

void WorkspaceScene::clearStackingOrder()
{
    stacking_order.clear();
}

void WorkspaceScene::paintWindow(const RenderTarget &renderTarget, const RenderViewport &viewport, WindowItem *item, int mask, const QRegion &region)
{
    if (region.isEmpty()) { // completely clipped
        return;
    }

    WindowPaintData data;
    effects->paintWindow(renderTarget, viewport, item->effectWindow(), mask, region, data);
}

// the function that'll be eventually called by paintWindow() above
void WorkspaceScene::finalPaintWindow(const RenderTarget &renderTarget, const RenderViewport &viewport, EffectWindow *w, int mask, const QRegion &region, WindowPaintData &data)
{
    effects->drawWindow(renderTarget, viewport, w, mask, region, data);
}

// will be eventually called from drawWindow()
void WorkspaceScene::finalDrawWindow(const RenderTarget &renderTarget, const RenderViewport &viewport, EffectWindow *w, int mask, const QRegion &region, WindowPaintData &data)
{
    m_renderer->renderItem(renderTarget, viewport, w->windowItem(), mask, region, data);
}

bool WorkspaceScene::makeOpenGLContextCurrent()
{
    return false;
}

void WorkspaceScene::doneOpenGLContextCurrent()
{
}

bool WorkspaceScene::supportsNativeFence() const
{
    return false;
}

OpenGlContext *WorkspaceScene::openglContext() const
{
    return nullptr;
}

} // namespace

#include "moc_workspacescene.cpp"
