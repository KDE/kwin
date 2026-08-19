/*
    SPDX-FileCopyrightText: 2023 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "core/graphicsbufferallocator.h"

namespace KWin
{

GraphicsBufferAllocator::GraphicsBufferAllocator()
{
}

GraphicsBufferAllocator::~GraphicsBufferAllocator()
{
}

uint64_t GraphicsBufferAllocator::align(uint64_t size, uint64_t minimum)
{
    if (auto remainder = size % minimum) {
        return size + (minimum - remainder);
    } else {
        return size;
    }
}

} // namespace KWin
