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

    Map map(MapFlags flags) override;
    void unmap() override;

    QSize size() const override;
    bool hasAlphaChannel() const override;
    const DmaBufAttributes *dmabufAttributes() const override;

private:
    gbm_bo *m_bo;
    void *m_mapPtr = nullptr;
    void *m_mapData = nullptr;
    // the stride of the buffer mapping can be different from the stride of the buffer itself
    uint32_t m_mapStride = 0;
    DmaBufAttributes m_dmabufAttributes;
    QSize m_size;
    bool m_hasAlphaChannel;
};

class KWIN_EXPORT DumbGraphicsBuffer : public GraphicsBuffer
{
    Q_OBJECT

public:
    DumbGraphicsBuffer(int drmFd, uint32_t handle, DmaBufAttributes attributes);
    ~DumbGraphicsBuffer() override;

    Map map(MapFlags flags) override;
    void unmap() override;

    QSize size() const override;
    bool hasAlphaChannel() const override;
    const DmaBufAttributes *dmabufAttributes() const override;

private:
    int m_drmFd;
    uint32_t m_handle;
    void *m_data = nullptr;
    size_t m_size = 0;
    DmaBufAttributes m_dmabufAttributes;
    bool m_hasAlphaChannel;
};

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
