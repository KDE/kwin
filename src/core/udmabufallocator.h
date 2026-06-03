/*
    SPDX-FileCopyrightText: 2026 Xaver Hugl <xaver.hugl@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once
#include "core/graphicsbufferallocator.h"
#include "utils/filedescriptor.h"

namespace KWin
{

class KWIN_EXPORT UDmabufAllocator : public GraphicsBufferAllocator
{
public:
    explicit UDmabufAllocator();

    GraphicsBuffer *allocate(const GraphicsBufferOptions &options) override;

    static GraphicsBuffer *allocate(uint32_t format, const QSize &size);
};

}
