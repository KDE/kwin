/*
    SPDX-FileCopyrightText: 2022 Aleix Pol Gonzalez <aleixpol@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "regionscreencastsource.h"
#include "filteredsceneview.h"
#include "screencastlayer.h"
#include "screencastutils.h"

#include "compositor.h"
#include "core/output.h"
#include "cursor.h"
#include "opengl/eglbackend.h"
#include "opengl/glframebuffer.h"
#include "opengl/gltexture.h"
#include "scene/workspacescene.h"
#include "workspace.h"

#include <drm_fourcc.h>

namespace KWin
{

RegionScreenCastSource::RegionScreenCastSource(const QRect &region, qreal scale, std::optional<pid_t> pidToHide)
    : ScreenCastSource()
    , m_region(region)
    , m_scale(scale)
    , m_pidToHide(pidToHide)
{
    Q_ASSERT(m_region.isValid());
    Q_ASSERT(m_scale > 0);

    // TODO once the layer doesn't depend on the output anymore, remove this?
    connect(workspace(), &Workspace::outputsChanged, this, &RegionScreenCastSource::close);
}

RegionScreenCastSource::~RegionScreenCastSource()
{
    pause();
}

QSize RegionScreenCastSource::textureSize() const
{
    return m_region.size() * m_scale;
}

qreal RegionScreenCastSource::devicePixelRatio() const
{
    return m_scale;
}

quint32 RegionScreenCastSource::drmFormat() const
{
    return DRM_FORMAT_ARGB8888;
}

std::chrono::nanoseconds RegionScreenCastSource::clock() const
{
    return m_last;
}

void RegionScreenCastSource::setRenderCursor(bool enable)
{
    m_renderCursor = enable;
    if (m_cursorView) {
        m_cursorView->setExclusive(!enable);
    }
}

QRegion RegionScreenCastSource::render(GLFramebuffer *target, const QRegion &bufferRepair)
{
    m_last = std::chrono::steady_clock::now().time_since_epoch();
    m_layer->setFramebuffer(target, bufferRepair & QRect(QPoint(), target->size()));
    if (!m_layer->preparePresentationTest()) {
        return QRegion{};
    }
    const auto beginInfo = m_layer->beginFrame();
    if (!beginInfo) {
        return QRegion{};
    }
    m_sceneView->prePaint();
    const auto bufferDamage = (m_layer->deviceRepaints() | m_sceneView->collectDamage()) & QRect(QPoint(), target->size());
    const auto repaints = beginInfo->repaint | bufferDamage;
    m_layer->resetRepaints();
    m_sceneView->paint(beginInfo->renderTarget, QPoint(), repaints);
    m_sceneView->postPaint();
    if (!m_layer->endFrame(repaints, bufferDamage, nullptr)) {
        return QRegion{};
    }
    return bufferDamage;
}

QRegion RegionScreenCastSource::render(QImage *target, const QRegion &bufferRepair)
{
    auto texture = GLTexture::allocate(GL_RGBA8, target->size());
    if (!texture) {
        return QRegion{};
    }
    GLFramebuffer buffer(texture.get());
    const QRegion ret = render(&buffer, infiniteRegion());
    grabTexture(texture.get(), target);
    return ret;
}

uint RegionScreenCastSource::refreshRate() const
{
    uint ret = 0;
    const auto allOutputs = workspace()->outputs();
    for (auto output : allOutputs) {
        if (output->geometry().intersects(m_region)) {
            ret = std::max<uint>(ret, output->refreshRate());
        }
    }
    return ret;
}

void RegionScreenCastSource::close()
{
    if (!m_closed) {
        m_closed = true;
        Q_EMIT closed();
    }
}

void RegionScreenCastSource::pause()
{
    if (!m_active) {
        return;
    }

    m_active = false;
    disconnect(m_layer.get(), &OutputLayer::repaintScheduled, this, &RegionScreenCastSource::frame);

    m_cursorView.reset();
    m_sceneView.reset();
    m_layer.reset();
}

void RegionScreenCastSource::resume()
{
    if (m_active) {
        return;
    }

    m_layer = std::make_unique<ScreencastLayer>(workspace()->outputs().front(), static_cast<EglBackend *>(Compositor::self()->backend())->openglContext()->displayObject()->nonExternalOnlySupportedDrmFormats());

    m_sceneView = std::make_unique<FilteredSceneView>(Compositor::self()->scene(), workspace()->outputs().front(), m_layer.get(), m_pidToHide);
    m_sceneView->setViewport(m_region);
    m_sceneView->setScale(m_scale);

    m_cursorView = std::make_unique<ItemTreeView>(m_sceneView.get(), Compositor::self()->scene()->cursorItem(), workspace()->outputs().front(), nullptr, nullptr);
    m_cursorView->setExclusive(!m_renderCursor);

    m_active = true;
    connect(m_layer.get(), &OutputLayer::repaintScheduled, this, &RegionScreenCastSource::frame);
    Q_EMIT frame();
}

bool RegionScreenCastSource::includesCursor(Cursor *cursor) const
{
    if (Cursors::self()->isCursorHidden()) {
        return false;
    }

    return cursor->geometry().intersects(m_region);
}

QPointF RegionScreenCastSource::mapFromGlobal(const QPointF &point) const
{
    return point - m_region.topLeft();
}

QRectF RegionScreenCastSource::mapFromGlobal(const QRectF &rect) const
{
    return rect.translated(-m_region.topLeft());
}

} // namespace KWin

#include "moc_regionscreencastsource.cpp"
