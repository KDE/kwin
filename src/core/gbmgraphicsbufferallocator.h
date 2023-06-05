/*
    SPDX-FileCopyrightText: 2023 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "core/graphicsbuffer.h"
#include "core/graphicsbufferallocator.h"

struct gbm_bo;
struct gbm_device;

namespace KWin
{

class KWIN_EXPORT GbmGraphicsBuffer : public GraphicsBuffer
{
    Q_OBJECT

public:
    GbmGraphicsBuffer(DmaBufAttributes attributes, gbm_bo *handle);
    ~GbmGraphicsBuffer() override;

    QSize size() const override;
    bool hasAlphaChannel() const override;
    const DmaBufAttributes *dmabufAttributes() const override;

    void *map() override;
    void unmap() override;

private:
    gbm_bo *m_bo;
    void *m_mapPtr = nullptr;
    void *m_mapData = nullptr;
    DmaBufAttributes m_dmabufAttributes;
    QSize m_size;
    bool m_hasAlphaChannel;
};

class KWIN_EXPORT GbmGraphicsBufferAllocator : public GraphicsBufferAllocator
{
public:
    explicit GbmGraphicsBufferAllocator(gbm_device *device);
    ~GbmGraphicsBufferAllocator() override;

    GbmGraphicsBuffer *allocate(const GraphicsBufferOptions &options) override;

private:
    gbm_device *m_gbmDevice;
};

} // namespace KWin
