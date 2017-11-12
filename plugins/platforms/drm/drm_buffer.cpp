/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2015 Martin Gräßlin <mgraesslin@kde.org>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*********************************************************************/
#include "drm_buffer.h"

#include "logging.h"

// system
#include <sys/mman.h>
#include <errno.h>
// drm
#include <xf86drm.h>
#include <xf86drmMode.h>

namespace KWin
{

DrmBuffer:: DrmBuffer(int fd)
    : m_fd(fd)
{
}

// DrmDumbBuffer
DrmDumbBuffer::DrmDumbBuffer(int fd, const QSize &size)
    : DrmBuffer(fd)
{
    m_size = size;
    drm_mode_create_dumb createArgs;
    memset(&createArgs, 0, sizeof createArgs);
    createArgs.bpp = 32;
    createArgs.width = size.width();
    createArgs.height = size.height();
    if (drmIoctl(fd, DRM_IOCTL_MODE_CREATE_DUMB, &createArgs) != 0) {
        qCWarning(KWIN_DRM) << "DRM_IOCTL_MODE_CREATE_DUMB failed";
        return;
    }
    m_handle = createArgs.handle;
    m_bufferSize = createArgs.size;
    m_stride = createArgs.pitch;
    if (drmModeAddFB(fd, size.width(), size.height(), 24, 32,
                     m_stride, createArgs.handle, &m_bufferId) != 0) {
        qCWarning(KWIN_DRM) << "drmModeAddFB failed with errno" << errno;
    }
}

DrmDumbBuffer::~DrmDumbBuffer()
{
    if (m_bufferId) {
        drmModeRmFB(fd(), m_bufferId);
    }

    delete m_image;
    if (m_memory) {
        munmap(m_memory, m_bufferSize);
    }
    if (m_handle) {
        drm_mode_destroy_dumb destroyArgs;
        destroyArgs.handle = m_handle;
        drmIoctl(fd(), DRM_IOCTL_MODE_DESTROY_DUMB, &destroyArgs);
    }
}

bool DrmDumbBuffer::needsModeChange(DrmBuffer *b) const {
    if (DrmDumbBuffer *db = dynamic_cast<DrmDumbBuffer*>(b)) {
        return m_stride != db->stride();
    } else {
        return true;
    }
}

bool DrmDumbBuffer::map(QImage::Format format)
{
    if (!m_handle || !m_bufferId) {
        return false;
    }
    drm_mode_map_dumb mapArgs;
    memset(&mapArgs, 0, sizeof mapArgs);
    mapArgs.handle = m_handle;
    if (drmIoctl(fd(), DRM_IOCTL_MODE_MAP_DUMB, &mapArgs) != 0) {
        return false;
    }
    void *address = mmap(nullptr, m_bufferSize, PROT_WRITE, MAP_SHARED, fd(), mapArgs.offset);
    if (address == MAP_FAILED) {
        return false;
    }
    m_memory = address;
    m_image = new QImage((uchar*)m_memory, m_size.width(), m_size.height(), m_stride, format);
    return !m_image->isNull();
}

}
