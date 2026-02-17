/*
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "outputscreencastsource.h"
#include "filteredsceneview.h"
#include "screencastlayer.h"
#include "screencastutils.h"

#include "compositor.h"
#include "core/output.h"
#include "core/renderbackend.h"
#include "core/renderloop.h"
#include "cursor.h"
#include "opengl/eglbackend.h"
#include "opengl/egldisplay.h"
#include "opengl/glframebuffer.h"
#include "opengl/gltexture.h"
#include "scene/workspacescene.h"
#include "workspace.h"

#include <drm_fourcc.h>

namespace KWin
{

OutputScreenCastSource::OutputScreenCastSource(LogicalOutput *output, std::optional<pid_t> pidToHide)
    : ScreenCastSource()
    , m_output(output)
    , m_pidToHide(pidToHide)
{
    connect(workspace(), &Workspace::outputRemoved, this, [this](LogicalOutput *output) {
        if (m_output == output) {
            Q_EMIT closed();
        }
    });
}

OutputScreenCastSource::~OutputScreenCastSource()
{
    pause();
}

quint32 OutputScreenCastSource::drmFormat() const
{
    return DRM_FORMAT_ARGB8888;
}

QSize OutputScreenCastSource::textureSize() const
{
    return m_output->pixelSize();
}

qreal OutputScreenCastSource::devicePixelRatio() const
{
    return m_output->scale();
}

void OutputScreenCastSource::setRenderCursor(bool enable)
{
    m_renderCursor = enable;
    if (m_cursorView) {
        m_cursorView->setExclusive(!enable);
    }
}

Region OutputScreenCastSource::render(QImage *target, const Region &bufferRepair)
{
    auto texture = GLTexture::allocate(GL_RGBA8, target->size());
    if (!texture) {
        return Region{};
    }
    GLFramebuffer buffer(texture.get());
    const Region ret = render(&buffer, Region::infinite());
    grabTexture(texture.get(), target);
    return ret;
}

Region OutputScreenCastSource::render(GLFramebuffer *target, const Region &bufferRepair)
{
    m_layer->setFramebuffer(target, bufferRepair & Rect(QPoint(), target->size()));
    if (!m_layer->preparePresentationTest()) {
        return Region{};
    }
    const auto beginInfo = m_layer->beginFrame();
    if (!beginInfo) {
        return Region{};
    }
    m_sceneView->prePaint();
    const auto bufferDamage = (m_layer->deviceRepaints() | m_sceneView->collectDamage()) & Rect(QPoint(), target->size());
    const auto repaints = beginInfo->repaint | bufferDamage;
    m_layer->resetRepaints();
    m_sceneView->paint(beginInfo->renderTarget, QPoint(), repaints);
    m_sceneView->postPaint();
    if (!m_layer->endFrame(repaints, bufferDamage, nullptr)) {
        return Region{};
    }
    return bufferDamage;
}

std::chrono::nanoseconds OutputScreenCastSource::clock() const
{
    return m_output->backendOutput()->renderLoop()->lastPresentationTimestamp();
}

uint OutputScreenCastSource::refreshRate() const
{
    return m_output->refreshRate();
}

void OutputScreenCastSource::resume()
{
    if (m_active) {
        return;
    }

    m_layer = std::make_unique<ScreencastLayer>(m_output, static_cast<EglBackend *>(Compositor::self()->backend())->openglContext()->displayObject()->nonExternalOnlySupportedDrmFormats());

    m_sceneView = std::make_unique<FilteredSceneView>(kwinApp()->scene(), m_output, m_layer.get(), m_pidToHide);
    m_sceneView->setViewport(m_output->geometryF());
    m_sceneView->setScale(m_output->scale());
    connect(m_output, &LogicalOutput::changed, m_sceneView.get(), [this]() {
        m_sceneView->setViewport(m_output->geometryF());
        m_sceneView->setScale(m_output->scale());
    });

    m_cursorView = std::make_unique<ItemTreeView>(m_sceneView.get(), kwinApp()->scene()->cursorItem(), m_output, nullptr, nullptr);
    m_cursorView->setExclusive(!m_renderCursor);

    connect(m_layer.get(), &OutputLayer::repaintScheduled, this, &OutputScreenCastSource::frame);
    Q_EMIT frame();

    m_active = true;
}

void OutputScreenCastSource::pause()
{
    if (!m_active) {
        return;
    }

    disconnect(m_layer.get(), &OutputLayer::repaintScheduled, this, &OutputScreenCastSource::frame);

    m_cursorView.reset();
    m_sceneView.reset();
    m_layer.reset();

    m_active = false;
}

bool OutputScreenCastSource::includesCursor(Cursor *cursor) const
{
    return cursor->isOnOutput(m_output);
}

QPointF OutputScreenCastSource::mapFromGlobal(const QPointF &point) const
{
    return m_output->mapFromGlobal(point);
}

RectF OutputScreenCastSource::mapFromGlobal(const RectF &rect) const
{
    return m_output->mapFromGlobal(rect);
}

} // namespace KWin

#include "moc_outputscreencastsource.cpp"
