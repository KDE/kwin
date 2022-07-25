/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2021 Xaver Hugl <xaver.hugl@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "drm_dumb_swapchain.h"
#include "drm_buffer.h"
#include "drm_dumb_buffer.h"
#include "drm_gpu.h"
#include "drm_logging.h"

namespace KWin
{

DumbSwapchain::DumbSwapchain(DrmGpu *gpu, const QSize &size, uint32_t drmFormat)
    : m_size(size)
    , m_format(drmFormat)
{
    for (int i = 0; i < 2; i++) {
        auto buffer = DrmDumbBuffer::createDumbBuffer(gpu, size, drmFormat);
        if (!buffer->map(QImage::Format::Format_ARGB32)) {
            break;
        }
        buffer->image()->fill(Qt::black);
        m_slots.append(Slot{
            .buffer = buffer,
            .age = 0,
        });
    }
    m_damageJournal.setCapacity(2);
    if (m_slots.count() < 2) {
        qCWarning(KWIN_DRM) << "Failed to create dumb buffers for swapchain!";
        m_slots.clear();
    }
}

std::shared_ptr<DrmDumbBuffer> DumbSwapchain::acquireBuffer(QRegion *needsRepaint)
{
    if (m_slots.isEmpty()) {
        return {};
    }
    index = (index + 1) % m_slots.count();
    if (needsRepaint) {
        *needsRepaint = m_damageJournal.accumulate(m_slots[index].age, infiniteRegion());
    }
    return m_slots[index].buffer;
}

std::shared_ptr<DrmDumbBuffer> DumbSwapchain::currentBuffer() const
{
    return m_slots[index].buffer;
}

void DumbSwapchain::releaseBuffer(const std::shared_ptr<DrmDumbBuffer> &buffer, const QRegion &damage)
{
    Q_ASSERT(m_slots[index].buffer == buffer);

    for (qsizetype i = 0; i < m_slots.count(); ++i) {
        if (m_slots[i].buffer == buffer) {
            m_slots[i].age = 1;
        } else if (m_slots[i].age > 0) {
            m_slots[i].age++;
        }
    }
    m_damageJournal.add(damage);
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
