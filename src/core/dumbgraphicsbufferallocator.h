/*
    SPDX-FileCopyrightText: 2023 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "core/graphicsbuffer.h"
#include "core/graphicsbufferallocator.h"

namespace KWin
{

class KWIN_EXPORT DumbGraphicsBuffer : public GraphicsBuffer
{
    Q_OBJECT

public:
    DumbGraphicsBuffer(int drmFd, uint32_t handle, DmaBufAttributes attributes);
    ~DumbGraphicsBuffer() override;

    QSize size() const override;
    bool hasAlphaChannel() const override;
    const DmaBufAttributes *dmabufAttributes() const override;

    void *map();
    void unmap();

private:
    int m_drmFd;
    uint32_t m_handle;
    void *m_data = nullptr;
    size_t m_size = 0;
    DmaBufAttributes m_dmabufAttributes;
    bool m_hasAlphaChannel;
};

class KWIN_EXPORT DumbGraphicsBufferAllocator : public GraphicsBufferAllocator
{
public:
    explicit DumbGraphicsBufferAllocator(int drmFd);

    DumbGraphicsBuffer *allocate(const QSize &size, uint32_t format, const QVector<uint64_t> &modifiers = {}) override;

private:
    int m_drmFd;
};

} // namespace KWin
