/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2015 Martin Gräßlin <mgraesslin@kde.org>
    SPDX-FileCopyrightText: 2022 Xaver Hugl <xaver.hugl@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include "core/graphicsbuffer.h"
#include "utils/filedescriptor.h"

#include <chrono>

namespace KWin
{

class DrmGpu;

class DrmFramebufferData
{
public:
    DrmFramebufferData(DrmGpu *gpu, uint32_t fbid)
        : m_gpu(gpu)
        , m_framebufferId(fbid)
    {
    }
    ~DrmFramebufferData();

    DrmGpu *const m_gpu;
    const uint32_t m_framebufferId;
};

class DrmFramebuffer
{
public:
    DrmFramebuffer(const std::shared_ptr<DrmFramebufferData> &data, GraphicsBuffer *buffer, FileDescriptor &&readFence);

    uint32_t framebufferId() const;
    void setSyncFd(FileDescriptor &&fence)
    {
        m_syncFd = std::move(fence);
        m_readable = m_syncFd.isReadable();
    }

    /**
     * may be nullptr
     */
    GraphicsBuffer *buffer() const;

    void releaseBuffer();
    bool isReadable();

    const FileDescriptor &syncFd() const;
    void setDeadline(std::chrono::steady_clock::time_point deadline);

protected:
    const std::shared_ptr<DrmFramebufferData> m_data;
    GraphicsBufferRef m_bufferRef;
    bool m_readable = false;
    FileDescriptor m_syncFd;
};

}
