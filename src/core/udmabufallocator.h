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
    explicit UDmabufAllocator(FileDescriptor &&dev, dev_t devNum);

    GraphicsBuffer *allocate(const GraphicsBufferOptions &options) override;

    static std::unique_ptr<UDmabufAllocator> create();

private:
    const FileDescriptor m_dev;
    const dev_t m_devNum;
};

}
