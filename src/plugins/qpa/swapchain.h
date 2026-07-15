/*
    SPDX-FileCopyrightText: 2023 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "core/graphicsbuffer.h"
#include "core/graphicsbufferallocator.h"

namespace KWin
{

namespace QPA
{

class Swapchain
{
public:
    Swapchain(GraphicsBufferAllocator *allocator, const GraphicsBufferOptions &options, std::shared_ptr<GraphicsBuffer> &&initialBuffer);
    ~Swapchain();

    QSize size() const;

    GraphicsBuffer *acquire();
    uint32_t format() const;
    const ModifierList &modifiers() const;

private:
    GraphicsBufferAllocator *m_allocator;
    GraphicsBufferOptions m_allocationOptions;
    QList<std::shared_ptr<GraphicsBuffer>> m_buffers;
};

}
}
