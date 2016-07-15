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
#include "drm_backend.h"

#include "logging.h"

// system
#include <sys/mman.h>
#include <errno.h>
// drm
#include <xf86drm.h>
#if HAVE_GBM
#include <gbm.h>
#endif

namespace KWin
{


DrmBuffer::DrmBuffer(DrmBackend *backend, const QSize &size)
    : m_backend(backend)
    , m_size(size)
{
    drm_mode_create_dumb createArgs;
    memset(&createArgs, 0, sizeof createArgs);
    createArgs.bpp = 32;
    createArgs.width = size.width();
    createArgs.height = size.height();
    if (drmIoctl(m_backend->fd(), DRM_IOCTL_MODE_CREATE_DUMB, &createArgs) != 0) {
        qCWarning(KWIN_DRM) << "DRM_IOCTL_MODE_CREATE_DUMB failed";
        return;
    }
    m_handle = createArgs.handle;
    m_bufferSize = createArgs.size;
    m_stride = createArgs.pitch;
    if (drmModeAddFB(m_backend->fd(), size.width(), size.height(), 24, 32,
                     m_stride, createArgs.handle, &m_bufferId) != 0) {
        qCWarning(KWIN_DRM) << "drmModeAddFB failed with errno" << errno;
    }
}


#if HAVE_GBM
static void gbmCallback(gbm_bo *bo, void *data)
{
    DrmBackend *backend = reinterpret_cast<DrmBackend*>(data);
    const auto &buffers = backend->buffers();
    for (auto buffer: buffers) {
        if (buffer->gbm() == bo) {
            delete buffer;
            return;
        }
    }
}
#endif

DrmBuffer::DrmBuffer(DrmBackend *backend, gbm_surface *surface)
    : m_backend(backend)
    , m_surface(surface)
{
#if HAVE_GBM
    m_bo = gbm_surface_lock_front_buffer(surface);
    if (!m_bo) {
        qCWarning(KWIN_DRM) << "Locking front buffer failed";
        return;
    }
    m_size = QSize(gbm_bo_get_width(m_bo), gbm_bo_get_height(m_bo));
    m_stride = gbm_bo_get_stride(m_bo);
    if (drmModeAddFB(m_backend->fd(), m_size.width(), m_size.height(), 24, 32, m_stride, gbm_bo_get_handle(m_bo).u32, &m_bufferId) != 0) {
        qCWarning(KWIN_DRM) << "drmModeAddFB failed";
    }
    gbm_bo_set_user_data(m_bo, m_backend, gbmCallback);
#endif
}

DrmBuffer::~DrmBuffer()
{
    m_backend->bufferDestroyed(this);
    delete m_image;
    if (m_memory) {
        munmap(m_memory, m_bufferSize);
    }
    if (m_bufferId) {
        drmModeRmFB(m_backend->fd(), m_bufferId);
    }
    if (m_handle) {
        drm_mode_destroy_dumb destroyArgs;
        destroyArgs.handle = m_handle;
        drmIoctl(m_backend->fd(), DRM_IOCTL_MODE_DESTROY_DUMB, &destroyArgs);
    }
    releaseGbm();
}

bool DrmBuffer::map(QImage::Format format)
{
    if (!m_handle || !m_bufferId) {
        return false;
    }
    drm_mode_map_dumb mapArgs;
    memset(&mapArgs, 0, sizeof mapArgs);
    mapArgs.handle = m_handle;
    if (drmIoctl(m_backend->fd(), DRM_IOCTL_MODE_MAP_DUMB, &mapArgs) != 0) {
        return false;
    }
    void *address = mmap(nullptr, m_bufferSize, PROT_WRITE, MAP_SHARED, m_backend->fd(), mapArgs.offset);
    if (address == MAP_FAILED) {
        return false;
    }
    m_memory = address;
    m_image = new QImage((uchar*)m_memory, m_size.width(), m_size.height(), m_stride, format);
    return !m_image->isNull();
}

void DrmBuffer::releaseGbm()
{
#if HAVE_GBM
    if (m_bo) {
        gbm_surface_release_buffer(m_surface, m_bo);
        m_bo = nullptr;
    }
#endif
}

}
