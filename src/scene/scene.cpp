/*
    SPDX-FileCopyrightText: 2022 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "scene/scene.h"
#include "core/output.h"
#include "core/outputlayer.h"
#include "scene/item.h"
#include "scene/itemrenderer.h"

namespace KWin
{

OutputLayer *SceneView::layer() const
{
    return m_layer;
}

void SceneView::setLayer(OutputLayer *layer)
{
    m_layer = layer;
}

void SceneView::frame(OutputFrame *frame)
{
}

QRegion SceneView::prePaint()
{
    return QRegion();
}

void SceneView::postPaint()
{
}

QList<SurfaceItem *> SceneView::scanoutCandidates(ssize_t maxCount) const
{
    return {};
}

double SceneView::desiredHdrHeadroom() const
{
    return 1;
}

void SceneView::accumulateRepaints(Item *item, QRegion *repaints)
{
    *repaints += item->takeRepaints(this);

    const auto childItems = item->childItems();
    for (Item *childItem : childItems) {
        accumulateRepaints(childItem, repaints);
    }
}

MainSceneView::MainSceneView(Scene *scene, Output *output)
    : m_scene(scene)
    , m_output(output)
{
    m_scene->addDelegate(this);
}

MainSceneView::~MainSceneView()
{
    m_scene->removeDelegate(this);
}

QList<SurfaceItem *> MainSceneView::scanoutCandidates(ssize_t maxCount) const
{
    return m_scene->scanoutCandidates(maxCount);
}

QRegion MainSceneView::prePaint()
{
    return m_scene->prePaint(this);
}

void MainSceneView::postPaint()
{
    m_scene->postPaint();
}

void MainSceneView::paint(const RenderTarget &renderTarget, const QRegion &region)
{
    m_scene->paint(renderTarget, region == infiniteRegion() ? infiniteRegion() : region.translated(viewport().topLeft()));
}

double MainSceneView::desiredHdrHeadroom() const
{
    return m_scene->desiredHdrHeadroom();
}

void MainSceneView::frame(OutputFrame *frame)
{
    m_scene->frame(this, frame);
}

Output *MainSceneView::output() const
{
    return m_output;
}

qreal MainSceneView::scale() const
{
    return m_output ? m_output->scale() : 1.0;
}

QRect MainSceneView::viewport() const
{
    return m_output->geometry();
}

void MainSceneView::addRepaint(const QRegion &region)
{
    if (!m_layer) {
        return;
    }
    m_layer->addRepaint(region);
}

void MainSceneView::scheduleRepaint(Item *item)
{
    if (!m_layer) {
        return;
    }
    m_layer->scheduleRepaint(item);
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
    for (const auto &delegate : std::as_const(m_delegates)) {
        const QRect viewport = delegate->viewport();
        QRegion dirtyRegion = region & viewport;
        dirtyRegion.translate(-viewport.topLeft());
        if (!dirtyRegion.isEmpty()) {
            delegate->addRepaint(dirtyRegion);
        }
    }
}

void Scene::addRepaint(SceneView *delegate, const QRegion &region)
{
    delegate->addRepaint(region.translated(-delegate->viewport().topLeft()));
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

QList<MainSceneView *> Scene::delegates() const
{
    return m_delegates;
}

void Scene::addDelegate(MainSceneView *delegate)
{
    m_delegates.append(delegate);
}

void Scene::removeDelegate(MainSceneView *delegate)
{
    m_delegates.removeOne(delegate);
    Q_EMIT delegateRemoved(delegate);
}

QList<SurfaceItem *> Scene::scanoutCandidates(ssize_t maxCount) const
{
    return {};
}

void Scene::frame(MainSceneView *delegate, OutputFrame *frame)
{
}

double Scene::desiredHdrHeadroom() const
{
    return 1;
}

} // namespace KWin

#include "moc_scene.cpp"
