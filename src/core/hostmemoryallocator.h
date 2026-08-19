/*
    SPDX-FileCopyrightText: 2026 Xaver Hugl <xaver.hugl@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once
#include "core/graphicsbufferallocator.h"

namespace KWin
{

class KWIN_EXPORT HostMemoryGraphicsBufferAllocator : public GraphicsBufferAllocator
{
public:
    explicit HostMemoryGraphicsBufferAllocator(size_t alignment);

    GraphicsBuffer *allocate(const GraphicsBufferOptions &options) override;

private:
    const size_t m_alignment;
};

}
