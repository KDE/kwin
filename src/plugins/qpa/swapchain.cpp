/*
    SPDX-FileCopyrightText: 2023 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "plugins/qpa/swapchain.h"

namespace KWin
{
namespace QPA
{

Swapchain::Swapchain(GraphicsBufferAllocator *allocator, const GraphicsBufferOptions &options, std::shared_ptr<GraphicsBuffer> &&initialBuffer)
    : m_allocator(allocator)
    , m_allocationOptions(options)
{
    m_buffers.push_back(std::move(initialBuffer));
}

Swapchain::~Swapchain()
{
}

QSize Swapchain::size() const
{
    return m_allocationOptions.size;
}

std::shared_ptr<GraphicsBuffer> Swapchain::acquire()
{
    for (const auto &buffer : std::as_const(m_buffers)) {
        if (!buffer->isReferenced()) {
            return buffer;
        }
    }

    auto buffer = m_allocator->allocate(m_allocationOptions);
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

const ModifierList &Swapchain::modifiers() const
{
    return m_allocationOptions.modifiers;
}

}
}
