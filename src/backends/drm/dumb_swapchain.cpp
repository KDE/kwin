/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2021 Xaver Hugl <xaver.hugl@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "dumb_swapchain.h"
#include "drm_gpu.h"
#include "drm_buffer.h"
#include "logging.h"

namespace KWin
{

DumbSwapchain::DumbSwapchain(DrmGpu *gpu, const QSize &size, uint32_t drmFormat, QImage::Format imageFormat)
    : m_size(size)
    , m_format(drmFormat)
{
    for (int i = 0; i < 2; i++) {
        auto buffer = QSharedPointer<DrmDumbBuffer>::create(gpu, size, drmFormat);
        if (!buffer->bufferId()) {
            break;
        }
        if (!buffer->map(imageFormat)) {
            break;
        }
        buffer->image()->fill(Qt::black);
        m_slots.append(Slot{.buffer = buffer, .age = 0,});
    }
    if (m_slots.count() < 2) {
        qCWarning(KWIN_DRM) << "Failed to create dumb buffers for swapchain!";
        m_slots.clear();
    }
}

QSharedPointer<DrmDumbBuffer> DumbSwapchain::acquireBuffer(int *age)
{
    if (m_slots.isEmpty()) {
        return nullptr;
    }
    index = (index + 1) % m_slots.count();
    if (age) {
        *age = m_slots[index].age;
    }
    return m_slots[index].buffer;
}

QSharedPointer<DrmDumbBuffer> DumbSwapchain::currentBuffer() const
{
    return m_slots[index].buffer;
}

void DumbSwapchain::releaseBuffer(QSharedPointer<DrmDumbBuffer> buffer)
{
    Q_ASSERT(m_slots[index].buffer == buffer);

    for (qsizetype i = 0; i < m_slots.count(); ++i) {
        if (m_slots[i].buffer == buffer) {
            m_slots[i].age = 1;
        } else if (m_slots[i].age > 0) {
            m_slots[i].age++;
        }
    }
}

uint32_t DumbSwapchain::drmFormat() const
{
    return m_format;
}

qsizetype DumbSwapchain::slotCount() const
{
    return m_slots.count();
}

QSize DumbSwapchain::size() const
{
    return m_size;
}

bool DumbSwapchain::isEmpty() const
{
    return m_slots.isEmpty();
}

}
