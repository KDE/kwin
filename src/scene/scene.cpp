/*
    SPDX-FileCopyrightText: 2022 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "scene/scene.h"
#include "core/output.h"
#include "core/renderlayer.h"
#include "scene/itemrenderer.h"

namespace KWin
{

SceneDelegate::SceneDelegate(Scene *scene, Output *output)
    : m_scene(scene)
    , m_output(output)
{
    m_scene->addDelegate(this);
}

SceneDelegate::~SceneDelegate()
{
    m_scene->removeDelegate(this);
}

QList<SurfaceItem *> SceneDelegate::scanoutCandidates(ssize_t maxCount) const
{
    return m_scene->scanoutCandidates(maxCount);
}

QRegion SceneDelegate::prePaint()
{
    return m_scene->prePaint(this);
}

void SceneDelegate::postPaint()
{
    m_scene->postPaint();
}

void SceneDelegate::paint(const RenderTarget &renderTarget, const QRegion &region)
{
    m_scene->paint(renderTarget, region == infiniteRegion() ? infiniteRegion() : region.translated(viewport().topLeft()));
}

double SceneDelegate::desiredHdrHeadroom() const
{
    return m_scene->desiredHdrHeadroom();
}

void SceneDelegate::frame(OutputFrame *frame)
{
    m_scene->frame(this, frame);
}

Output *SceneDelegate::output() const
{
    return m_output;
}

qreal SceneDelegate::scale() const
{
    return m_output ? m_output->scale() : 1.0;
}

QRect SceneDelegate::viewport() const
{
    return m_output ? m_output->geometry() : m_scene->geometry();
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
            delegate->layer()->addRepaint(dirtyRegion);
        }
    }
}

void Scene::addRepaint(SceneDelegate *delegate, const QRegion &region)
{
    delegate->layer()->addRepaint(region.translated(-delegate->viewport().topLeft()));
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
    Q_EMIT delegateRemoved(delegate);
}

QList<SurfaceItem *> Scene::scanoutCandidates(ssize_t maxCount) const
{
    return {};
}

void Scene::frame(SceneDelegate *delegate, OutputFrame *frame)
{
}

double Scene::desiredHdrHeadroom() const
{
    return 1;
}

} // namespace KWin

#include "moc_scene.cpp"
