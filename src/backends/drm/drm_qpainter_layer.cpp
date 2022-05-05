/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2022 Xaver Hugl <xaver.hugl@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "drm_qpainter_layer.h"
#include "drm_abstract_output.h"
#include "drm_backend.h"
#include "drm_buffer.h"
#include "drm_dumb_buffer.h"
#include "drm_gpu.h"
#include "drm_output.h"
#include "drm_pipeline.h"
#include "drm_virtual_output.h"
#include "dumb_swapchain.h"
#include "scene_qpainter_drm_backend.h"

#include <drm_fourcc.h>

namespace KWin
{

DrmQPainterLayer::DrmQPainterLayer(DrmQPainterBackend *backend, DrmPipeline *pipeline)
    : DrmPipelineLayer(pipeline)
{
    connect(backend, &DrmQPainterBackend::aboutToBeDestroyed, this, [this]() {
        m_swapchain.reset();
    });
}

OutputLayerBeginFrameInfo DrmQPainterLayer::beginFrame()
{
    if (!doesSwapchainFit()) {
        m_swapchain = std::make_shared<DumbSwapchain>(m_pipeline->gpu(), m_pipeline->bufferSize(), DRM_FORMAT_XRGB8888);
    }
    QRegion needsRepaint;
    if (!m_swapchain->acquireBuffer(&needsRepaint)) {
        return {};
    }
    return OutputLayerBeginFrameInfo{
        .renderTarget = RenderTarget(m_swapchain->currentBuffer()->image()),
        .repaint = needsRepaint,
    };
}

void DrmQPainterLayer::endFrame(const QRegion &renderedRegion, const QRegion &damagedRegion)
{
    Q_UNUSED(renderedRegion)
    m_currentDamage = damagedRegion;
    m_swapchain->releaseBuffer(m_swapchain->currentBuffer(), damagedRegion);
}

bool DrmQPainterLayer::checkTestBuffer()
{
    if (!doesSwapchainFit()) {
        m_swapchain = std::make_shared<DumbSwapchain>(m_pipeline->gpu(), m_pipeline->bufferSize(), DRM_FORMAT_XRGB8888);
    }
    return !m_swapchain->isEmpty();
}

bool DrmQPainterLayer::doesSwapchainFit() const
{
    return m_swapchain && m_swapchain->size() == m_pipeline->bufferSize();
}

std::shared_ptr<DrmFramebuffer> DrmQPainterLayer::currentBuffer() const
{
    return m_swapchain ? m_currentFramebuffer : nullptr;
}

QRegion DrmQPainterLayer::currentDamage() const
{
    return m_currentDamage;
}

DrmVirtualQPainterLayer::DrmVirtualQPainterLayer(DrmVirtualOutput *output)
    : m_output(output)
{
}

OutputLayerBeginFrameInfo DrmVirtualQPainterLayer::beginFrame()
{
    if (m_image.isNull() || m_image.size() != m_output->pixelSize()) {
        m_image = QImage(m_output->pixelSize(), QImage::Format_RGB32);
    }
    return OutputLayerBeginFrameInfo{
        .renderTarget = RenderTarget(&m_image),
        .repaint = QRegion(),
    };
}

void DrmVirtualQPainterLayer::endFrame(const QRegion &renderedRegion, const QRegion &damagedRegion)
{
    Q_UNUSED(renderedRegion)
    m_currentDamage = damagedRegion;
}

QRegion DrmVirtualQPainterLayer::currentDamage() const
{
    return m_currentDamage;
}

DrmLeaseQPainterLayer::DrmLeaseQPainterLayer(DrmQPainterBackend *backend, DrmPipeline *pipeline)
    : DrmPipelineLayer(pipeline)
{
    connect(backend, &DrmQPainterBackend::aboutToBeDestroyed, this, [this]() {
        m_buffer.reset();
    });
}

bool DrmLeaseQPainterLayer::checkTestBuffer()
{
    const auto size = m_pipeline->bufferSize();
    if (!m_framebuffer || m_buffer->size() != size) {
        m_buffer = DrmDumbBuffer::createDumbBuffer(m_pipeline->gpu(), size, DRM_FORMAT_XRGB8888);
        if (m_buffer) {
            m_framebuffer = DrmFramebuffer::createFramebuffer(m_buffer);
        } else {
            m_framebuffer.reset();
        }
    }
    return m_framebuffer != nullptr;
}

std::shared_ptr<DrmFramebuffer> DrmLeaseQPainterLayer::currentBuffer() const
{
    return m_framebuffer;
}

OutputLayerBeginFrameInfo DrmLeaseQPainterLayer::beginFrame()
{
    return {};
}

void DrmLeaseQPainterLayer::endFrame(const QRegion &damagedRegion, const QRegion &renderedRegion)
{
    Q_UNUSED(damagedRegion)
    Q_UNUSED(renderedRegion)
}
}
