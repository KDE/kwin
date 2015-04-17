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
        // wait for pageflips
        while (m_pageFlipsPending != 0) {
            QCoreApplication::processEvents(QEventLoop::WaitForMoreEvents);
        }
        qDeleteAll(m_outputs);
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
    auto v = VirtualTerminal::create(this);
    connect(v, &VirtualTerminal::activeChanged, this, &DrmBackend::activate);
}

void DrmBackend::activate(bool active)
{
    if (active) {
        reactivate();
    } else {
        deactivate();
    }
}

void DrmBackend::reactivate()
{
    if (m_active) {
        return;
    }
    m_active = true;
    DrmBuffer *c = m_cursor[(m_cursorIndex + 1) % 2];
    const QPoint cp = Cursor::pos() - softwareCursorHotspot();
    for (auto it = m_outputs.constBegin(); it != m_outputs.constEnd(); ++it) {
        DrmOutput *o = *it;
        o->pageFlipped();
        o->blank();
        o->showCursor(c);
        o->moveCursor(cp);
    }
    // restart compositor
    m_pageFlipsPending = 0;
    if (Compositor *compositor = Compositor::self()) {
        compositor->bufferSwapComplete();
        compositor->addRepaintFull();
    }
}

void DrmBackend::deactivate()
{
    if (!m_active) {
        return;
    }
    // block compositor
    if (m_pageFlipsPending == 0 && Compositor::self()) {
        Compositor::self()->aboutToSwapBuffers();
    }
    // hide cursor and disable
    for (auto it = m_outputs.constBegin(); it != m_outputs.constEnd(); ++it) {
        DrmOutput *o = *it;
        o->hideCursor();
        o->restoreSaved();
    }
    m_active = false;
}

void DrmBackend::pageFlipHandler(int fd, unsigned int frame, unsigned int sec, unsigned int usec, void *data)
{
    Q_UNUSED(fd)
    Q_UNUSED(frame)
    Q_UNUSED(sec)
    Q_UNUSED(usec)
    auto output = reinterpret_cast<DrmOutput*>(data);
    output->pageFlipped();
    output->m_backend->m_pageFlipsPending--;
    if (output->m_backend->m_pageFlipsPending == 0) {
        // TODO: improve, this currently means we wait for all page flips or all outputs.
        // It would be better to driver the repaint per output
        if (Compositor::self()) {
            Compositor::self()->bufferSwapComplete();
        }
    }
}

void DrmBackend::openDrm()
{
    connect(LogindIntegration::self(), &LogindIntegration::sessionActiveChanged, this, &DrmBackend::activate);
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
    m_active = true;
    QSocketNotifier *notifier = new QSocketNotifier(m_fd, QSocketNotifier::Read, this);
    connect(notifier, &QSocketNotifier::activated, this,
        [this] {
            if (!VirtualTerminal::self()->isActive()) {
                return;
            }
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
        DrmOutput *drmOutput = new DrmOutput(this);
        drmOutput->m_crtcId = encoder->crtc_id;
        drmOutput->m_mode = crtc->mode;
        drmOutput->m_connector = connector->connector_id;
        drmOutput->init();
        m_outputs << drmOutput;
    }
    // TODO: install global space
}

void DrmBackend::present(DrmBuffer *buffer)
{
    if (m_outputs.isEmpty()) {
        return;
    }
    // TODO: correct output
    if (m_outputs.first()->present(buffer)) {
        m_pageFlipsPending++;
        if (m_pageFlipsPending == 1 && Compositor::self()) {
            Compositor::self()->aboutToSwapBuffers();
        }
    }
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
    for (auto it = m_outputs.constBegin(); it != m_outputs.constEnd(); ++it) {
        (*it)->showCursor(c);
    }
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
    for (auto it = m_outputs.constBegin(); it != m_outputs.constEnd(); ++it) {
        (*it)->hideCursor();
    }
}

void DrmBackend::moveCursor()
{
    const QPoint p = Cursor::pos() - softwareCursorHotspot();
    for (auto it = m_outputs.constBegin(); it != m_outputs.constEnd(); ++it) {
        (*it)->moveCursor(p);
    }
}

QSize DrmBackend::size() const
{
    if (m_outputs.isEmpty()) {
        return QSize();
    }
    return m_outputs.first()->size();
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

DrmOutput::DrmOutput(DrmBackend *backend)
    : m_backend(backend)
{
}

DrmOutput::~DrmOutput()
{
    hideCursor();
    cleanupBlackBuffer();
}

void DrmOutput::hideCursor()
{
    drmModeSetCursor(m_backend->fd(), m_crtcId, 0, 0, 0);
}

void DrmOutput::restoreSaved()
{
    if (!m_savedCrtc.isNull()) {
        drmModeSetCrtc(m_backend->fd(), m_savedCrtc->crtc_id, m_savedCrtc->buffer_id,
                       m_savedCrtc->x, m_savedCrtc->y, &m_connector, 1, &m_savedCrtc->mode);
    }
}

void DrmOutput::showCursor(DrmBuffer *c)
{
    const QSize &s = c->size();
    drmModeSetCursor(m_backend->fd(), m_crtcId, c->handle(), s.width(), s.height());
}

void DrmOutput::moveCursor(const QPoint &globalPos)
{
    const QPoint p = globalPos - m_globalPos;
    drmModeMoveCursor(m_backend->fd(), m_crtcId, p.x(), p.y());
}

QSize DrmOutput::size() const
{
    return QSize(m_mode.hdisplay, m_mode.vdisplay);
}

QRect DrmOutput::geometry() const
{
    return QRect(m_globalPos, size());
}

bool DrmOutput::present(DrmBuffer *buffer)
{
    if (!buffer || buffer->bufferId() == 0) {
        return false;
    }
    if (!VirtualTerminal::self()->isActive()) {
        m_currentBuffer = buffer;
        return false;
    }
    if (m_currentBuffer) {
        return false;
    }
    if (m_lastStride != buffer->stride()) {
        // need to set a new mode first
        if (!setMode(buffer)) {
            return false;
        }
    }
    const bool ok = drmModePageFlip(m_backend->fd(), m_crtcId, buffer->bufferId(), DRM_MODE_PAGE_FLIP_EVENT, this) == 0;
    if (ok) {
        m_currentBuffer = buffer;
    } else {
        qWarning(KWIN_CORE) << "Page flip failed";
        buffer->releaseGbm();
    }
    return ok;
}

void DrmOutput::pageFlipped()
{
    if (!m_currentBuffer) {
        return;
    }
    m_currentBuffer->releaseGbm();
    m_currentBuffer = nullptr;
    cleanupBlackBuffer();
}

void DrmOutput::cleanupBlackBuffer()
{
    if (m_blackBuffer) {
        delete m_blackBuffer;
        m_blackBuffer = nullptr;
    }
}

void DrmOutput::init()
{
    m_savedCrtc.reset(drmModeGetCrtc(m_backend->fd(), m_crtcId));
    blank();
}

void DrmOutput::blank()
{
    if (!m_blackBuffer) {
        m_blackBuffer = m_backend->createBuffer(size());
        m_blackBuffer->map();
        m_blackBuffer->image()->fill(Qt::black);
    }
    setMode(m_blackBuffer);
}

bool DrmOutput::setMode(DrmBuffer *buffer)
{
    if (drmModeSetCrtc(m_backend->fd(), m_crtcId, buffer->bufferId(), 0, 0, &m_connector, 1, &m_mode) == 0) {
        m_lastStride = buffer->stride();
        return true;
    } else {
        qCWarning(KWIN_CORE) << "Mode setting failed";
        return false;
    }
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
