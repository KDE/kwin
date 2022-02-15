/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2022 Xaver Hugl <xaver.hugl@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "drm_qpainter_layer.h"
#include "dumb_swapchain.h"
#include "drm_abstract_output.h"
#include "drm_buffer.h"
#include "scene_qpainter_drm_backend.h"
#include "drm_gpu.h"
#include "drm_backend.h"

#include <drm_fourcc.h>

namespace KWin
{

DrmQPainterLayer::DrmQPainterLayer(DrmDisplayDevice *displayDevice)
    : DrmLayer(displayDevice)
{
    connect(static_cast<DrmQPainterBackend*>(displayDevice->gpu()->platform()->renderBackend()), &DrmQPainterBackend::aboutToBeDestroyed, this, [this]() {
        m_swapchain.reset();
    });
}

std::optional<QRegion> DrmQPainterLayer::startRendering()
{
    if (!doesSwapchainFit()) {
        m_swapchain = QSharedPointer<DumbSwapchain>::create(m_displayDevice->gpu(), m_displayDevice->sourceSize(), DRM_FORMAT_XRGB8888);
    }
    QRegion needsRepaint;
    if (!m_swapchain->acquireBuffer(m_displayDevice->renderGeometry(), &needsRepaint)) {
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

bool DrmQPainterLayer::scanout(SurfaceItem *surfaceItem)
{
    Q_UNUSED(surfaceItem);
    return false;
}

QSharedPointer<DrmBuffer> DrmQPainterLayer::testBuffer()
{
    if (!doesSwapchainFit()) {
        m_swapchain = QSharedPointer<DumbSwapchain>::create(m_displayDevice->gpu(), m_displayDevice->sourceSize(), DRM_FORMAT_XRGB8888);
    }
    return m_swapchain->currentBuffer();
}

bool DrmQPainterLayer::doesSwapchainFit() const
{
    return m_swapchain && m_swapchain->size() == m_displayDevice->sourceSize();
}

QSharedPointer<DrmBuffer> DrmQPainterLayer::currentBuffer() const
{
    return m_swapchain ? m_swapchain->currentBuffer() : nullptr;
}

QRegion DrmQPainterLayer::currentDamage() const
{
    return m_currentDamage;
}

bool DrmQPainterLayer::hasDirectScanoutBuffer() const
{
    return false;
}

}
