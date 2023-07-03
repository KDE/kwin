/*
    SPDX-FileCopyrightText: 2023 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "core/graphicsbufferallocator.h"

struct gbm_bo;
struct gbm_device;

namespace KWin
{

class KWIN_EXPORT GbmGraphicsBufferAllocator : public GraphicsBufferAllocator
{
public:
    explicit GbmGraphicsBufferAllocator(gbm_device *device);
    ~GbmGraphicsBufferAllocator() override;

    GraphicsBuffer *allocate(const GraphicsBufferOptions &options) override;

private:
    gbm_device *m_gbmDevice;
};

} // namespace KWin
