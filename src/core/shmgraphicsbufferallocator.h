/*
    SPDX-FileCopyrightText: 2023 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "core/graphicsbuffer.h"
#include "core/graphicsbufferallocator.h"
#include "utils/memorymap.h"

namespace KWin
{

class KWIN_EXPORT ShmGraphicsBuffer : public GraphicsBuffer
{
    Q_OBJECT

public:
    explicit ShmGraphicsBuffer(ShmAttributes &&attributes);

    void *map(MapFlags flags) override;
    void unmap() override;

    QSize size() const override;
    bool hasAlphaChannel() const override;
    const ShmAttributes *shmAttributes() const override;

private:
    ShmAttributes m_attributes;
    MemoryMap m_memoryMap;
    bool m_hasAlphaChannel;
};

class KWIN_EXPORT ShmGraphicsBufferAllocator : public GraphicsBufferAllocator
{
public:
    GraphicsBuffer *allocate(const GraphicsBufferOptions &options) override;
};

} // namespace KWin
