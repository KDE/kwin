/*
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "renderbackend.h"
#include "outputlayer.h"
#include "renderloop_p.h"
#include "scene/surfaceitem.h"

#include <drm_fourcc.h>

namespace KWin
{

OutputFrame::OutputFrame(RenderLoop *loop, OutputLayer *layer)
    : m_loop(loop)
    , m_layer(layer)
{
}

OutputFrame::~OutputFrame() = default;

void OutputFrame::addFeedback(std::unique_ptr<PresentationFeedback> &&feedback)
{
    m_feedbacks.push_back(std::move(feedback));
}

void OutputFrame::presented(std::chrono::nanoseconds refreshDuration, std::chrono::nanoseconds timestamp, PresentationMode mode)
{
    std::chrono::nanoseconds renderTime;
    if (m_layer) {
        renderTime = m_layer->queryRenderTime();
    }
    RenderLoopPrivate::get(m_loop)->notifyFrameCompleted(timestamp, renderTime, mode);
    for (const auto &feedback : m_feedbacks) {
        feedback->presented(refreshDuration, timestamp, mode);
    }
}

void OutputFrame::failed()
{
    RenderLoopPrivate::get(m_loop)->notifyFrameFailed();
}

void OutputFrame::setContentType(ContentType type)
{
    m_contentType = type;
}

std::optional<ContentType> OutputFrame::contentType() const
{
    return m_contentType;
}

void OutputFrame::setPresentationMode(PresentationMode mode)
{
    m_presentationMode = mode;
}

PresentationMode OutputFrame::presentationMode() const
{
    return m_presentationMode;
}

RenderBackend::RenderBackend(QObject *parent)
    : QObject(parent)
{
}

OutputLayer *RenderBackend::cursorLayer(Output *output)
{
    return nullptr;
}

OverlayWindow *RenderBackend::overlayWindow() const
{
    return nullptr;
}

bool RenderBackend::checkGraphicsReset()
{
    return false;
}

GraphicsBufferAllocator *RenderBackend::graphicsBufferAllocator() const
{
    return nullptr;
}

bool RenderBackend::testImportBuffer(GraphicsBuffer *buffer)
{
    return false;
}

QHash<uint32_t, QList<uint64_t>> RenderBackend::supportedFormats() const
{
    return QHash<uint32_t, QList<uint64_t>>{{DRM_FORMAT_XRGB8888, QList<uint64_t>{DRM_FORMAT_MOD_LINEAR}}};
}

std::unique_ptr<SurfaceTexture> RenderBackend::createSurfaceTextureX11(SurfacePixmapX11 *pixmap)
{
    return nullptr;
}

std::unique_ptr<SurfaceTexture> RenderBackend::createSurfaceTextureWayland(SurfacePixmap *pixmap)
{
    return nullptr;
}

} // namespace KWin

#include "moc_renderbackend.cpp"
