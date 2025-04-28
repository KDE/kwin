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

#include <QPointer>
#include <chrono>

namespace KWin
{

class DrmGpu;

class DrmFramebufferData
{
public:
    DrmFramebufferData(DrmGpu *gpu, uint32_t fbid, GraphicsBuffer *buffer);
    ~DrmFramebufferData();

    DrmGpu *const m_gpu;
    const uint32_t m_framebufferId;
    const QPointer<GraphicsBuffer> m_buffer;
};

class DrmFramebuffer
{
public:
    DrmFramebuffer(const std::shared_ptr<DrmFramebufferData> &data, GraphicsBuffer *buffer, FileDescriptor &&readFence);

    uint32_t framebufferId() const;

    /**
     * may be nullptr
     */
    GraphicsBuffer *buffer() const;

    void releaseBuffer();
    bool isReadable();

    const FileDescriptor &syncFd() const;
    void setDeadline(std::chrono::steady_clock::time_point deadline);

    std::shared_ptr<DrmFramebufferData> data() const;

protected:
    const std::shared_ptr<DrmFramebufferData> m_data;
    GraphicsBufferRef m_bufferRef;
    bool m_readable = false;
    FileDescriptor m_syncFd;
};

}
