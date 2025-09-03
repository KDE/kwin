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
#include "core/backendoutput.h"
#include "core/graphicsbufferview.h"
#include "core/output.h"
#include "core/pixelgrid.h"
#include "core/renderbackend.h"
#include "core/renderloop.h"
#include "core/renderviewport.h"
#include "cursoritem.h"
#include "effect/effecthandler.h"
#include "opengl/eglbackend.h"
#include "opengl/eglcontext.h"
#include "scene/decorationitem.h"
#include "scene/dndiconitem.h"
#include "scene/itemrenderer.h"
#include "scene/rootitem.h"
#include "scene/surfaceitem.h"
#include "scene/windowitem.h"
#include "utils/envvar.h"
#include "wayland/seat.h"
#include "wayland_server.h"
#include "window.h"
#include "workspace.h"

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
    , m_cursorItem(std::make_unique<CursorItem>(m_overlayItem.get()))
{
    setGeometry(workspace()->geometry());
    connect(workspace(), &Workspace::geometryChanged, this, [this]() {
        setGeometry(workspace()->geometry());
    });

    connect(waylandServer()->seat(), &SeatInterface::dragStarted, this, &WorkspaceScene::createDndIconItem);
    connect(waylandServer()->seat(), &SeatInterface::dragEnded, this, &WorkspaceScene::destroyDndIconItem);

    // make sure it's over the dnd icon
    m_cursorItem->setZ(1);
    connect(Cursors::self(), &Cursors::hiddenChanged, this, &WorkspaceScene::updateCursor);
    connect(Cursors::self(), &Cursors::positionChanged, this, &WorkspaceScene::updateCursor);
    updateCursor();
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

    auto updatePosition = [this]() {
        const auto position = waylandServer()->seat()->dragPosition();
        m_dndIcon->setPosition(position);
        m_dndIcon->setOutput(workspace()->outputAt(position));
    };

    updatePosition();
    connect(waylandServer()->seat(), &SeatInterface::dragMoved, m_dndIcon.get(), updatePosition);
}

void WorkspaceScene::destroyDndIconItem()
{
    m_dndIcon.reset();
}

void WorkspaceScene::updateCursor()
{
    if (Cursors::self()->isCursorHidden()) {
        m_cursorItem->setVisible(false);
    } else {
        m_cursorItem->setVisible(true);
        m_cursorItem->setPosition(Cursors::self()->currentCursor()->pos());
    }
}

Item *WorkspaceScene::containerItem() const
{
    return m_containerItem.get();
}

Item *WorkspaceScene::overlayItem() const
{
    return m_overlayItem.get();
}

Item *WorkspaceScene::cursorItem() const
{
    return m_cursorItem.get();
}

struct ClipCorner
{
    RectF box;
    BorderRadius radius;
};

static void maybePushCorners(Item *item, QStack<ClipCorner> &corners)
{
    if (!item->borderRadius().isNull()) {
        corners.push({
            .box = item->rect(),
            .radius = item->borderRadius(),
        });
    } else if (!corners.isEmpty()) {
        const auto &top = corners.top();
        corners.push({
            .box = item->transform().inverted().mapRect(top.box.translated(-item->position())),
            .radius = top.radius,
        });
    }
}

static bool addCandidates(SceneView *delegate, Item *item, QList<Item *> &candidates, ssize_t maxCount, Region &occluded, QStack<ClipCorner> &corners)
{
    if (item->opacity() == 0.0) {
        return true;
    }
    if (item->opacity() != 1.0 || item->hasEffects()) {
        return false;
    }
    const QList<Item *> children = item->sortedChildItems();
    auto it = children.rbegin();
    for (; it != children.rend(); it++) {
        Item *const child = *it;
        if (child->z() < 0) {
            break;
        }
        if (child->isVisible() && !occluded.contains(child->mapToView(child->boundingRect(), delegate).toAlignedRect())) {
            if (!addCandidates(delegate, child, candidates, maxCount, occluded, corners)) {
                return false;
            }
        }
    }
    if (occluded.contains(item->mapToView(item->boundingRect(), delegate).roundedOut())) {
        return true;
    }
    if (!item->rect().isEmpty()) {
        candidates.push_back(item);
        if (candidates.size() > maxCount) {
            return false;
        }
    }

    maybePushCorners(item, corners);
    auto cleanupCorners = qScopeGuard([&corners]() {
        if (!corners.isEmpty()) {
            corners.pop();
        }
    });

    Region opaque = item->opaque();
    if (!corners.isEmpty()) {
        const auto &top = corners.top();
        opaque = top.radius.clip(opaque, top.box);
    }

    occluded += item->mapToView(opaque, delegate);
    for (; it != children.rend(); it++) {
        Item *const child = *it;
        if (child->isVisible() && !occluded.contains(child->mapToView(child->boundingRect(), delegate).toAlignedRect())) {
            if (!addCandidates(delegate, child, candidates, maxCount, occluded, corners)) {
                return false;
            }
        }
    }
    return true;
}

static bool checkForBlackBackground(Item *background)
{
    SurfaceItem *surface = qobject_cast<SurfaceItem *>(background);
    if (!surface) {
        return false;
    }
    if (!surface->buffer()
        || (!surface->buffer()->singlePixelAttributes() && !surface->buffer()->shmAttributes())
        || surface->buffer()->size() != QSize(1, 1)) {
        return false;
    }
    const GraphicsBufferView view(surface->buffer());
    if (!view.image()) {
        return false;
    }
    const QRgb rgb = view.image()->pixel(0, 0);
    const QVector3D encoded(qRed(rgb) / 255.0, qGreen(rgb) / 255.0, qBlue(rgb) / 255.0);
    const QVector3D nits = surface->colorDescription()->mapTo(encoded, ColorDescription(Colorimetry::BT709, TransferFunction(TransferFunction::linear), 100, 0, std::nullopt, std::nullopt), surface->renderingIntent());
    // below 0.1 nits, it shouldn't be noticeable that we replace it with black
    return nits.lengthSquared() <= (0.1 * 0.1);
}

static Rect mapToDevice(SceneView *view, Item *item, const RectF &itemLocal)
{
    const RectF localLogical = item->mapToView(itemLocal, view).translated(-view->viewport().topLeft());
    return localLogical.scaled(view->scale()).rounded();
}

static Region mapToDevice(SceneView *view, Item *item, const Region &itemLocal)
{
    Region ret;
    for (const RectF local : itemLocal.rects()) {
        ret |= mapToDevice(view, item, local);
    }
    return ret;
}

static bool findOverlayCandidates(SceneView *view, Item *item, ssize_t maxTotalCount,
                                  Region &occupied, Region &opaque, Region &effected,
                                  QList<Item *> &overlays, QList<Item *> &underlays,
                                  QStack<ClipCorner> &corners, bool &skippedItems)
{
    if (opaque.contains(view->deviceRect())) {
        return true;
    }
    if (!item || !item->isVisible() || item->opacity() == 0.0 || item->boundingRect().isEmpty() || !view->viewport().intersects(item->mapToView(item->boundingRect(), view))) {
        return true;
    }
    if (item->hasEffects()) {
        // can't put this item, any children on items below this one
        // on an overlay, as we don't know what the effect does
        effected += mapToDevice(view, item, item->boundingRect());
        skippedItems = true;
        return true;
    }
    maybePushCorners(item, corners);
    auto cleanupCorners = qScopeGuard([&corners]() {
        if (!corners.isEmpty()) {
            corners.pop();
        }
    });

    const QList<Item *> children = item->sortedChildItems();
    auto it = children.rbegin();
    for (; it != children.rend(); it++) {
        Item *const child = *it;
        if (child->z() < 0) {
            break;
        }
        if (!findOverlayCandidates(view, child, maxTotalCount, occupied, opaque, effected, overlays, underlays, corners, skippedItems)) {
            return false;
        }
    }

    // for the Item to be possibly relevant for overlays, it needs to
    // - be a SurfaceItem (for now at least)
    // - not be empty
    // - be the topmost item in the relevant screen area
    // - regularly get updates
    // - use dmabufs
    // - not have any surface-wide opacity (for now)
    // - not be entirely covered by other opaque windows
    SurfaceItem *surfaceItem = dynamic_cast<SurfaceItem *>(item);
    const Rect deviceRect = mapToDevice(view, item, item->rect());
    const Region itemDeviceOpaque = mapToDevice(view, item, item->opaque());
    if (surfaceItem
        && !surfaceItem->rect().isEmpty()
        && surfaceItem->frameTimeEstimation().transform([](const auto t) {
        return t < std::chrono::nanoseconds(1'000'000'000) / 20;
    }).value_or(false)
        && surfaceItem->buffer()->dmabufAttributes()
        // TODO make the compositor handle item opacity as well
        && surfaceItem->opacity() == 1.0
        && !opaque.contains(deviceRect)
        && !effected.intersects(deviceRect)) {
        if (occupied.intersects(deviceRect) || (!corners.isEmpty() && corners.top().radius.clips(item->rect(), corners.top().box))) {
            const bool isOpaque = surfaceItem->opaque().contains(surfaceItem->rect().roundedOut());
            if (!isOpaque) {
                // only fully opaque items can be used as underlays
                return false;
            }
            underlays.push_back(surfaceItem);
        } else {
            overlays.push_back(surfaceItem);
        }
        // if any items were skipped, we'll need to include the scene later on,
        // so the max total number of layers is effectively reduced by one
        const ssize_t effectiveMax = skippedItems ? maxTotalCount - 1 : maxTotalCount;
        if (overlays.size() + underlays.size() > effectiveMax) {
            // If we have to repaint the primary plane anyways, it's not going to provide an efficiency
            // or latency improvement to put some but not all quickly updating surfaces on overlays,
            // at least not with the current way we use them.
            return false;
        }
    } else if (!deviceRect.isEmpty()) {
        occupied += deviceRect;
        if (!skippedItems
            && itemDeviceOpaque.contains(view->deviceRect())
            && checkForBlackBackground(item)) {
            opaque = itemDeviceOpaque;
            // completely black background -> this item can be skipped,
            // and we don't need to consider any items below it either
            return true;
        }
        skippedItems = true;
    }
    opaque += itemDeviceOpaque;

    for (; it != children.rend(); it++) {
        Item *const child = *it;
        if (!findOverlayCandidates(view, child, maxTotalCount, occupied, opaque, effected, overlays, underlays, corners, skippedItems)) {
            return false;
        }
    }
    return true;
}

static const bool s_forceSoftwareCursor = environmentVariableBoolValue("KWIN_FORCE_SW_CURSOR").value_or(false);

QList<Item *> WorkspaceScene::layerCandidates(ssize_t maxTotalCount) const
{
    const auto fallback = [this, maxTotalCount]() {
        QList<Item *> ret;
        if (maxTotalCount > 1
            && !s_forceSoftwareCursor
            && cursorItem()->isVisible()
            && painted_delegate->viewport().intersects(cursorItem()->mapToView(cursorItem()->boundingRect(), painted_delegate))) {
            ret.push_back(cursorItem());
        }
        ret.push_back(containerItem());
        return ret;
    };
    Region occupied;
    Region opaque;
    Region effected;
    QList<Item *> overlays;
    QList<Item *> underlays;
    QStack<ClipCorner> cornerStack;
    bool skippedItems = false;
    const auto overlayItems = m_overlayItem->sortedChildItems();
    for (Item *item : overlayItems | std::views::reverse) {
        if (!item->isVisible() || !painted_delegate->viewport().intersects(item->mapToView(item->boundingRect(), painted_delegate))) {
            continue;
        }
        if (item == cursorItem()) {
            if (s_forceSoftwareCursor) {
                continue;
            }
            // for the time being, prioritize the cursor above all else,
            // even while it's not moving
            overlays.push_back(item);
            // if any items were skipped, we'll need to include the scene later on,
            // so the max total number of layers is effectively reduced by one
            const ssize_t effectiveMax = skippedItems ? maxTotalCount - 1 : maxTotalCount;
            if (underlays.size() + overlays.size() > effectiveMax) {
                return fallback();
            }
            continue;
        }
        if (!findOverlayCandidates(painted_delegate, item, maxTotalCount, occupied, opaque, effected, overlays, underlays, cornerStack, skippedItems)) {
            return fallback();
        }
    }
    if (effects->blocksDirectScanout()) {
        return fallback();
    }
    const auto items = m_containerItem->sortedChildItems();
    for (Item *item : items | std::views::reverse) {
        if (!findOverlayCandidates(painted_delegate, item, maxTotalCount, occupied, opaque, effected, overlays, underlays, cornerStack, skippedItems)) {
            return fallback();
        }
    }
    // NOTE the return list needs to be sorted top to bottom
    QList<Item *> ret;
    ret.reserve(underlays.size() + 1 + overlays.size());
    std::ranges::copy(overlays, std::back_inserter(ret));
    if (skippedItems) {
        // some items were skipped -> need to add the scene
        ret.push_back(containerItem());
    }
    std::ranges::copy(underlays, std::back_inserter(ret));
    return ret;
}

static double getDesiredHdrHeadroom(Item *item)
{
    if (!item->isVisible()) {
        return 1;
    }
    double ret = 1;
    const auto children = item->childItems();
    for (const auto &child : children) {
        ret = std::max(ret, getDesiredHdrHeadroom(child));
    }
    const auto &color = item->colorDescription();
    if (color->maxHdrLuminance() && *color->maxHdrLuminance() > color->referenceLuminance()) {
        return std::max(ret, *color->maxHdrLuminance() / color->referenceLuminance());
    } else {
        return ret;
    }
}

double WorkspaceScene::desiredHdrHeadroom() const
{
    double maxHeadroom = 1;
    for (const auto &item : stacking_order) {
        if (!item->window()->frameGeometry().intersects(painted_delegate->viewport())) {
            continue;
        }
        maxHeadroom = std::max(maxHeadroom, getDesiredHdrHeadroom(item));
    }
    return maxHeadroom;
}

void WorkspaceScene::frame(SceneView *delegate, OutputFrame *frame)
{
    LogicalOutput *logicalOutput = delegate->logicalOutput();
    const auto frameTime = std::chrono::duration_cast<std::chrono::milliseconds>(logicalOutput->backendOutput()->renderLoop()->lastPresentationTimestamp());
    m_containerItem->framePainted(delegate, logicalOutput, frame, frameTime);
    if (m_overlayItem) {
        m_overlayItem->framePainted(delegate, logicalOutput, frame, frameTime);
    }
}

void WorkspaceScene::prePaint(SceneView *delegate)
{
    painted_delegate = delegate;
    painted_screen = painted_delegate->logicalOutput();

    createStackingOrder();

    const RenderLoop *renderLoop = painted_screen->backendOutput()->renderLoop();
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
    prePaintData.view = delegate;

    effects->makeOpenGLContextCurrent();
    Q_EMIT preFrameRender();

    effects->prePaintScreen(prePaintData, m_expectedPresentTimestamp);
    m_paintContext.deviceDamage = painted_delegate->mapToDeviceCoordinatesAligned(prePaintData.paint) & painted_delegate->deviceRect();
    m_paintContext.mask = prePaintData.mask;
    m_paintContext.phase2Data.clear();

    if (m_paintContext.mask & (PAINT_SCREEN_TRANSFORMED | PAINT_SCREEN_WITH_TRANSFORMED_WINDOWS)) {
        preparePaintGenericScreen();
    } else {
        preparePaintSimpleScreen();
    }
}

static void resetRepaintsHelper(Item *item, SceneView *delegate)
{
    if (delegate->shouldRenderItem(item)) {
        item->resetRepaints(delegate);
    }

    const auto childItems = item->childItems();
    for (Item *childItem : childItems) {
        resetRepaintsHelper(childItem, delegate);
    }
}

static void accumulateRepaints(Item *item, SceneView *delegate, Region *repaints)
{
    if (delegate->shouldRenderItem(item)) {
        *repaints += item->takeDeviceRepaints(delegate);
    }

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
        data.devicePaint = Region::infinite(); // no clipping, so doesn't really matter

        effects->prePaintWindow(painted_delegate, windowItem->effectWindow(), data, m_expectedPresentTimestamp);
        m_paintContext.phase2Data.append(Phase2Data{
            .item = windowItem,
            .deviceRegion = Region::infinite(),
            .deviceOpaque = data.deviceOpaque,
            .mask = data.mask,
        });
    }
}

void WorkspaceScene::preparePaintSimpleScreen()
{
    for (WindowItem *windowItem : std::as_const(stacking_order)) {
        Window *window = windowItem->window();
        WindowPrePaintData data;
        data.mask = m_paintContext.mask;

        // Clip out the decoration for opaque windows; the decoration is drawn in the second pass.
        if (window->opacity() == 1.0) {
            const SurfaceItem *surfaceItem = windowItem->surfaceItem();
            const auto mapOpaqueToDevice = [this](const Item *item) {
                const Region opaque = item->borderRadius().clip(item->opaque(), item->rect());
                const Rect deviceRect = snapToPixelGrid(painted_delegate->mapToDeviceCoordinates(item->mapToView(item->rect(), painted_delegate)));
                Region ret;
                for (RectF rect : opaque.rects()) {
                    ret |= snapToPixelGrid(painted_delegate->mapToDeviceCoordinates(item->mapToView(rect, painted_delegate))) & deviceRect;
                }
                return ret;
            };
            if (Q_LIKELY(surfaceItem)) {
                data.deviceOpaque = mapOpaqueToDevice(surfaceItem);
            }

            const DecorationItem *decorationItem = windowItem->decorationItem();
            if (decorationItem) {
                data.deviceOpaque += mapOpaqueToDevice(decorationItem);
            }
        }

        effects->prePaintWindow(painted_delegate, windowItem->effectWindow(), data, m_expectedPresentTimestamp);
        m_paintContext.phase2Data.append(Phase2Data{
            .item = windowItem,
            .deviceRegion = data.devicePaint & painted_delegate->deviceRect(),
            .deviceOpaque = data.deviceOpaque,
            .mask = data.mask,
        });
    }
}

Region WorkspaceScene::collectDamage()
{
    if (m_paintContext.mask & (PAINT_SCREEN_TRANSFORMED | PAINT_SCREEN_WITH_TRANSFORMED_WINDOWS)) {
        resetRepaintsHelper(m_overlayItem.get(), painted_delegate);
        m_paintContext.deviceDamage = painted_delegate->deviceRect();
        return m_paintContext.deviceDamage;
    } else {
        // Perform an occlusion cull pass, remove surface damage occluded by opaque windows.
        Region opaque;
        for (int i = m_paintContext.phase2Data.size() - 1; i >= 0; --i) {
            auto &paintData = m_paintContext.phase2Data[i];
            accumulateRepaints(paintData.item, painted_delegate, &paintData.deviceRegion);
            m_paintContext.deviceDamage += paintData.deviceRegion - opaque;
            // TODO change effects API, so occlusion culling is per item, rather than per window
            const bool canCover = painted_delegate->shouldRenderItem(paintData.item->surfaceItem())
                || painted_delegate->shouldRenderHole(paintData.item->surfaceItem());
            if (!(paintData.mask & (PAINT_WINDOW_TRANSLUCENT | PAINT_WINDOW_TRANSFORMED)) && canCover) {
                opaque += paintData.deviceOpaque;
            }
        }

        accumulateRepaints(m_overlayItem.get(), painted_delegate, &m_paintContext.deviceDamage);

        return m_paintContext.deviceDamage & painted_delegate->deviceRect();
    }
}

void WorkspaceScene::postPaint()
{
    effects->postPaintScreen();

    painted_delegate = nullptr;
    painted_screen = nullptr;
    clearStackingOrder();
}

void WorkspaceScene::paint(const RenderTarget &renderTarget, const QPoint &deviceOffset, const Region &deviceRegion)
{
    RenderViewport viewport(painted_delegate->viewport(), painted_delegate->scale(), renderTarget, deviceOffset);

    m_renderer->beginFrame(renderTarget, viewport);

    effects->paintScreen(renderTarget, viewport, m_paintContext.mask, deviceRegion, painted_screen);
    m_paintScreenCount = 0;

    if (m_overlayItem) {
        const Rect bounds = viewport.mapToDeviceCoordinates(m_overlayItem->mapToScene(m_overlayItem->boundingRect())).toRect();
        const Region deviceRepaint = deviceRegion & bounds;
        if (!deviceRepaint.isEmpty()) {
            m_renderer->renderItem(renderTarget, viewport, m_overlayItem.get(), PAINT_SCREEN_TRANSFORMED, deviceRepaint, WindowPaintData{}, [this](Item *item) {
                return !painted_delegate->shouldRenderItem(item);
            }, [this](Item *item) {
                return painted_delegate->shouldRenderHole(item);
            });
        }
    }

    Q_EMIT frameRendered();
    m_renderer->endFrame();
}

// the function that'll be eventually called by paintScreen() above
void WorkspaceScene::finalPaintScreen(const RenderTarget &renderTarget, const RenderViewport &viewport, int mask, const Region &deviceRegion, LogicalOutput *screen)
{
    m_paintScreenCount++;
    if (mask & (PAINT_SCREEN_TRANSFORMED | PAINT_SCREEN_WITH_TRANSFORMED_WINDOWS)) {
        paintGenericScreen(renderTarget, viewport, mask, screen);
    } else {
        paintSimpleScreen(renderTarget, viewport, mask, deviceRegion);
    }
}

// The generic painting code that can handle even transformations.
// It simply paints bottom-to-top.
void WorkspaceScene::paintGenericScreen(const RenderTarget &renderTarget, const RenderViewport &viewport, int, LogicalOutput *screen)
{
    if (m_paintContext.mask & PAINT_SCREEN_BACKGROUND_FIRST) {
        if (m_paintScreenCount == 1) {
            m_renderer->renderBackground(renderTarget, viewport, Region::infinite());
        }
    } else {
        m_renderer->renderBackground(renderTarget, viewport, Region::infinite());
    }

    for (const Phase2Data &paintData : std::as_const(m_paintContext.phase2Data)) {
        paintWindow(renderTarget, viewport, paintData.item, paintData.mask, paintData.deviceRegion);
    }
}

// The optimized case without any transformations at all.
// It can paint only the requested region and can use clipping
// to reduce painting and improve performance.
void WorkspaceScene::paintSimpleScreen(const RenderTarget &renderTarget, const RenderViewport &viewport, int, const Region &deviceRegion)
{
    // This is the occlusion culling pass
    Region visible = deviceRegion;
    for (int i = m_paintContext.phase2Data.size() - 1; i >= 0; --i) {
        Phase2Data *data = &m_paintContext.phase2Data[i];
        data->deviceRegion = visible & viewport.deviceRect();

        if (!(data->mask & PAINT_WINDOW_TRANSFORMED)) {
            data->deviceRegion &= viewport.mapToDeviceCoordinatesAligned(data->item->mapToScene(data->item->boundingRect()));

            // TODO change effects API, so occlusion culling is per item, rather than per window
            const bool canCover = painted_delegate->shouldRenderItem(data->item->surfaceItem())
                || painted_delegate->shouldRenderHole(data->item->surfaceItem());
            if (!(data->mask & PAINT_WINDOW_TRANSLUCENT) && canCover) {
                visible -= data->deviceOpaque;
            }
        }
    }

    m_renderer->renderBackground(renderTarget, viewport, visible);

    for (const Phase2Data &paintData : std::as_const(m_paintContext.phase2Data)) {
        paintWindow(renderTarget, viewport, paintData.item, paintData.mask, paintData.deviceRegion);
    }
}

void WorkspaceScene::createStackingOrder()
{
    QList<Item *> items = m_containerItem->sortedChildItems();
    for (Item *item : std::as_const(items)) {
        WindowItem *windowItem = static_cast<WindowItem *>(item);
        if (painted_delegate && painted_delegate->shouldHideWindow(windowItem->window())) {
            continue;
        }
        if (windowItem->isVisible()) {
            stacking_order.append(windowItem);
        }
    }
}

void WorkspaceScene::clearStackingOrder()
{
    stacking_order.clear();
}

void WorkspaceScene::paintWindow(const RenderTarget &renderTarget, const RenderViewport &viewport, WindowItem *item, int mask, const Region &deviceRegion)
{
    if (deviceRegion.isEmpty()) { // completely clipped
        return;
    }

    WindowPaintData data;
    effects->paintWindow(renderTarget, viewport, item->effectWindow(), mask, deviceRegion, data);
}

// the function that'll be eventually called by paintWindow() above
void WorkspaceScene::finalPaintWindow(const RenderTarget &renderTarget, const RenderViewport &viewport, EffectWindow *w, int mask, const Region &deviceRegion, WindowPaintData &data)
{
    effects->drawWindow(renderTarget, viewport, w, mask, deviceRegion, data);
}

// will be eventually called from drawWindow()
void WorkspaceScene::finalDrawWindow(const RenderTarget &renderTarget, const RenderViewport &viewport, EffectWindow *w, int mask, const Region &deviceRegion, WindowPaintData &data)
{
    // TODO: Reconsider how the CrossFadeEffect captures the initial window contents to remove
    // null pointer delegate checks in "should render item" and "should render hole" checks.
    m_renderer->renderItem(renderTarget, viewport, w->windowItem(), mask, deviceRegion, data, [this](Item *item) {
        return painted_delegate && !painted_delegate->shouldRenderItem(item);
    }, [this](Item *item) {
        return painted_delegate && painted_delegate->shouldRenderHole(item);
    });
}

EglContext *WorkspaceScene::openglContext() const
{
    if (auto eglBackend = qobject_cast<EglBackend *>(Compositor::self()->backend())) {
        return eglBackend->openglContext();
    }
    return nullptr;
}

bool WorkspaceScene::animationsSupported() const
{
    const auto context = openglContext();
    return context && !context->isSoftwareRenderer();
}

} // namespace

#include "moc_workspacescene.cpp"
