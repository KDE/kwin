/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2022 Xaver Hugl <xaver.hugl@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "drm_qpainter_layer.h"
#include "core/graphicsbufferview.h"
#include "drm_buffer.h"
#include "drm_gpu.h"
#include "drm_logging.h"
#include "drm_output.h"
#include "drm_pipeline.h"
#include "drm_virtual_output.h"
#include "qpainter/qpainterswapchain.h"

#include <cerrno>
#include <drm_fourcc.h>

namespace KWin
{

DrmQPainterLayer::DrmQPainterLayer(DrmPipeline *pipeline, DrmPlane::TypeIndex type)
    : DrmPipelineLayer(pipeline, type)
{
}

std::optional<OutputLayerBeginFrameInfo> DrmQPainterLayer::doBeginFrame()
{
    if (!doesSwapchainFit()) {
        m_swapchain = std::make_shared<QPainterSwapchain>(m_pipeline->gpu()->drmDevice()->allocator(), targetRect().size(), m_pipeline->formats(m_type).contains(DRM_FORMAT_ARGB8888) ? DRM_FORMAT_ARGB8888 : DRM_FORMAT_XRGB8888);
        m_damageJournal = DamageJournal();
    }

    m_currentBuffer = m_swapchain->acquire();
    if (!m_currentBuffer) {
        return std::nullopt;
    }

    m_renderTime = std::make_unique<CpuRenderTimeQuery>();
    const QRegion repaint = m_damageJournal.accumulate(m_currentBuffer->age(), infiniteRegion());
    return OutputLayerBeginFrameInfo{
        .renderTarget = RenderTarget(m_currentBuffer->view()->image()),
        .repaint = repaint,
    };
}

bool DrmQPainterLayer::doEndFrame(const QRegion &renderedRegion, const QRegion &damagedRegion, OutputFrame *frame)
{
    m_renderTime->end();
    if (frame) {
        frame->addRenderTimeQuery(std::move(m_renderTime));
    }
    m_currentFramebuffer = m_pipeline->gpu()->importBuffer(m_currentBuffer->buffer(), FileDescriptor{});
    m_damageJournal.add(damagedRegion);
    m_swapchain->release(m_currentBuffer);
    if (!m_currentFramebuffer) {
        qCWarning(KWIN_DRM, "Failed to create dumb framebuffer: %s", strerror(errno));
    }
    return m_currentFramebuffer != nullptr;
}

bool DrmQPainterLayer::preparePresentationTest()
{
    if (!doesSwapchainFit()) {
        m_swapchain = std::make_shared<QPainterSwapchain>(m_pipeline->gpu()->drmDevice()->allocator(), targetRect().size(), m_pipeline->formats(m_type).contains(DRM_FORMAT_ARGB8888) ? DRM_FORMAT_ARGB8888 : DRM_FORMAT_XRGB8888);
        m_currentBuffer = m_swapchain->acquire();
        if (m_currentBuffer) {
            m_currentFramebuffer = m_pipeline->gpu()->importBuffer(m_currentBuffer->buffer(), FileDescriptor{});
            m_swapchain->release(m_currentBuffer);
            if (!m_currentFramebuffer) {
                qCWarning(KWIN_DRM, "Failed to create dumb framebuffer: %s", strerror(errno));
            }
        } else {
            m_currentFramebuffer.reset();
        }
    }
    return m_currentFramebuffer != nullptr;
}

bool DrmQPainterLayer::doesSwapchainFit() const
{
    return m_swapchain && m_swapchain->size() == targetRect().size();
}

std::shared_ptr<DrmFramebuffer> DrmQPainterLayer::currentBuffer() const
{
    return m_currentFramebuffer;
}

void DrmQPainterLayer::releaseBuffers()
{
    m_swapchain.reset();
}

DrmDevice *DrmQPainterLayer::scanoutDevice() const
{
    return m_pipeline->gpu()->drmDevice();
}

QHash<uint32_t, QList<uint64_t>> DrmQPainterLayer::supportedDrmFormats() const
{
    return m_pipeline->formats(m_type);
}

QList<QSize> DrmQPainterLayer::recommendedSizes() const
{
    return m_pipeline->recommendedSizes(m_type);
}

ColorDescription DrmQPainterLayer::colorDescription() const
{
    return m_pipeline->output()->layerBlendingColor();
}

DrmVirtualQPainterLayer::DrmVirtualQPainterLayer(DrmVirtualOutput *output)
    : DrmOutputLayer(output)
{
}

std::optional<OutputLayerBeginFrameInfo> DrmVirtualQPainterLayer::doBeginFrame()
{
    if (m_image.isNull() || m_image.size() != m_output->modeSize()) {
        m_image = QImage(m_output->modeSize(), QImage::Format_RGB32);
    }
    m_renderTime = std::make_unique<CpuRenderTimeQuery>();
    return OutputLayerBeginFrameInfo{
        .renderTarget = RenderTarget(&m_image),
        .repaint = QRegion(),
    };
}

bool DrmVirtualQPainterLayer::doEndFrame(const QRegion &renderedRegion, const QRegion &damagedRegion, OutputFrame *frame)
{
    m_renderTime->end();
    frame->addRenderTimeQuery(std::move(m_renderTime));
    return true;
}

void DrmVirtualQPainterLayer::releaseBuffers()
{
}

DrmDevice *DrmVirtualQPainterLayer::scanoutDevice() const
{
    // TODO make this use GraphicsBuffers too?
    return nullptr;
}

QHash<uint32_t, QList<uint64_t>> DrmVirtualQPainterLayer::supportedDrmFormats() const
{
    return {{DRM_FORMAT_ARGB8888, QList<uint64_t>{DRM_FORMAT_MOD_LINEAR}}};
}
}
