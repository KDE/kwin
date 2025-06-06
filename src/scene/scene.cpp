/*
    SPDX-FileCopyrightText: 2022 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "scene/scene.h"
#include "core/output.h"
#include "core/outputlayer.h"
#include "core/renderviewport.h"
#include "effect/effect.h"
#include "scene/cursoritem.h"
#include "scene/item.h"
#include "scene/itemrenderer.h"

namespace KWin
{

RenderView::RenderView(Output *output, OutputLayer *layer)
    : m_output(output)
    , m_layer(layer)
{
}

Output *RenderView::output() const
{
    return m_output;
}

qreal RenderView::scale() const
{
    return m_output ? m_output->scale() : 1.0;
}

OutputLayer *RenderView::layer() const
{
    return m_layer;
}

void RenderView::addRepaint(const QRegion &region)
{
    if (!m_layer) {
        return;
    }
    m_layer->addRepaint(region);
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

SceneView::SceneView(Scene *scene, Output *output, OutputLayer *layer)
    : RenderView(output, layer)
    , m_scene(scene)
    , m_output(output)
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

QRegion SceneView::prePaint()
{
    return m_scene->prePaint(this);
}

QRegion SceneView::updatePrePaint()
{
    return m_scene->updatePrePaint();
}

void SceneView::postPaint()
{
    m_scene->postPaint();
}

void SceneView::paint(const RenderTarget &renderTarget, const QRegion &region)
{
    // FIXME damage in logical coordinates may cause issues here
    // if the viewport is on a non-integer position!
    m_scene->paint(renderTarget, region == infiniteRegion() ? infiniteRegion() : region.translated(viewport().topLeft().toPoint()));
}

double SceneView::desiredHdrHeadroom() const
{
    return m_scene->desiredHdrHeadroom();
}

void SceneView::frame(OutputFrame *frame)
{
    m_scene->frame(this, frame);
}

QRectF SceneView::viewport() const
{
    return m_output ? m_output->geometryF() : m_scene->geometry();
}

void SceneView::addExclusiveView(RenderView *view)
{
    m_exclusiveViews.push_back(view);
}

void SceneView::removeExclusiveView(RenderView *view)
{
    m_exclusiveViews.removeOne(view);
}

bool SceneView::shouldRenderItem(Item *item) const
{
    return std::ranges::none_of(m_exclusiveViews, [item](RenderView *view) {
        return view->shouldRenderItem(item);
    });
}

Scene *SceneView::scene() const
{
    return m_scene;
}

ItemTreeView::ItemTreeView(SceneView *parentView, Item *item, Output *output, OutputLayer *layer)
    : RenderView(output, layer)
    , m_parentView(parentView)
    , m_item(item)
{
    parentView->scene()->addView(this);
}

ItemTreeView::~ItemTreeView()
{
    m_parentView->scene()->removeView(this);
    if (m_exclusive) {
        m_parentView->removeExclusiveView(this);
        if (m_item) {
            m_item->scheduleRepaint(m_item->rect());
        }
    }
}

QPointF ItemTreeView::hotspot() const
{
    if (auto cursor = qobject_cast<CursorItem *>(m_item)) {
        return cursor->hotspot();
    } else {
        return QPointF{};
    }
}

QRectF ItemTreeView::viewport() const
{
    const auto recommendedSizes = m_layer ? m_layer->recommendedSizes() : QList<QSize>{};
    if (!recommendedSizes.empty()) {
        const auto bufferSize = scaledRect(m_item->boundingRect(), scale()).size();
        auto bigEnough = recommendedSizes | std::views::filter([bufferSize](const auto &size) {
            return size.width() >= bufferSize.width() && size.height() >= bufferSize.height();
        });
        const auto it = std::ranges::min_element(bigEnough, [](const auto &left, const auto &right) {
            return left.width() * left.height() < right.width() * right.height();
        });
        if (it != bigEnough.end()) {
            const auto logicalSize = QSizeF(*it) / scale();
            return m_item->mapToScene(QRectF(m_item->boundingRect().topLeft(), logicalSize));
        }
    }
    return m_item->mapToScene(m_item->boundingRect());
}

QList<SurfaceItem *> ItemTreeView::scanoutCandidates(ssize_t maxCount) const
{
    // TODO
    return {};
}

void ItemTreeView::frame(OutputFrame *frame)
{
    const auto frameTime = std::chrono::duration_cast<std::chrono::milliseconds>(m_output->renderLoop()->lastPresentationTimestamp());
    m_item->framePainted(nullptr, frame, frameTime);
}

static void accumulateRepaints(Item *item, ItemTreeView *view, QRegion *repaints)
{
    *repaints += item->takeRepaints(view);

    const auto childItems = item->childItems();
    for (Item *childItem : childItems) {
        accumulateRepaints(childItem, view, repaints);
    }
}

QRegion ItemTreeView::prePaint()
{
    QRegion ret;
    accumulateRepaints(m_item, this, &ret);
    // FIXME damage tracking for this layer still has some bugs, this effectively disables it
    ret = viewport().toAlignedRect();
    // FIXME this should really not be rounded!
    return ret.translated(-viewport().topLeft().toPoint());
}

QRegion ItemTreeView::updatePrePaint()
{
    QRegion ret;
    accumulateRepaints(m_item, this, &ret);
    // FIXME this offset should really not be rounded
    return ret.translated(-viewport().topLeft().toPoint());
}

void ItemTreeView::paint(const RenderTarget &renderTarget, const QRegion &region)
{
    const QRegion globalRegion = region == infiniteRegion() ? infiniteRegion() : region.translated(viewport().topLeft().toPoint());
    RenderViewport renderViewport(viewport(), m_output->scale(), renderTarget);
    auto renderer = m_item->scene()->renderer();
    renderer->beginFrame(renderTarget, renderViewport);
    renderer->renderBackground(renderTarget, renderViewport, globalRegion);
    WindowPaintData data;
    renderer->renderItem(renderTarget, renderViewport, m_item, 0, globalRegion, data, {});
    renderer->endFrame();
}

void ItemTreeView::postPaint()
{
}

bool ItemTreeView::shouldRenderItem(Item *item) const
{
    return m_item && (item == m_item || m_item->isAncestorOf(item));
}

void ItemTreeView::setExclusive(bool enable)
{
    if (m_exclusive == enable) {
        return;
    }
    m_exclusive = enable;
    if (enable) {
        m_parentView->addExclusiveView(this);
        m_item->scheduleSceneRepaint(m_item->boundingRect());
    } else {
        m_parentView->removeExclusiveView(this);
        m_item->scheduleRepaint(m_item->rect());
    }
}

Item *ItemTreeView::item() const
{
    return m_item;
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
    return m_item->isVisible();
}

bool ItemTreeView::canSkipMoveRepaint(Item *item)
{
    // this could be more generic, but it's all we need for now
    return m_layer && item == m_item;
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
    addRepaint(geometry());
}

void Scene::addRepaint(int x, int y, int width, int height)
{
    addRepaint(QRegion(x, y, width, height));
}

void Scene::addRepaint(const QRegion &region)
{
    for (const auto &view : std::as_const(m_views)) {
        const QRectF viewport = view->viewport();
        QRegion dirtyRegion = region & viewport.toAlignedRect();
        // FIXME damage in logical coordinates may cause issues here
        // if the viewport is on a non-integer position!
        dirtyRegion.translate(-viewport.topLeft().toPoint());
        if (!dirtyRegion.isEmpty()) {
            view->addRepaint(dirtyRegion);
        }
    }
}

void Scene::addRepaint(RenderView *view, const QRegion &region)
{
    // FIXME damage in logical coordinates may cause issues here
    // if the viewport is on a non-integer position!
    view->addRepaint(region.translated(-view->viewport().topLeft().toPoint()));
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
