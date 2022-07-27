/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2015 Martin Gräßlin <mgraesslin@kde.org>
    SPDX-FileCopyrightText: 2022 Xaver Hugl <xaver.hugl@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include "utils/filedescriptor.h"

#include <QImage>
#include <QSize>
#include <array>
#include <memory>
#include <optional>

namespace KWin
{

class DrmGpu;
class DrmFramebuffer;

class DrmGpuBuffer
{
public:
    DrmGpuBuffer(DrmGpu *gpu, QSize size, uint32_t format, uint64_t modifier, const std::array<uint32_t, 4> &handles, const std::array<uint32_t, 4> &strides, const std::array<uint32_t, 4> &offsets, uint32_t planeCount);
    virtual ~DrmGpuBuffer() = default;

    DrmGpu *gpu() const;
    uint32_t format() const;
    uint64_t modifier() const;
    QSize size() const;
    const std::array<FileDescriptor, 4> &fds();
    std::array<uint32_t, 4> handles() const;
    std::array<uint32_t, 4> strides() const;
    std::array<uint32_t, 4> offsets() const;
    uint32_t planeCount() const;

protected:
    virtual void createFds();

    DrmGpu *const m_gpu;
    const QSize m_size;
    const uint32_t m_format;
    const uint64_t m_modifier;
    const std::array<uint32_t, 4> m_handles;
    const std::array<uint32_t, 4> m_strides;
    const std::array<uint32_t, 4> m_offsets;
    const uint32_t m_planeCount;
    std::array<FileDescriptor, 4> m_fds;
};

class DrmFramebuffer
{
public:
    DrmFramebuffer(const std::shared_ptr<DrmGpuBuffer> &buffer, uint32_t fbId);
    ~DrmFramebuffer();

    uint32_t framebufferId() const;
    /**
     * may be nullptr
     */
    DrmGpuBuffer *buffer() const;

    void releaseBuffer();

    static std::shared_ptr<DrmFramebuffer> createFramebuffer(const std::shared_ptr<DrmGpuBuffer> &buffer);

protected:
    const uint32_t m_framebufferId;
    DrmGpu *const m_gpu;
    std::shared_ptr<DrmGpuBuffer> m_buffer;
};

}
