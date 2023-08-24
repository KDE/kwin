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
#include "drm_pipeline.h"
#include "drm_virtual_output.h"
#include "platformsupport/scenes/qpainter/qpainterswapchain.h"

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
        m_swapchain = std::make_shared<QPainterSwapchain>(m_pipeline->gpu()->graphicsBufferAllocator(), m_pipeline->mode()->size(), DRM_FORMAT_XRGB8888);
        m_damageJournal = DamageJournal();
    }

    m_currentBuffer = m_swapchain->acquire();
    if (!m_currentBuffer) {
        return std::nullopt;
    }

    m_renderStart = std::chrono::steady_clock::now();
    const QRegion repaint = m_damageJournal.accumulate(m_currentBuffer->age(), infiniteRegion());
    return OutputLayerBeginFrameInfo{
        .renderTarget = RenderTarget(m_currentBuffer->view()->image()),
        .repaint = repaint,
    };
}

bool DrmQPainterLayer::endFrame(const QRegion &renderedRegion, const QRegion &damagedRegion)
{
    m_renderTime = std::chrono::steady_clock::now() - m_renderStart;
    m_currentFramebuffer = m_pipeline->gpu()->importBuffer(m_currentBuffer->buffer());
    m_damageJournal.add(damagedRegion);
    m_swapchain->release(m_currentBuffer);
    if (!m_currentFramebuffer) {
        qCWarning(KWIN_DRM, "Failed to create dumb framebuffer: %s", strerror(errno));
    }
    return m_currentFramebuffer != nullptr;
}

bool DrmQPainterLayer::checkTestBuffer()
{
    if (!doesSwapchainFit()) {
        m_swapchain = std::make_shared<QPainterSwapchain>(m_pipeline->gpu()->graphicsBufferAllocator(), m_pipeline->mode()->size(), DRM_FORMAT_XRGB8888);
        m_currentBuffer = m_swapchain->acquire();
        if (m_currentBuffer) {
            m_currentFramebuffer = m_pipeline->gpu()->importBuffer(m_currentBuffer->buffer());
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
    return m_swapchain && m_swapchain->size() == m_pipeline->mode()->size();
}

std::shared_ptr<DrmFramebuffer> DrmQPainterLayer::currentBuffer() const
{
    return m_currentFramebuffer;
}

QRegion DrmQPainterLayer::currentDamage() const
{
    return m_damageJournal.lastDamage();
}

void DrmQPainterLayer::releaseBuffers()
{
    m_swapchain.reset();
}

quint32 DrmQPainterLayer::format() const
{
    return DRM_FORMAT_XRGB8888;
}

std::chrono::nanoseconds DrmQPainterLayer::queryRenderTime() const
{
    return m_renderTime;
}

DrmCursorQPainterLayer::DrmCursorQPainterLayer(DrmPipeline *pipeline)
    : DrmPipelineLayer(pipeline)
{
}

std::optional<OutputLayerBeginFrameInfo> DrmCursorQPainterLayer::beginFrame()
{
    if (!m_swapchain) {
        m_swapchain = std::make_shared<QPainterSwapchain>(m_pipeline->gpu()->graphicsBufferAllocator(), m_pipeline->gpu()->cursorSize(), DRM_FORMAT_ARGB8888);
    }
    m_currentBuffer = m_swapchain->acquire();
    if (!m_currentBuffer) {
        return std::nullopt;
    }
    m_renderStart = std::chrono::steady_clock::now();
    return OutputLayerBeginFrameInfo{
        .renderTarget = RenderTarget(m_currentBuffer->view()->image()),
        .repaint = infiniteRegion(),
    };
}

bool DrmCursorQPainterLayer::endFrame(const QRegion &renderedRegion, const QRegion &damagedRegion)
{
    m_renderTime = std::chrono::steady_clock::now() - m_renderStart;
    m_currentFramebuffer = m_pipeline->gpu()->importBuffer(m_currentBuffer->buffer());
    m_swapchain->release(m_currentBuffer);
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

std::chrono::nanoseconds DrmCursorQPainterLayer::queryRenderTime() const
{
    return m_renderTime;
}

DrmVirtualQPainterLayer::DrmVirtualQPainterLayer(DrmVirtualOutput *output)
    : m_output(output)
{
}

std::optional<OutputLayerBeginFrameInfo> DrmVirtualQPainterLayer::beginFrame()
{
    if (m_image.isNull() || m_image.size() != m_output->modeSize()) {
        m_image = QImage(m_output->modeSize(), QImage::Format_RGB32);
    }
    m_renderStart = std::chrono::steady_clock::now();
    return OutputLayerBeginFrameInfo{
        .renderTarget = RenderTarget(&m_image),
        .repaint = QRegion(),
    };
}

bool DrmVirtualQPainterLayer::endFrame(const QRegion &renderedRegion, const QRegion &damagedRegion)
{
    m_renderTime = std::chrono::steady_clock::now() - m_renderStart;
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

std::chrono::nanoseconds DrmVirtualQPainterLayer::queryRenderTime() const
{
    return m_renderTime;
}
}
