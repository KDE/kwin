/*
    SPDX-FileCopyrightText: 2023 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "plugins/qpa/swapchain.h"

namespace KWin
{
namespace QPA
{

Swapchain::Swapchain(GraphicsBufferAllocator *allocator, const GraphicsBufferOptions &options, GraphicsBuffer *initialBuffer)
    : m_allocator(allocator)
    , m_allocationOptions(options)
{
    m_buffers.push_back(initialBuffer);
}

Swapchain::~Swapchain()
{
    for (GraphicsBuffer *buffer : std::as_const(m_buffers)) {
        buffer->drop();
    }
}

QSize Swapchain::size() const
{
    return m_allocationOptions.size;
}

GraphicsBuffer *Swapchain::acquire()
{
    for (GraphicsBuffer *buffer : std::as_const(m_buffers)) {
        if (!buffer->isReferenced()) {
            return buffer;
        }
    }

    GraphicsBuffer *buffer = m_allocator->allocate(m_allocationOptions);
    if (!buffer) {
        return nullptr;
    }

    m_buffers.append(buffer);
    return buffer;
}

uint32_t Swapchain::format() const
{
    return m_allocationOptions.format;
}

QVector<uint64_t> Swapchain::modifiers() const
{
    return m_allocationOptions.modifiers;
}
}
}
