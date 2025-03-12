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

SceneView::SceneView(Scene *scene, Output *output, OutputLayer *layer)
    : m_scene(scene)
    , m_output(output)
    , m_layer(layer)
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

Output *SceneView::output() const
{
    return m_output;
}

qreal SceneView::scale() const
{
    return m_output ? m_output->scale() : 1.0;
}

QRect SceneView::viewport() const
{
    return m_output ? m_output->geometry() : m_scene->geometry();
}

void SceneView::hideItem(Item *item)
{
    if (!m_hiddenItems.contains(item)) {
        item->scheduleSceneRepaint(item->rect());
        m_hiddenItems.push_back(item);
    }
}

void SceneView::showItem(Item *item)
{
    if (m_hiddenItems.removeOne(item)) {
        item->scheduleRepaint(item->rect());
    }
}

bool SceneView::shouldRenderItem(Item *item) const
{
    return !m_hiddenItems.contains(item);
}

void SceneView::addRepaint(const QRegion &region)
{
    if (!m_layer) {
        return;
    }
    m_layer->addRepaint(region);
}

void SceneView::scheduleRepaint(Item *item)
{
    if (!m_layer) {
        return;
    }
    m_layer->scheduleRepaint(item);
}

OutputLayer *SceneView::layer() const
{
    return m_layer;
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

QList<SceneView *> Scene::views() const
{
    return m_delegates;
}

void Scene::addView(SceneView *delegate)
{
    m_delegates.append(delegate);
}

void Scene::removeView(SceneView *delegate)
{
    m_delegates.removeOne(delegate);
    Q_EMIT delegateRemoved(delegate);
}

QList<SurfaceItem *> Scene::scanoutCandidates(ssize_t maxCount) const
{
    return {};
}

void Scene::frame(SceneView *delegate, OutputFrame *frame)
{
}

double Scene::desiredHdrHeadroom() const
{
    return 1;
}

} // namespace KWin

#include "moc_scene.cpp"
