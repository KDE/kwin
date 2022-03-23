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

std::optional<QRegion> DrmQPainterLayer::startRendering()
{
    if (!doesSwapchainFit()) {
        m_swapchain = QSharedPointer<DumbSwapchain>::create(m_pipeline->gpu(), m_pipeline->sourceSize(), DRM_FORMAT_XRGB8888);
    }
    QRegion needsRepaint;
    if (!m_swapchain->acquireBuffer(m_pipeline->output()->geometry(), &needsRepaint)) {
        return std::optional<QRegion>();
    }
    return needsRepaint;
}

bool DrmQPainterLayer::endRendering(const QRegion &damagedRegion)
{
    m_currentDamage = damagedRegion;
    m_swapchain->releaseBuffer(m_swapchain->currentBuffer(), damagedRegion);
    return true;
}

QSharedPointer<DrmBuffer> DrmQPainterLayer::testBuffer()
{
    if (!doesSwapchainFit()) {
        m_swapchain = QSharedPointer<DumbSwapchain>::create(m_pipeline->gpu(), m_pipeline->sourceSize(), DRM_FORMAT_XRGB8888);
    }
    return m_swapchain->currentBuffer();
}

bool DrmQPainterLayer::doesSwapchainFit() const
{
    return m_swapchain && m_swapchain->size() == m_pipeline->sourceSize();
}

QSharedPointer<DrmBuffer> DrmQPainterLayer::currentBuffer() const
{
    return m_swapchain ? m_swapchain->currentBuffer() : nullptr;
}

QRegion DrmQPainterLayer::currentDamage() const
{
    return m_currentDamage;
}

QImage *DrmQPainterLayer::image()
{
    return m_swapchain ? m_swapchain->currentBuffer()->image() : nullptr;
}

DrmVirtualQPainterLayer::DrmVirtualQPainterLayer(DrmVirtualOutput *output)
    : m_output(output)
{
}

std::optional<QRegion> DrmVirtualQPainterLayer::startRendering()
{
    if (m_image.isNull() || m_image.size() != m_output->pixelSize()) {
        m_image = QImage(m_output->pixelSize(), QImage::Format_RGB32);
    }
    return QRegion();
}

bool DrmVirtualQPainterLayer::endRendering(const QRegion &damagedRegion)
{
    m_currentDamage = damagedRegion;
    return true;
}

QRegion DrmVirtualQPainterLayer::currentDamage() const
{
    return m_currentDamage;
}

QImage *DrmVirtualQPainterLayer::image()
{
    return &m_image;
}

DrmLeaseQPainterLayer::DrmLeaseQPainterLayer(DrmQPainterBackend *backend, DrmPipeline *pipeline)
    : DrmPipelineLayer(pipeline)
{
    connect(backend, &DrmQPainterBackend::aboutToBeDestroyed, this, [this]() {
        m_buffer.reset();
    });
}

QSharedPointer<DrmBuffer> DrmLeaseQPainterLayer::testBuffer()
{
    const auto size = m_pipeline->sourceSize();
    if (!m_buffer || m_buffer->size() != size) {
        m_buffer = QSharedPointer<DrmDumbBuffer>::create(m_pipeline->gpu(), size, DRM_FORMAT_XRGB8888);
    }
    return m_buffer;
}

QSharedPointer<DrmBuffer> DrmLeaseQPainterLayer::currentBuffer() const
{
    return m_buffer;
}

}
