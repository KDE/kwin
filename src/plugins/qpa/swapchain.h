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
    Swapchain(GraphicsBufferAllocator *allocator, const GraphicsBufferOptions &options);
    ~Swapchain();

    QSize size() const;

    GraphicsBuffer *acquire();

private:
    GraphicsBufferAllocator *m_allocator;
    GraphicsBufferOptions m_allocationOptions;
    QList<GraphicsBuffer *> m_buffers;
};

}
}
