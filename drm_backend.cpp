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
#include "composite.h"
#include "cursor.h"
#include "logind.h"
#include "scene_qpainter.h"
#include "screens_drm.h"
#include "udev.h"
#include "utils.h"
#include "virtual_terminal.h"
#if HAVE_GBM
#include "egl_gbm_backend.h"
#endif
// Qt
#include <QSocketNotifier>
#include <QPainter>
// system
#include <unistd.h>
#include <sys/mman.h>
// drm
#include <xf86drm.h>
#include <xf86drmMode.h>
#include <libdrm/drm_mode.h>
#if HAVE_GBM
#include <gbm.h>
#endif

#ifndef DRM_CAP_CURSOR_WIDTH
#define DRM_CAP_CURSOR_WIDTH 0x8
#endif

#ifndef DRM_CAP_CURSOR_HEIGHT
#define DRM_CAP_CURSOR_HEIGHT 0x9
#endif

#include <QDebug>

namespace KWin
{

DrmBackend::DrmBackend(QObject *parent)
    : AbstractBackend(parent)
    , m_udev(new Udev)
{
    m_cursor[0] = nullptr;
    m_cursor[1] = nullptr;
}

DrmBackend::~DrmBackend()
{
    if (m_fd >= 0) {
        hideCursor();
        delete m_cursor[0];
        delete m_cursor[1];
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

void DrmBackend::pageFlipHandler(int fd, unsigned int frame, unsigned int sec, unsigned int usec, void *data)
{
    Q_UNUSED(fd)
    Q_UNUSED(frame)
    Q_UNUSED(sec)
    Q_UNUSED(usec)
    DrmBuffer *buffer = reinterpret_cast<DrmBuffer*>(data);
    buffer->m_backend->m_pageFlipPending = false;
#if HAVE_GBM
    if (buffer->m_bo) {
        gbm_surface_release_buffer(buffer->m_surface, buffer->m_bo);
        buffer->m_bo = nullptr;
    }
#endif
    Compositor::self()->bufferSwapComplete();
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
    QSocketNotifier *notifier = new QSocketNotifier(m_fd, QSocketNotifier::Read, this);
    connect(notifier, &QSocketNotifier::activated, this,
        [this] {
            drmEventContext e;
            memset(&e, 0, sizeof e);
            e.version = DRM_EVENT_CONTEXT_VERSION;
            e.page_flip_handler = pageFlipHandler;
            drmHandleEvent(m_fd, &e);
        }
    );
    m_drmId = device->sysNum();
    queryResources();
    emit screensQueried();

    initCursor();
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
        // TODO: improve
        DrmBuffer *b = new DrmBuffer(this, m_resolution);
        b->map();
        b->image()->fill(Qt::black);
        drmModeSetCrtc(m_fd, m_crtcId, b->m_bufferId, 0, 0, &m_connector, 1, &m_mode);
        // for the moment only one crtc
        break;
    }
}

void DrmBackend::present(DrmBuffer *buffer)
{
    if (!buffer || buffer->m_bufferId == 0) {
        return;
    }
    if (m_pageFlipPending) {
        return;
    }
    m_pageFlipPending = true;
    Compositor::self()->aboutToSwapBuffers();
    drmModePageFlip(m_fd, m_crtcId, buffer->m_bufferId, DRM_MODE_PAGE_FLIP_EVENT, buffer);
}

void DrmBackend::installCursorFromServer()
{
    updateCursorFromServer();
}

void DrmBackend::installCursorImage(Qt::CursorShape shape)
{
    updateCursorImage(shape);
}

void DrmBackend::initCursor()
{
    uint64_t capability = 0;
    QSize cursorSize;
    if (drmGetCap(m_fd, DRM_CAP_CURSOR_WIDTH, &capability) == 0) {
        cursorSize.setWidth(capability);
    } else {
        cursorSize.setWidth(64);
    }
    if (drmGetCap(m_fd, DRM_CAP_CURSOR_HEIGHT, &capability) == 0) {
        cursorSize.setHeight(capability);
    } else {
        cursorSize.setHeight(64);
    }
    m_cursor[0] = createBuffer(cursorSize);
    m_cursor[0]->map(QImage::Format_ARGB32_Premultiplied);
    m_cursor[0]->image()->fill(Qt::transparent);
    m_cursor[1] = createBuffer(cursorSize);
    m_cursor[1]->map(QImage::Format_ARGB32_Premultiplied);
    m_cursor[0]->image()->fill(Qt::transparent);
    // now we have screens and can set cursors, so start tracking
    connect(this, &DrmBackend::cursorChanged, this, &DrmBackend::updateCursor);
    connect(Cursor::self(), &Cursor::posChanged, this, &DrmBackend::moveCursor);
    installCursorImage(Qt::ArrowCursor);
}

void DrmBackend::setCursor()
{
    DrmBuffer *c = m_cursor[m_cursorIndex];
    m_cursorIndex = (m_cursorIndex + 1) % 2;
    drmModeSetCursor(m_fd, m_crtcId, c->m_handle, c->m_size.width(), c->m_size.height());
}

void DrmBackend::updateCursor()
{
    const QImage &cursorImage = softwareCursor();
    if (cursorImage.isNull()) {
        hideCursor();
        return;
    }
    QImage *c = m_cursor[m_cursorIndex]->image();
    c->fill(Qt::transparent);
    QPainter p;
    p.begin(c);
    p.drawImage(QPoint(0, 0), cursorImage);
    p.end();

    setCursor();
    moveCursor();
}

void DrmBackend::hideCursor()
{
    drmModeSetCursor(m_fd, m_crtcId, 0, 0, 0);
}

void DrmBackend::moveCursor()
{
    const QPoint p = Cursor::pos() - softwareCursorHotspot();
    drmModeMoveCursor(m_fd, m_crtcId, p.x(), p.y());
}

Screens *DrmBackend::createScreens(QObject *parent)
{
    return new DrmScreens(this, parent);
}

QPainterBackend *DrmBackend::createQPainterBackend()
{
    return new DrmQPainterBackend(this);
}

OpenGLBackend *DrmBackend::createOpenGLBackend()
{
#if HAVE_GBM
    return new EglGbmBackend(this);
#endif
    return AbstractBackend::createOpenGLBackend();
}

DrmBuffer *DrmBackend::createBuffer(const QSize &size)
{
    return new DrmBuffer(this, size);
}

DrmBuffer *DrmBackend::createBuffer(gbm_surface *surface)
{
#if HAVE_GBM
    return new DrmBuffer(this, surface);
#endif
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


#if HAVE_GBM
static void gbmCallback(gbm_bo *bo, void *data)
{
    Q_UNUSED(bo);
    delete reinterpret_cast<DrmBuffer*>(data);
}
#endif

DrmBuffer::DrmBuffer(DrmBackend *backend, gbm_surface *surface)
    : m_backend(backend)
    , m_surface(surface)
{
#if HAVE_GBM
    m_bo = gbm_surface_lock_front_buffer(surface);
    if (!m_bo) {
        qWarning(KWIN_CORE) << "Locking front buffer failed";
        return;
    }
    m_size = QSize(gbm_bo_get_width(m_bo), gbm_bo_get_height(m_bo));
    m_stride = gbm_bo_get_stride(m_bo);
    if (drmModeAddFB(m_backend->fd(), m_size.width(), m_size.height(), 24, 32, m_stride, gbm_bo_get_handle(m_bo).u32, &m_bufferId) != 0) {
        qWarning(KWIN_CORE) << "drmModeAddFB failed";
    }
    gbm_bo_set_user_data(m_bo, this, gbmCallback);
#endif
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
#if HAVE_GBM
    if (m_bo) {
        gbm_surface_release_buffer(m_surface, m_bo);
    }
#endif
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

}
