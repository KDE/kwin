/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2022 Xaver Hugl <xaver.hugl@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "drm_dumb_buffer.h"

#include "drm_gpu.h"
#include "drm_logging.h"

#include <cerrno>
#include <drm_fourcc.h>
#include <sys/mman.h>
#include <xf86drm.h>

namespace KWin
{

DrmDumbBuffer::DrmDumbBuffer(DrmGpu *gpu, const QSize &size, uint32_t format, uint32_t handle, uint32_t stride, size_t bufferSize)
    : DrmGpuBuffer(gpu, size, format, DRM_FORMAT_MOD_LINEAR, {handle, 0, 0, 0}, {stride, 0, 0, 0}, {0, 0, 0, 0}, 1)
    , m_bufferSize(bufferSize)
{
}

DrmDumbBuffer::~DrmDumbBuffer()
{
    m_image.reset();
    if (m_memory) {
        munmap(m_memory, m_bufferSize);
    }
    drm_mode_destroy_dumb destroyArgs;
    destroyArgs.handle = m_handles[0];
    drmIoctl(m_gpu->fd(), DRM_IOCTL_MODE_DESTROY_DUMB, &destroyArgs);
}

bool DrmDumbBuffer::map(QImage::Format format)
{
    drm_mode_map_dumb mapArgs;
    memset(&mapArgs, 0, sizeof mapArgs);
    mapArgs.handle = m_handles[0];
    if (drmIoctl(m_gpu->fd(), DRM_IOCTL_MODE_MAP_DUMB, &mapArgs) != 0) {
        return false;
    }
#ifdef KWIN_UNIT_TEST
    m_memory = reinterpret_cast<void *>(mapArgs.offset);
#else
    void *address = mmap(nullptr, m_bufferSize, PROT_WRITE, MAP_SHARED, m_gpu->fd(), mapArgs.offset);
    if (address == MAP_FAILED) {
        return false;
    }
    m_memory = address;
#endif
    m_image = std::make_unique<QImage>((uchar *)m_memory, m_size.width(), m_size.height(), m_strides[0], format);
    return !m_image->isNull();
}

QImage *DrmDumbBuffer::image() const
{
    return m_image.get();
}

void *DrmDumbBuffer::data() const
{
    return m_memory;
}

std::shared_ptr<DrmDumbBuffer> DrmDumbBuffer::createDumbBuffer(DrmGpu *gpu, const QSize &size, uint32_t format)
{
    drm_mode_create_dumb createArgs;
    memset(&createArgs, 0, sizeof createArgs);
    createArgs.bpp = 32;
    createArgs.width = size.width();
    createArgs.height = size.height();
    if (drmIoctl(gpu->fd(), DRM_IOCTL_MODE_CREATE_DUMB, &createArgs) != 0) {
        qCWarning(KWIN_DRM) << "DRM_IOCTL_MODE_CREATE_DUMB failed" << strerror(errno);
        return nullptr;
    }
    return std::make_shared<DrmDumbBuffer>(gpu, size, format, createArgs.handle, createArgs.pitch, createArgs.size);
}

}
