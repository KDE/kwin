/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2021 Xaver Hugl <xaver.hugl@gmail.com>
    SPDX-FileCopyrightText: 2023 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "platformsupport/scenes/qpainter/qpainterswapchain.h"
#include "core/graphicsbufferview.h"
#include "core/graphicsbufferallocator.h"
#include "utils/common.h"

namespace KWin
{

QPainterSwapchainSlot::QPainterSwapchainSlot(GraphicsBuffer *buffer)
    : m_buffer(buffer)
    , m_view(std::make_unique<GraphicsBufferView>(buffer, GraphicsBuffer::Read | GraphicsBuffer::Write))
{
}

QPainterSwapchainSlot::~QPainterSwapchainSlot()
{
    m_view.reset();
    m_buffer->drop();
}

GraphicsBuffer *QPainterSwapchainSlot::buffer() const
{
    return m_buffer;
}

GraphicsBufferView *QPainterSwapchainSlot::view() const
{
    return m_view.get();
}

int QPainterSwapchainSlot::age() const
{
    return m_age;
}

QPainterSwapchain::QPainterSwapchain(GraphicsBufferAllocator *allocator, const QSize &size, uint32_t format)
    : m_allocator(allocator)
    , m_size(size)
    , m_format(format)
{
}

QPainterSwapchain::~QPainterSwapchain()
{
}

QSize QPainterSwapchain::size() const
{
    return m_size;
}

uint32_t QPainterSwapchain::format() const
{
    return m_format;
}

std::shared_ptr<QPainterSwapchainSlot> QPainterSwapchain::acquire()
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
        qCWarning(KWIN_QPAINTER) << "Failed to allocate a qpainter swapchain graphics buffer";
        return nullptr;
    }

    auto slot = std::make_shared<QPainterSwapchainSlot>(buffer);
    m_slots.append(slot);

    return slot;
}

void QPainterSwapchain::release(std::shared_ptr<QPainterSwapchainSlot> slot)
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
