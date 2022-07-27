/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2015 Martin Gräßlin <mgraesslin@kde.org>
    SPDX-FileCopyrightText: 2022 Xaver Hugl <xaver.hugl@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "drm_buffer.h"

#include "drm_gpu.h"
#include "drm_logging.h"

// system
#include <sys/mman.h>
// c++
#include <cerrno>
// drm
#include <drm_fourcc.h>
#include <unistd.h>
#include <xf86drm.h>
#include <xf86drmMode.h>

namespace KWin
{

DrmGpuBuffer::DrmGpuBuffer(DrmGpu *gpu, QSize size, uint32_t format, uint64_t modifier, const std::array<uint32_t, 4> &handles, const std::array<uint32_t, 4> &strides, const std::array<uint32_t, 4> &offsets, uint32_t planeCount)
    : m_gpu(gpu)
    , m_size(size)
    , m_format(format)
    , m_modifier(modifier)
    , m_handles(handles)
    , m_strides(strides)
    , m_offsets(offsets)
    , m_planeCount(planeCount)
{
}

DrmGpu *DrmGpuBuffer::gpu() const
{
    return m_gpu;
}

uint32_t DrmGpuBuffer::format() const
{
    return m_format;
}

uint64_t DrmGpuBuffer::modifier() const
{
    return m_modifier;
}

QSize DrmGpuBuffer::size() const
{
    return m_size;
}

const std::array<FileDescriptor, 4> &DrmGpuBuffer::fds()
{
    if (!m_fds[0].isValid()) {
        createFds();
    }
    return m_fds;
}

std::array<uint32_t, 4> DrmGpuBuffer::handles() const
{
    return m_handles;
}

std::array<uint32_t, 4> DrmGpuBuffer::strides() const
{
    return m_strides;
}

std::array<uint32_t, 4> DrmGpuBuffer::offsets() const
{
    return m_offsets;
}

uint32_t DrmGpuBuffer::planeCount() const
{
    return m_planeCount;
}

void DrmGpuBuffer::createFds()
{
}

DrmFramebuffer::DrmFramebuffer(const std::shared_ptr<DrmGpuBuffer> &buffer, uint32_t fbId)
    : m_framebufferId(fbId)
    , m_gpu(buffer->gpu())
    , m_buffer(buffer)
{
}

DrmFramebuffer::~DrmFramebuffer()
{
    drmModeRmFB(m_gpu->fd(), m_framebufferId);
}

uint32_t DrmFramebuffer::framebufferId() const
{
    return m_framebufferId;
}

DrmGpuBuffer *DrmFramebuffer::buffer() const
{
    return m_buffer.get();
}

void DrmFramebuffer::releaseBuffer()
{
    m_buffer.reset();
}

std::shared_ptr<DrmFramebuffer> DrmFramebuffer::createFramebuffer(const std::shared_ptr<DrmGpuBuffer> &buffer)
{
    const auto size = buffer->size();
    const auto handles = buffer->handles();
    const auto strides = buffer->strides();
    const auto offsets = buffer->offsets();

    uint32_t framebufferId = 0;
    int ret;
    if (buffer->gpu()->addFB2ModifiersSupported() && buffer->modifier() != DRM_FORMAT_MOD_INVALID) {
        uint64_t modifier[4];
        for (uint32_t i = 0; i < 4; i++) {
            modifier[i] = i < buffer->planeCount() ? buffer->modifier() : 0;
        }
        ret = drmModeAddFB2WithModifiers(buffer->gpu()->fd(), size.width(), size.height(), buffer->format(), handles.data(), strides.data(), offsets.data(), modifier, &framebufferId, DRM_MODE_FB_MODIFIERS);
    } else {
        ret = drmModeAddFB2(buffer->gpu()->fd(), size.width(), size.height(), buffer->format(), handles.data(), strides.data(), offsets.data(), &framebufferId, 0);
        if (ret == EOPNOTSUPP && handles.size() == 1) {
            ret = drmModeAddFB(buffer->gpu()->fd(), size.width(), size.height(), 24, 32, strides[0], handles[0], &framebufferId);
        }
    }
    if (ret == 0) {
        return std::make_shared<DrmFramebuffer>(buffer, framebufferId);
    } else {
        return nullptr;
    }
}
}
