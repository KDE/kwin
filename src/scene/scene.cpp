/*
    SPDX-FileCopyrightText: 2022 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "scene/scene.h"
#include "core/backendoutput.h"
#include "core/outputlayer.h"
#include "core/pixelgrid.h"
#include "core/renderviewport.h"
#include "effect/effect.h"
#include "scene/cursoritem.h"
#include "scene/item.h"
#include "scene/itemrenderer.h"
#include "scene/surfaceitem.h"

namespace KWin
{

RenderView::RenderView(LogicalOutput *logicalOutput, BackendOutput *backendOutput, OutputLayer *layer)
    : m_logicalOutput(logicalOutput)
    , m_backendOutput(backendOutput)
    , m_layer(layer)
{
}

LogicalOutput *RenderView::logicalOutput() const
{
    return m_logicalOutput;
}

BackendOutput *RenderView::backendOutput() const
{
    return m_backendOutput;
}

OutputLayer *RenderView::layer() const
{
    return m_layer;
}

void RenderView::setLayer(OutputLayer *layer)
{
    m_layer = layer;
}

void RenderView::addDeviceRepaint(const QRegion &deviceRegion)
{
    if (!m_layer) {
        return;
    }
    m_layer->addDeviceRepaint(deviceRegion);
}

void RenderView::scheduleRepaint(Item *item)
{
    if (!m_layer) {
        return;
    }
    m_layer->scheduleRepaint(item);
}

bool RenderView::canSkipMoveRepaint(Item *item)
{
    return false;
}

bool RenderView::shouldRenderItem(Item *item) const
{
    return true;
}

void RenderView::setExclusive(bool enable)
{
}

QPointF RenderView::hotspot() const
{
    return QPointF{};
}

bool RenderView::isVisible() const
{
    return true;
}

bool RenderView::shouldRenderHole(Item *item) const
{
    return false;
}

QRect RenderView::deviceRect() const
{
    return QRect(renderOffset(), deviceSize());
}

QSize RenderView::deviceSize() const
{
    return (viewport().size() * scale()).toSize();
}

QRectF RenderView::mapToDeviceCoordinates(const QRectF &logicalGeometry) const
{
    return scaledRect(logicalGeometry.translated(-viewport().topLeft()), scale()).translated(m_renderOffset);
}

QRect RenderView::mapToDeviceCoordinatesAligned(const QRect &logicalGeometry) const
{
    return mapToDeviceCoordinates(QRectF(logicalGeometry)).toAlignedRect();
}

QRect RenderView::mapToDeviceCoordinatesAligned(const QRectF &logicalGeometry) const
{
    return mapToDeviceCoordinates(logicalGeometry).toAlignedRect();
}

QRect RenderView::mapToDeviceCoordinatesContained(const QRect &logicalGeometry) const
{
    const QRectF ret = scaledRect(QRectF(logicalGeometry).translated(-viewport().topLeft()), scale());
    return QRect(QPoint(std::ceil(ret.x()), std::ceil(ret.y())),
                 QPoint(std::floor(ret.x() + ret.width()) - 1, std::floor(ret.y() + ret.height()) - 1))
        .translated(m_renderOffset);
}

QRegion RenderView::mapToDeviceCoordinatesAligned(const QRegion &logicalGeometry) const
{
    QRegion ret;
    for (const QRect &logicalRect : logicalGeometry) {
        ret |= mapToDeviceCoordinatesAligned(logicalRect);
    }
    return ret;
}

QRegion RenderView::mapToDeviceCoordinatesContained(const QRegion &logicalGeometry) const
{
    QRegion ret;
    for (const QRect &logicalRect : logicalGeometry) {
        ret |= mapToDeviceCoordinatesContained(logicalRect);
    }
    return ret;
}

QRectF RenderView::mapFromDeviceCoordinates(const QRectF &deviceGeometry) const
{
    return scaledRect(deviceGeometry.translated(-m_renderOffset), 1.0 / scale()).translated(viewport().topLeft());
}

QRect RenderView::mapFromDeviceCoordinatesAligned(const QRect &deviceGeometry) const
{
    return scaledRect(deviceGeometry.translated(-m_renderOffset), 1.0 / scale()).translated(viewport().topLeft()).toAlignedRect();
}

QRegion RenderView::mapFromDeviceCoordinatesAligned(const QRegion &deviceGeometry) const
{
    QRegion ret;
    for (const QRect &deviceRect : deviceGeometry) {
        ret |= mapFromDeviceCoordinatesAligned(deviceRect);
    }
    return ret;
}

QPoint RenderView::renderOffset() const
{
    return m_renderOffset;
}

void RenderView::setRenderOffset(const QPoint &offset)
{
    if (m_renderOffset == offset) {
        return;
    }
    addDeviceRepaint(deviceRect());
    m_renderOffset = offset;
    addDeviceRepaint(deviceRect());
}

SceneView::SceneView(Scene *scene, LogicalOutput *logicalOutput, BackendOutput *backendOutput, OutputLayer *layer)
    : RenderView(logicalOutput, backendOutput, layer)
    , m_scene(scene)
{
    m_scene->addView(this);
}

SceneView::~SceneView()
{
    m_scene->removeView(this);
}

QList<SurfaceItem *> SceneView::scanoutCandidates(ssize_t maxCount) const
{
    return m_scene->scanoutCandidates(maxCount);
}

void SceneView::prePaint()
{
    m_scene->prePaint(this);
}

QRegion SceneView::collectDamage()
{
    return m_scene->collectDamage();
}

void SceneView::postPaint()
{
    m_scene->postPaint();
}

void SceneView::paint(const RenderTarget &renderTarget, const QPoint &deviceOffset, const QRegion &deviceRegion)
{
    m_scene->paint(renderTarget, deviceOffset, deviceRegion);
}

double SceneView::desiredHdrHeadroom() const
{
    return m_scene->desiredHdrHeadroom();
}

void SceneView::frame(OutputFrame *frame)
{
    m_scene->frame(this, frame);
}

void SceneView::setViewport(const QRectF &viewport)
{
    if (viewport == m_viewport) {
        return;
    }
    addDeviceRepaint(deviceRect());
    m_viewport = viewport;
    addDeviceRepaint(deviceRect());
}

void SceneView::setScale(qreal scale)
{
    if (scale == m_scale) {
        return;
    }
    addDeviceRepaint(deviceRect());
    m_scale = scale;
    addDeviceRepaint(deviceRect());
}

QRectF SceneView::viewport() const
{
    return m_viewport;
}

qreal SceneView::scale() const
{
    return m_scale;
}

void SceneView::addExclusiveView(RenderView *view)
{
    m_exclusiveViews.push_back(view);
}

void SceneView::removeExclusiveView(RenderView *view)
{
    m_exclusiveViews.removeOne(view);
    m_underlayViews.removeOne(view);
}

void SceneView::addUnderlay(RenderView *view)
{
    m_underlayViews.push_back(view);
}

void SceneView::removeUnderlay(RenderView *view)
{
    m_underlayViews.removeOne(view);
}

bool SceneView::shouldRenderItem(Item *item) const
{
    return std::ranges::none_of(m_exclusiveViews, [item](RenderView *view) {
        return view->shouldRenderItem(item);
    });
}

bool SceneView::shouldRenderHole(Item *item) const
{
    return std::ranges::any_of(m_underlayViews, [item](RenderView *view) {
        return view->shouldRenderItem(item);
    });
}

Scene *SceneView::scene() const
{
    return m_scene;
}

void SceneView::addWindowFilter(std::function<bool(Window *)> filter)
{
    m_windowFilters.push_back(filter);
}

bool SceneView::shouldHideWindow(Window *window) const
{
    return std::ranges::any_of(m_windowFilters, [window](const auto filter) {
        return filter(window);
    });
}

ItemView::ItemView(SceneView *parentView, Item *item, LogicalOutput *logicalOutput, BackendOutput *backendOutput, OutputLayer *layer)
    : RenderView(logicalOutput, backendOutput, layer)
    , m_parentView(parentView)
    , m_item(item)
{
    parentView->scene()->addView(this);
}

ItemView::~ItemView()
{
    m_parentView->scene()->removeView(this);
    if (m_exclusive) {
        m_parentView->removeExclusiveView(this);
        if (m_item) {
            m_item->scheduleSceneRepaint(m_item->rect());
        }
    }
}

qreal ItemView::scale() const
{
    return m_parentView->scale();
}

QPointF ItemView::hotspot() const
{
    if (auto cursor = qobject_cast<CursorItem *>(m_item)) {
        return cursor->hotspot();
    } else {
        return QPointF{};
    }
}

QRectF ItemView::viewport() const
{
    // TODO make the viewport explicit instead?
    if (!m_item) {
        return QRectF();
    }
    return calculateViewport(m_item->rect());
}

QRectF ItemView::calculateViewport(const QRectF &itemRect) const
{
    const QRectF snapped = snapToPixels(itemRect, scale());
    const auto recommendedSizes = m_layer ? m_layer->recommendedSizes() : QList<QSize>{};
    if (!recommendedSizes.empty()) {
        const auto bufferSize = scaledRect(itemRect, scale()).size();
        auto bigEnough = recommendedSizes | std::views::filter([bufferSize](const auto &size) {
            return size.width() >= bufferSize.width() && size.height() >= bufferSize.height();
        });
        const auto it = std::ranges::min_element(bigEnough, [](const auto &left, const auto &right) {
            return left.width() * left.height() < right.width() * right.height();
        });
        if (it != bigEnough.end()) {
            const auto logicalSize = QSizeF(*it) / scale();
            return m_item->mapToView(QRectF(snapped.topLeft(), logicalSize), this);
        }
    }
    return m_item->mapToView(snapped, this);
}

bool ItemView::isVisible() const
{
    return m_item->isVisible();
}

QList<SurfaceItem *> ItemView::scanoutCandidates(ssize_t maxCount) const
{
    if (auto item = dynamic_cast<SurfaceItem *>(m_item.get())) {
        return {item};
    } else {
        return {};
    }
}

void ItemView::frame(OutputFrame *frame)
{
    const auto frameTime = std::chrono::duration_cast<std::chrono::milliseconds>(m_logicalOutput->backendOutput()->renderLoop()->lastPresentationTimestamp());
    m_item->framePainted(this, m_logicalOutput, frame, frameTime);
}

void ItemView::prePaint()
{
}

QRegion ItemView::collectDamage()
{
    return m_item->takeDeviceRepaints(this);
}

void ItemView::postPaint()
{
}

void ItemView::paint(const RenderTarget &renderTarget, const QPoint &deviceOffset, const QRegion &region)
{
    const QRegion globalRegion = region == infiniteRegion() ? infiniteRegion() : region.translated(viewport().topLeft().toPoint());
    RenderViewport renderViewport(viewport(), m_logicalOutput->scale(), renderTarget, deviceOffset);
    auto renderer = m_item->scene()->renderer();
    renderer->beginFrame(renderTarget, renderViewport);
    renderer->renderBackground(renderTarget, renderViewport, globalRegion);
    WindowPaintData data;
    renderer->renderItem(renderTarget, renderViewport, m_item, 0, globalRegion, data, [this](Item *toRender) {
        return toRender != m_item;
    }, {});
    renderer->endFrame();
}

bool ItemView::shouldRenderItem(Item *item) const
{
    return m_item && item == m_item;
}

void ItemView::setExclusive(bool enable)
{
    if (m_exclusive == enable) {
        return;
    }
    m_exclusive = enable;
    if (enable) {
        m_item->scheduleSceneRepaint(m_item->rect());
        // also need to add all the Item's pending repaint regions to the scene,
        // otherwise some required repaints may be missing
        m_parentView->addDeviceRepaint(m_item->takeDeviceRepaints(m_parentView));
        m_parentView->addExclusiveView(this);
        if (m_underlay) {
            m_parentView->addUnderlay(this);
        }
    } else {
        m_parentView->removeExclusiveView(this);
        m_item->scheduleRepaint(m_item->rect());
    }
}

void ItemView::setUnderlay(bool underlay)
{
    if (m_underlay == underlay) {
        return;
    }
    m_underlay = underlay;
    if (!m_exclusive) {
        return;
    }
    if (m_underlay) {
        m_parentView->addUnderlay(this);
    } else {
        m_parentView->removeUnderlay(this);
    }
    m_item->scheduleSceneRepaint(m_item->rect());
}

bool ItemView::needsRepaint()
{
    return m_item->hasRepaints(this);
}

bool ItemView::canSkipMoveRepaint(Item *item)
{
    return m_layer && item == m_item;
}

Item *ItemView::item() const
{
    return m_item;
}

double ItemView::desiredHdrHeadroom() const
{
    const auto &color = m_item->colorDescription();
    const double max = color->maxHdrLuminance().value_or(color->referenceLuminance());
    return max / color->referenceLuminance();
}

ItemTreeView::ItemTreeView(SceneView *parentView, Item *item, LogicalOutput *logicalOutput, BackendOutput *backendOutput, OutputLayer *layer)
    : ItemView(parentView, item, logicalOutput, backendOutput, layer)
{
}

ItemTreeView::~ItemTreeView()
{
    if (m_exclusive && m_item) {
        m_item->scheduleRepaint(m_item->boundingRect());
    }
}

QRectF ItemTreeView::viewport() const
{
    // TODO make the viewport explicit instead?
    if (!m_item) {
        return QRectF();
    }
    return calculateViewport(m_item->boundingRect());
}

QList<SurfaceItem *> ItemTreeView::scanoutCandidates(ssize_t maxCount) const
{
    if (dynamic_cast<SurfaceItem *>(m_item.get())) {
        const bool visibleChildren = std::ranges::any_of(m_item->childItems(), [](Item *child) {
            return child->isVisible();
        });
        if (visibleChildren) {
            return {};
        }
        return {static_cast<SurfaceItem *>(m_item.get())};
    }
    return {};
}

static void accumulateRepaints(Item *item, ItemTreeView *view, QRegion *repaints)
{
    *repaints += item->takeDeviceRepaints(view);

    const auto childItems = item->childItems();
    for (Item *childItem : childItems) {
        accumulateRepaints(childItem, view, repaints);
    }
}

QRegion ItemTreeView::collectDamage()
{
    QRegion ret;
    accumulateRepaints(m_item, this, &ret);
    // FIXME damage tracking for this layer still has some bugs, this effectively disables it
    ret = infiniteRegion();
    return ret;
}

void ItemTreeView::paint(const RenderTarget &renderTarget, const QPoint &deviceOffset, const QRegion &deviceRegion)
{
    RenderViewport renderViewport(viewport(), m_logicalOutput->scale(), renderTarget, deviceOffset);
    auto renderer = m_item->scene()->renderer();
    renderer->beginFrame(renderTarget, renderViewport);
    renderer->renderBackground(renderTarget, renderViewport, deviceRegion);
    WindowPaintData data;
    renderer->renderItem(renderTarget, renderViewport, m_item, 0, deviceRegion, data, {}, {});
    renderer->endFrame();
}

bool ItemTreeView::shouldRenderItem(Item *item) const
{
    return item == m_item || m_item->isAncestorOf(item);
}

static void schedulePendingRepaints(RenderView *view, Item *item)
{
    view->addDeviceRepaint(item->takeDeviceRepaints(view));
    const auto children = item->childItems();
    for (Item *child : children) {
        schedulePendingRepaints(view, child);
    }
}

void ItemTreeView::setExclusive(bool enable)
{
    if (m_exclusive == enable) {
        return;
    }
    m_exclusive = enable;
    if (enable) {
        m_item->scheduleSceneRepaint(m_item->boundingRect());
        // also need to add all the Item's pending repaint regions to the scene,
        // otherwise some required repaints may be missing
        schedulePendingRepaints(m_parentView, m_item);
        m_parentView->addExclusiveView(this);
        if (m_underlay) {
            m_parentView->addUnderlay(this);
        }
    } else {
        m_parentView->removeExclusiveView(this);
        m_item->scheduleRepaint(m_item->boundingRect());
    }
}

static bool recursiveNeedsRepaint(Item *item, RenderView *view)
{
    if (item->hasRepaints(view)) {
        return true;
    }
    const auto children = item->childItems();
    return std::ranges::any_of(children, [view](Item *childItem) {
        return recursiveNeedsRepaint(childItem, view);
    });
}

bool ItemTreeView::needsRepaint()
{
    return recursiveNeedsRepaint(m_item, this);
}

bool ItemTreeView::isVisible() const
{
    // Item::isVisible isn't enough here, we only want to render the view
    // if there's actual contents
    return m_item->hasVisibleContents();
}

bool ItemTreeView::canSkipMoveRepaint(Item *item)
{
    // this could be more generic, but it's all we need for now
    return m_layer && item == m_item;
}

static double recursiveMaxHdrHeadroom(Item *item)
{
    const auto &color = item->colorDescription();
    const double max = color->maxHdrLuminance().value_or(color->referenceLuminance());
    double headroom = max / color->referenceLuminance();
    const auto children = item->childItems();
    for (Item *child : children) {
        headroom = std::max(headroom, recursiveMaxHdrHeadroom(child));
    }
    return headroom;
}

double ItemTreeView::desiredHdrHeadroom() const
{
    return recursiveMaxHdrHeadroom(m_item);
}

Scene::Scene(std::unique_ptr<ItemRenderer> &&renderer)
    : m_renderer(std::move(renderer))
{
}

Scene::~Scene()
{
}

ItemRenderer *Scene::renderer() const
{
    return m_renderer.get();
}

void Scene::addRepaintFull()
{
    for (const auto &view : std::as_const(m_views)) {
        view->addDeviceRepaint(infiniteRegion());
    }
}

void Scene::addLogicalRepaint(int x, int y, int width, int height)
{
    addLogicalRepaint(QRegion(x, y, width, height));
}

void Scene::addLogicalRepaint(const QRegion &logicalRegion)
{
    for (const auto &view : std::as_const(m_views)) {
        addLogicalRepaint(view, logicalRegion);
    }
}

void Scene::addLogicalRepaint(RenderView *view, const QRegion &logicalRegion)
{
    QRegion dirtyRegion = view->mapToDeviceCoordinatesAligned(logicalRegion);
    if (!dirtyRegion.isEmpty()) {
        view->addDeviceRepaint(dirtyRegion);
    }
}

void Scene::addDeviceRepaint(RenderView *view, const QRegion &deviceRegion)
{
    view->addDeviceRepaint(deviceRegion);
}

QRegion Scene::damage() const
{
    return QRegion();
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

QList<RenderView *> Scene::views() const
{
    return m_views;
}

void Scene::addView(RenderView *view)
{
    m_views.append(view);
}

void Scene::removeView(RenderView *view)
{
    m_views.removeOne(view);
    Q_EMIT viewRemoved(view);
}

QList<SurfaceItem *> Scene::scanoutCandidates(ssize_t maxCount) const
{
    return {};
}

void Scene::frame(SceneView *view, OutputFrame *frame)
{
}

double Scene::desiredHdrHeadroom() const
{
    return 1;
}

} // namespace KWin

#include "moc_scene.cpp"
