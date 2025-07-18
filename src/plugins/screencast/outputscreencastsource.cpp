/*
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "outputscreencastsource.h"
#include "screencastutils.h"

#include "compositor.h"
#include "core/output.h"
#include "core/renderbackend.h"
#include "core/renderloop.h"
#include "cursor.h"
#include "opengl/eglbackend.h"
#include "opengl/egldisplay.h"
#include "opengl/gltexture.h"
#include "opengl/glutils.h"
#include "scene/workspacescene.h"
#include "workspace.h"

#include <drm_fourcc.h>

namespace KWin
{

ScreencastLayer::ScreencastLayer(Output *output, EglContext *context)
    : OutputLayer(output)
    , m_context(context)
{
}

void ScreencastLayer::setFramebuffer(GLFramebuffer *buffer)
{
    // TODO is there a better way to deal with this?
    m_buffer = buffer;
}

DrmDevice *ScreencastLayer::scanoutDevice() const
{
    return nullptr;
}

QHash<uint32_t, QList<uint64_t>> ScreencastLayer::supportedDrmFormats() const
{
    return m_context->displayObject()->nonExternalOnlySupportedDrmFormats();
}

std::optional<OutputLayerBeginFrameInfo> ScreencastLayer::doBeginFrame()
{
    return OutputLayerBeginFrameInfo{
        .renderTarget = RenderTarget(m_buffer),
        // TODO make damage tracking work with the pipewire buffers?
        .repaint = infiniteRegion(),
    };
}

bool ScreencastLayer::doEndFrame(const QRegion &renderedRegion, const QRegion &damagedRegion, OutputFrame *frame)
{
    return true;
}

OutputScreenCastSource::OutputScreenCastSource(Output *output, QObject *parent)
    : ScreenCastSource(parent)
    , m_output(output)
    , m_layer(std::make_unique<ScreencastLayer>(m_output, static_cast<EglBackend *>(Compositor::self()->backend())->openglContext()))
    , m_sceneView(std::make_unique<SceneView>(Compositor::self()->scene(), output, m_layer.get()))
{
    connect(workspace(), &Workspace::outputRemoved, this, [this](Output *output) {
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

void OutputScreenCastSource::render(QImage *target)
{
    auto texture = GLTexture::allocate(GL_RGBA8, target->size());
    if (!texture) {
        return;
    }
    GLFramebuffer buffer(texture.get());
    render(&buffer);
    grabTexture(texture.get(), target);
}

void OutputScreenCastSource::render(GLFramebuffer *target)
{
    m_layer->setFramebuffer(target);
    if (!m_layer->preparePresentationTest()) {
        return;
    }
    const auto beginInfo = m_layer->beginFrame();
    if (!beginInfo) {
        return;
    }
    const auto damaged = m_layer->repaints() | m_sceneView->prePaint();
    const auto repaints = beginInfo->repaint | damaged;
    m_layer->resetRepaints();
    m_sceneView->paint(beginInfo->renderTarget, repaints);
    m_sceneView->postPaint();
    m_sceneView->frame(nullptr);
    if (!m_layer->endFrame(repaints, damaged, nullptr)) {
        return;
    }
}

std::chrono::nanoseconds OutputScreenCastSource::clock() const
{
    return m_output->renderLoop()->lastPresentationTimestamp();
}

uint OutputScreenCastSource::refreshRate() const
{
    return m_output->refreshRate();
}

void OutputScreenCastSource::report()
{
    if (!m_layer->repaints().isEmpty()) {
        Q_EMIT frame(scaleRegion(m_layer->repaints(), m_output->scale()));
    }
}

void OutputScreenCastSource::resume()
{
    if (m_active) {
        return;
    }

    connect(m_layer.get(), &OutputLayer::repaintScheduled, this, &OutputScreenCastSource::report);
    Q_EMIT frame(QRect(QPoint(), m_output->pixelSize()));

    m_active = true;
}

void OutputScreenCastSource::pause()
{
    if (!m_active) {
        return;
    }

    if (m_output) {
        disconnect(m_output, &Output::outputChange, this, &OutputScreenCastSource::report);
    }

    m_active = false;
}

bool OutputScreenCastSource::includesCursor(Cursor *cursor) const
{
    // FIXME add an item tree view for the cursor and make it work!
    return false;
    // if (Cursors::self()->isCursorHidden()) {
    //     return false;
    // }
    //
    // return cursor->isOnOutput(m_output);
}

QPointF OutputScreenCastSource::mapFromGlobal(const QPointF &point) const
{
    return m_output->mapFromGlobal(point);
}

QRectF OutputScreenCastSource::mapFromGlobal(const QRectF &rect) const
{
    return m_output->mapFromGlobal(rect);
}

} // namespace KWin

#include "moc_outputscreencastsource.cpp"
