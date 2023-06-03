/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2021 Xaver Hugl <xaver.hugl@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "backends/drm/drm_dumb_swapchain.h"
#include "core/gbmgraphicsbufferallocator.h"
#include "core/graphicsbufferview.h"
#include "backends/drm/drm_gpu.h"

#include <memory>

namespace KWin
{

DumbSwapchainSlot::DumbSwapchainSlot(GraphicsBuffer *buffer)
    : m_buffer(buffer)
    , m_view(std::make_unique<GraphicsBufferView>(buffer, GraphicsBuffer::Read | GraphicsBuffer::Write))
{
}

DumbSwapchainSlot::~DumbSwapchainSlot()
{
    m_view.reset();
    m_buffer->drop();
}

GraphicsBuffer *DumbSwapchainSlot::buffer() const
{
    return m_buffer;
}

GraphicsBufferView *DumbSwapchainSlot::view() const
{
    return m_view.get();
}

int DumbSwapchainSlot::age() const
{
    return m_age;
}

DumbSwapchain::DumbSwapchain(DrmGpu *gpu, const QSize &size, uint32_t format)
    : m_allocator(std::make_unique<GbmGraphicsBufferAllocator>(gpu->gbmDevice()))
    , m_size(size)
    , m_format(format)
{
}

DumbSwapchain::~DumbSwapchain()
{
}

QSize DumbSwapchain::size() const
{
    return m_size;
}

uint32_t DumbSwapchain::format() const
{
    return m_format;
}

std::shared_ptr<DumbSwapchainSlot> DumbSwapchain::acquire()
{
    for (const auto &slot : std::as_const(m_slots)) {
        if (!slot->buffer()->isReferenced()) {
            return slot;
        }
    }

    GraphicsBuffer *buffer = m_allocator->allocate(GraphicsBufferOptions{
        .size = m_size,
        .format = m_format,
        .software = true,
    });
    if (!buffer) {
        qCWarning(KWIN_DRM) << "Failed to allocate a dumb buffer";
        return nullptr;
    }

    auto slot = std::make_shared<DumbSwapchainSlot>(buffer);
    m_slots.append(slot);

    return slot;
}

void DumbSwapchain::release(std::shared_ptr<DumbSwapchainSlot> slot)
{
    for (qsizetype i = 0; i < m_slots.count(); ++i) {
        if (m_slots[i] == slot) {
            m_slots[i]->m_age = 1;
        } else if (m_slots[i]->m_age > 0) {
            m_slots[i]->m_age++;
        }
    }
}

}
