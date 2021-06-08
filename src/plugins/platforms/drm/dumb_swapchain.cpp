/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2021 Xaver Hugl <xaver.hugl@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "dumb_swapchain.h"
#include "drm_gpu.h"
#include "logging.h"

namespace KWin
{

DumbSwapchain::DumbSwapchain(DrmGpu *gpu, const QSize &size)
    : m_size(size)
{
    for (int i = 0; i < 2; i++) {
        auto buffer = QSharedPointer<DrmDumbBuffer>::create(gpu, size);
        if (!buffer->bufferId()) {
            break;
        }
        if (!buffer->map()) {
            break;
        }
        buffer->image()->fill(Qt::black);
        m_buffers << buffer;
    }
    if (m_buffers.count() < 2) {
        qCWarning(KWIN_DRM) << "Failed to create dumb buffers for swapchain!";
        m_buffers.clear();
    }
}

QSharedPointer<DrmDumbBuffer> DumbSwapchain::acquireBuffer()
{
    if (m_buffers.isEmpty()) {
        return nullptr;
    }
    index = (index + 1) % m_buffers.count();
    return m_buffers[index];
}

QSharedPointer<DrmDumbBuffer> DumbSwapchain::currentBuffer() const
{
    return m_buffers[index];
}

}
