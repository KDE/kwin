/*
    SPDX-FileCopyrightText: 2014 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/
#include "buffer_interface.h"
#include "compositor_interface.h"
#include "display.h"
#include "logging.h"
#include "linuxdmabuf_v1_interface.h"
// Wayland
#include <wayland-server.h>
// EGL
#include <EGL/egl.h>
#include <QtGui/qopengl.h>

#include "drm_fourcc.h"

namespace KWaylandServer
{

namespace EGL
{
typedef GLboolean(*eglQueryWaylandBufferWL_func)(EGLDisplay dpy, struct wl_resource *buffer, EGLint attribute, EGLint *value);
eglQueryWaylandBufferWL_func eglQueryWaylandBufferWL = nullptr;
}

class BufferInterface::Private
{
public:
    Private(BufferInterface *q, Display *display, wl_resource *resource);
    ~Private();
    QImage::Format format() const;
    QImage createImage();
    wl_resource *buffer;
    wl_shm_buffer *shmBuffer;
    LinuxDmabufBuffer *dmabufBuffer;
    int refCount;
    QSize size;
    bool alpha;

    static BufferInterface *get(wl_resource *r);

private:
    static void destroyListenerCallback(wl_listener *listener, void *data);
    static Private *cast(wl_resource *r);
    static void imageBufferCleanupHandler(void *info);
    static QList<Private*> s_buffers;
    static Private *s_accessedBuffer;
    static int s_accessCounter;

    BufferInterface *q;
    wl_listener listener;
};

QList<BufferInterface::Private*> BufferInterface::Private::s_buffers;
BufferInterface::Private *BufferInterface::Private::s_accessedBuffer = nullptr;
int BufferInterface::Private::s_accessCounter = 0;

BufferInterface::Private *BufferInterface::Private::cast(wl_resource *r)
{
    auto it = std::find_if(s_buffers.constBegin(), s_buffers.constEnd(), [r](Private *d) { return d->buffer == r; });
    if (it == s_buffers.constEnd()) {
        return nullptr;
    }
    return *it;
}

BufferInterface *BufferInterface::Private::get(wl_resource *r)
{
    Private *p = cast(r);
    if (!p) {
        return nullptr;
    }
    return p->q;
}

void BufferInterface::Private::imageBufferCleanupHandler(void *info)
{
    Private *p = reinterpret_cast<Private*>(info);
    Q_ASSERT(p == s_accessedBuffer);
    Q_ASSERT(s_accessCounter > 0);
    s_accessCounter--;
    if (s_accessCounter == 0) {
        s_accessedBuffer = nullptr;
    }
    wl_shm_buffer_end_access(p->shmBuffer);
}

BufferInterface::Private::Private(BufferInterface *q, Display *display, wl_resource *resource)
    : buffer(resource)
    , shmBuffer(wl_shm_buffer_get(resource))
    , dmabufBuffer(nullptr)
    , refCount(0)
    , alpha(false)
    , q(q)
{
    if (!shmBuffer && wl_resource_instance_of(resource, &wl_buffer_interface, LinuxDmabufUnstableV1Interface::bufferImplementation())) {
        dmabufBuffer = static_cast<LinuxDmabufBuffer *>(wl_resource_get_user_data(resource));
    }
    s_buffers << this;
    listener.notify = destroyListenerCallback;
    listener.link.prev = nullptr;
    listener.link.next = nullptr;
    wl_resource_add_destroy_listener(resource, &listener);
    if (shmBuffer) {
        size = QSize(wl_shm_buffer_get_width(shmBuffer), wl_shm_buffer_get_height(shmBuffer));
        // check alpha
        switch (wl_shm_buffer_get_format(shmBuffer)) {
        case WL_SHM_FORMAT_ARGB8888:
            alpha = true;
            break;
        case WL_SHM_FORMAT_XRGB8888:
        default:
            alpha = false;
            break;
        }
    } else if (dmabufBuffer) {
        switch (dmabufBuffer->format()) {
        case DRM_FORMAT_ARGB4444:
        case DRM_FORMAT_ABGR4444:
        case DRM_FORMAT_RGBA4444:
        case DRM_FORMAT_BGRA4444:

        case DRM_FORMAT_ARGB1555:
        case DRM_FORMAT_ABGR1555:
        case DRM_FORMAT_RGBA5551:
        case DRM_FORMAT_BGRA5551:

        case DRM_FORMAT_ARGB8888:
        case DRM_FORMAT_ABGR8888:
        case DRM_FORMAT_RGBA8888:
        case DRM_FORMAT_BGRA8888:

        case DRM_FORMAT_ARGB2101010:
        case DRM_FORMAT_ABGR2101010:
        case DRM_FORMAT_RGBA1010102:
        case DRM_FORMAT_BGRA1010102:

        case DRM_FORMAT_XRGB8888_A8:
        case DRM_FORMAT_XBGR8888_A8:
        case DRM_FORMAT_RGBX8888_A8:
        case DRM_FORMAT_BGRX8888_A8:
        case DRM_FORMAT_RGB888_A8:
        case DRM_FORMAT_BGR888_A8:
        case DRM_FORMAT_RGB565_A8:
        case DRM_FORMAT_BGR565_A8:
            alpha = true;
            break;
        default:
            alpha = false;
            break;
        }
        size = dmabufBuffer->size();
    } else {
        EGLDisplay eglDisplay = display->eglDisplay();
        static bool resolved = false;
        using namespace EGL;
        if (!resolved && eglDisplay != EGL_NO_DISPLAY) {
            eglQueryWaylandBufferWL = (eglQueryWaylandBufferWL_func)eglGetProcAddress("eglQueryWaylandBufferWL");
            resolved = true;
        }
        if (eglQueryWaylandBufferWL) {
            EGLint width, height;
            bool valid = false;
            valid = eglQueryWaylandBufferWL(eglDisplay, buffer, EGL_WIDTH, &width);
            valid = valid && eglQueryWaylandBufferWL(eglDisplay, buffer, EGL_HEIGHT, &height);
            if (valid) {
                size = QSize(width, height);
            }
            // check alpha
            EGLint format;
            if (eglQueryWaylandBufferWL(eglDisplay, buffer, EGL_TEXTURE_FORMAT, &format)) {
                switch (format) {
                case EGL_TEXTURE_RGBA:
                    alpha = true;
                    break;
                case EGL_TEXTURE_RGB:
                default:
                    alpha = false;
                    break;
                }
            }
        }
    }
}

BufferInterface::Private::~Private()
{
    wl_list_remove(&listener.link);
    s_buffers.removeAll(this);
}

BufferInterface *BufferInterface::get(Display *display, wl_resource *r)
{
    if (!r) {
        return nullptr;
    }
    // TODO: verify it's a buffer
    BufferInterface *b = Private::get(r);
    if (b) {
        return b;
    }
    return new BufferInterface(display, r);
}

BufferInterface::BufferInterface(Display *display, wl_resource *resource)
    : QObject()
    , d(new Private(this, display, resource))
{
}

BufferInterface::~BufferInterface()
{
    if (d->refCount != 0) {
        qCWarning(KWAYLAND_SERVER) << "Buffer destroyed while still being referenced, ref count:" << d->refCount;
    }
}

void BufferInterface::Private::destroyListenerCallback(wl_listener *listener, void *data)
{
    Q_UNUSED(listener);
    auto b = cast(reinterpret_cast<wl_resource*>(data));
    b->buffer = nullptr;
    emit b->q->aboutToBeDestroyed(b->q);
    delete b->q;
}

void BufferInterface::ref()
{
    d->refCount++;
}

void BufferInterface::unref()
{
    Q_ASSERT(d->refCount > 0);
    d->refCount--;
    if (d->refCount == 0) {
        if (d->buffer) {
            wl_buffer_send_release(d->buffer);
            wl_client_flush(wl_resource_get_client(d->buffer));
        }
    }
}

QImage::Format BufferInterface::Private::format() const
{
    if (!shmBuffer) {
        return QImage::Format_Invalid;
    }
    switch (wl_shm_buffer_get_format(shmBuffer)) {
    case WL_SHM_FORMAT_ARGB8888:
        return QImage::Format_ARGB32_Premultiplied;
    case WL_SHM_FORMAT_XRGB8888:
        return QImage::Format_RGB32;
    default:
        return QImage::Format_Invalid;
    }
}

QImage BufferInterface::data()
{
    return d->createImage();
}

QImage BufferInterface::Private::createImage()
{
    if (!shmBuffer) {
        return QImage();
    }
    if (s_accessedBuffer != nullptr && s_accessedBuffer != this) {
        return QImage();
    }
    const QImage::Format imageFormat = format();
    if (imageFormat == QImage::Format_Invalid) {
        return QImage();
    }
    s_accessedBuffer = this;
    s_accessCounter++;
    wl_shm_buffer_begin_access(shmBuffer);
    return QImage((const uchar*)wl_shm_buffer_get_data(shmBuffer),
                  size.width(),
                  size.height(),
                  wl_shm_buffer_get_stride(shmBuffer),
                  imageFormat,
                  &imageBufferCleanupHandler, this);
}

bool BufferInterface::isReferenced() const
{
    return d->refCount > 0;
}

wl_shm_buffer *BufferInterface::shmBuffer()
{
    return d->shmBuffer;
}

LinuxDmabufBuffer *BufferInterface::linuxDmabufBuffer()
{
    return d->dmabufBuffer;
}

wl_resource *BufferInterface::resource() const
{
    return d->buffer;
}

int BufferInterface::width() const
{
    return d->size.width();
}

int BufferInterface::height() const
{
    return d->size.height();
}

QSize BufferInterface::size() const
{
    return d->size;
}

void BufferInterface::setSize(const QSize &size)
{
    if (d->shmBuffer || d->size == size) {
        return;
    }
    d->size = size;
    emit sizeChanged();
}

bool BufferInterface::hasAlphaChannel() const
{
    return d->alpha;
}

}
