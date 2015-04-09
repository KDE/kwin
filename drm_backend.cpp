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
#include "drm_backend.h"
#include "logind.h"
#include "scene_qpainter.h"
#include "screens_drm.h"
#include "udev.h"
#include "utils.h"
#include "virtual_terminal.h"
// system
#include <unistd.h>
#include <sys/mman.h>
// drm
#include <xf86drm.h>
#include <xf86drmMode.h>
#include <libdrm/drm_mode.h>

#include <QDebug>

namespace KWin
{

DrmBackend::DrmBackend(QObject *parent)
    : AbstractBackend(parent)
    , m_udev(new Udev)
{
}

DrmBackend::~DrmBackend()
{
    if (m_fd >= 0) {
        close(m_fd);
    }
}

void DrmBackend::init()
{
    LogindIntegration *logind = LogindIntegration::self();
    auto takeControl = [logind, this]() {
        if (logind->hasSessionControl()) {
            openDrm();
        } else {
            logind->takeControl();
            connect(logind, &LogindIntegration::hasSessionControlChanged, this, &DrmBackend::openDrm);
        }
    };
    if (logind->isConnected()) {
        takeControl();
    } else {
        connect(logind, &LogindIntegration::connectedChanged, this, takeControl);
    }
    VirtualTerminal::create(this);
}

void DrmBackend::openDrm()
{
    VirtualTerminal::self()->init();
    UdevDevice::Ptr device = m_udev->primaryGpu();
    if (!device) {
        qCWarning(KWIN_CORE) << "Did not find a GPU";
        return;
    }
    int fd = LogindIntegration::self()->takeDevice(device->devNode());
    if (fd < 0) {
        qCWarning(KWIN_CORE) << "failed to open drm device at" << device->devNode();
        return;
    }
    m_fd = fd;
    m_drmId = device->sysNum();
    queryResources();
    emit screensQueried();
}

template <typename Pointer, void (*cleanupFunc)(Pointer*)>
struct DrmCleanup
{
    static inline void cleanup(Pointer *ptr)
    {
        cleanupFunc(ptr);
    }
};
template <typename T, void (*cleanupFunc)(T*)> using ScopedDrmPointer = QScopedPointer<T, DrmCleanup<T, cleanupFunc>>;

void DrmBackend::queryResources()
{
    if (m_fd < 0) {
        return;
    }
    ScopedDrmPointer<_drmModeRes, &drmModeFreeResources> resources(drmModeGetResources(m_fd));
    if (!resources) {
        qCWarning(KWIN_CORE) << "drmModeGetResources failed";
        return;
    }
    for (int i = 0; i < resources->count_connectors; ++i) {
        const auto id = resources->connectors[i];
        ScopedDrmPointer<_drmModeConnector, &drmModeFreeConnector> connector(drmModeGetConnector(m_fd, id));
        if (!connector) {
            continue;
        }
        if (connector->connection != DRM_MODE_CONNECTED) {
            continue;
        }
        ScopedDrmPointer<_drmModeEncoder, &drmModeFreeEncoder> encoder(drmModeGetEncoder(m_fd, connector->encoder_id));
        if (!encoder) {
            continue;
        }
        ScopedDrmPointer<_drmModeCrtc, &drmModeFreeCrtc> crtc(drmModeGetCrtc(m_fd, encoder->crtc_id));
        if (!crtc) {
            continue;
        }
        m_resolution = QSize(crtc->mode.hdisplay, crtc->mode.vdisplay);
        m_crtcId = encoder->crtc_id;
        m_connector = connector->connector_id;
        m_mode = crtc->mode;
        // for the moment only one crtc
        break;
    }
}

void DrmBackend::present(DrmBuffer *buffer)
{
    drmModeSetCrtc(m_fd, m_crtcId, buffer->m_bufferId, 0, 0, &m_connector, 1, &m_mode);
}

Screens *DrmBackend::createScreens(QObject *parent)
{
    return new DrmScreens(this, parent);
}

QPainterBackend *DrmBackend::createQPainterBackend()
{
    return new DrmQPainterBackend(this);
}

DrmBuffer *DrmBackend::createBuffer(const QSize &size)
{
    return new DrmBuffer(this, size);
}

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
        return;
    }
    m_handle = createArgs.handle;
    m_bufferSize = createArgs.size;
    m_stride = createArgs.pitch;
    drmModeAddFB(m_backend->fd(), size.width(), size.height(), 24, 32,
                 m_stride, createArgs.handle, &m_bufferId);
}

DrmBuffer::~DrmBuffer()
{
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
}

bool DrmBuffer::map()
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
    m_image = new QImage((uchar*)m_memory, m_size.width(), m_size.height(), m_stride, QImage::Format_RGB32);
    return !m_image->isNull();
}

}
