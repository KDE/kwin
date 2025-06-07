/*
    SPDX-FileCopyrightText: 2022 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "scene/scene.h"
#include "core/output.h"
#include "core/outputlayer.h"
#include "core/renderviewport.h"
#include "effect/effect.h"
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

bool RenderView::shouldRenderItem(Item *item) const
{
    return true;
}

void RenderView::setExclusive(bool enable)
{
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

void SceneView::postPaint()
{
    m_scene->postPaint();
}

void SceneView::paint(const RenderTarget &renderTarget, const QRegion &region)
{
    m_scene->paint(renderTarget, region == infiniteRegion() ? infiniteRegion() : region.translated(viewport().topLeft()));
}

double SceneView::desiredHdrHeadroom() const
{
    return m_scene->desiredHdrHeadroom();
}

void SceneView::frame(OutputFrame *frame)
{
    m_scene->frame(this, frame);
}

QRect SceneView::viewport() const
{
    return m_output ? m_output->geometry() : m_scene->geometry();
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

void ItemTreeView::setViewport(const QRect &viewport)
{
    m_viewport = viewport;
}

QRect ItemTreeView::viewport() const
{
    return m_viewport;
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
    ret = viewport();
    return ret.translated(-viewport().topLeft());
}

void ItemTreeView::paint(const RenderTarget &renderTarget, const QRegion &region)
{
    const QRegion globalRegion = region == infiniteRegion() ? infiniteRegion() : region.translated(viewport().topLeft());
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

Item *ExclusiveItemTreeView::item() const
{
    return m_item;
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
        const QRect viewport = view->viewport();
        QRegion dirtyRegion = region & viewport;
        dirtyRegion.translate(-viewport.topLeft());
        if (!dirtyRegion.isEmpty()) {
            view->addRepaint(dirtyRegion);
        }
    }
}

void Scene::addRepaint(RenderView *view, const QRegion &region)
{
    view->addRepaint(region.translated(-view->viewport().topLeft()));
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
