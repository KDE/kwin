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
#include "drm_dumb_swapchain.h"
#include "drm_gpu.h"
#include "drm_logging.h"
#include "drm_output.h"
#include "drm_pipeline.h"
#include "drm_qpainter_backend.h"
#include "drm_virtual_output.h"

#include <cerrno>
#include <drm_fourcc.h>

namespace KWin
{

DrmQPainterLayer::DrmQPainterLayer(DrmPipeline *pipeline)
    : DrmPipelineLayer(pipeline)
{
}

std::optional<OutputLayerBeginFrameInfo> DrmQPainterLayer::beginFrame()
{
    if (!doesSwapchainFit()) {
        m_swapchain = std::make_shared<DumbSwapchain>(m_pipeline->gpu(), m_pipeline->bufferSize(), DRM_FORMAT_XRGB8888);
    }
    QRegion needsRepaint;
    if (!m_swapchain->acquireBuffer(&needsRepaint)) {
        return std::nullopt;
    }
    return OutputLayerBeginFrameInfo{
        .renderTarget = RenderTarget(m_swapchain->currentBuffer()->image()),
        .repaint = needsRepaint,
    };
}

bool DrmQPainterLayer::endFrame(const QRegion &renderedRegion, const QRegion &damagedRegion)
{
    m_currentDamage = damagedRegion;
    m_swapchain->releaseBuffer(m_swapchain->currentBuffer(), damagedRegion);
    m_currentFramebuffer = DrmFramebuffer::createFramebuffer(m_swapchain->currentBuffer());
    if (!m_currentFramebuffer) {
        qCWarning(KWIN_DRM, "Failed to create dumb framebuffer: %s", strerror(errno));
    }
    return m_currentFramebuffer != nullptr;
}

bool DrmQPainterLayer::checkTestBuffer()
{
    if (!doesSwapchainFit()) {
        m_swapchain = std::make_shared<DumbSwapchain>(m_pipeline->gpu(), m_pipeline->bufferSize(), DRM_FORMAT_XRGB8888);
        if (!m_swapchain->isEmpty()) {
            m_currentFramebuffer = DrmFramebuffer::createFramebuffer(m_swapchain->currentBuffer());
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
    return m_swapchain && m_swapchain->size() == m_pipeline->bufferSize();
}

std::shared_ptr<DrmFramebuffer> DrmQPainterLayer::currentBuffer() const
{
    return m_currentFramebuffer;
}

QRegion DrmQPainterLayer::currentDamage() const
{
    return m_currentDamage;
}

void DrmQPainterLayer::releaseBuffers()
{
    m_swapchain.reset();
}

DrmCursorQPainterLayer::DrmCursorQPainterLayer(DrmPipeline *pipeline)
    : DrmOverlayLayer(pipeline)
{
}

std::optional<OutputLayerBeginFrameInfo> DrmCursorQPainterLayer::beginFrame()
{
    if (!m_swapchain) {
        m_swapchain = std::make_shared<DumbSwapchain>(m_pipeline->gpu(), m_pipeline->gpu()->cursorSize(), DRM_FORMAT_ARGB8888);
    }
    QRegion needsRepaint;
    if (!m_swapchain->acquireBuffer(&needsRepaint)) {
        return std::nullopt;
    }
    return OutputLayerBeginFrameInfo{
        .renderTarget = RenderTarget(m_swapchain->currentBuffer()->image()),
        .repaint = needsRepaint,
    };
}

bool DrmCursorQPainterLayer::endFrame(const QRegion &renderedRegion, const QRegion &damagedRegion)
{
    m_swapchain->releaseBuffer(m_swapchain->currentBuffer(), damagedRegion);
    m_currentFramebuffer = DrmFramebuffer::createFramebuffer(m_swapchain->currentBuffer());
    if (!m_currentFramebuffer) {
        qCWarning(KWIN_DRM, "Failed to create dumb framebuffer for the cursor: %s", strerror(errno));
    }
    return m_currentFramebuffer != nullptr;
}

bool DrmCursorQPainterLayer::checkTestBuffer()
{
    return false;
}

std::shared_ptr<DrmFramebuffer> DrmCursorQPainterLayer::currentBuffer() const
{
    return m_currentFramebuffer;
}

QRegion DrmCursorQPainterLayer::currentDamage() const
{
    return {};
}

void DrmCursorQPainterLayer::releaseBuffers()
{
    m_swapchain.reset();
}

DrmVirtualQPainterLayer::DrmVirtualQPainterLayer(DrmVirtualOutput *output)
    : m_output(output)
{
}

std::optional<OutputLayerBeginFrameInfo> DrmVirtualQPainterLayer::beginFrame()
{
    if (m_image.isNull() || m_image.size() != m_output->pixelSize()) {
        m_image = QImage(m_output->pixelSize(), QImage::Format_RGB32);
    }
    return OutputLayerBeginFrameInfo{
        .renderTarget = RenderTarget(&m_image),
        .repaint = QRegion(),
    };
}

bool DrmVirtualQPainterLayer::endFrame(const QRegion &renderedRegion, const QRegion &damagedRegion)
{
    m_currentDamage = damagedRegion;
    return true;
}

QRegion DrmVirtualQPainterLayer::currentDamage() const
{
    return m_currentDamage;
}

void DrmVirtualQPainterLayer::releaseBuffers()
{
}
}
