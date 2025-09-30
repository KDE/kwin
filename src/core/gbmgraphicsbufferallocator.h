/*
    SPDX-FileCopyrightText: 2023 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "core/graphicsbufferallocator.h"

namespace KWin
{

class DrmDevice;

class KWIN_EXPORT GbmGraphicsBufferAllocator : public GraphicsBufferAllocator
{
public:
    explicit GbmGraphicsBufferAllocator(DrmDevice *device);
    ~GbmGraphicsBufferAllocator() override;

    GraphicsBuffer *allocate(const GraphicsBufferOptions &options) override;

private:
    DrmDevice *m_device;
};

} // namespace KWin
