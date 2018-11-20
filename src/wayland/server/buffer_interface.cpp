/********************************************************************
Copyright 2014  Martin Gräßlin <mgraesslin@kde.org>

This library is free software; you can redistribute it and/or
modify it under the terms of the GNU Lesser General Public
License as published by the Free Software Foundation; either
version 2.1 of the License, or (at your option) version 3, or any
later version accepted by the membership of KDE e.V. (or its
successor approved by the membership of KDE e.V.), which shall
act as a proxy defined in Section 6 of version 3 of the license.

This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public
License along with this library.  If not, see <http://www.gnu.org/licenses/>.
*********************************************************************/
#include "buffer_interface.h"
#include "display.h"
#include "logging.h"
#include "surface_interface.h"
// Wayland
#include <wayland-server.h>
// EGL
#include <EGL/egl.h>
#include <QtGui/qopengl.h>

namespace KWayland
{
namespace Server
{

namespace EGL
{
typedef GLboolean(*eglQueryWaylandBufferWL_func)(EGLDisplay dpy, struct wl_resource *buffer, EGLint attribute, EGLint *value);
eglQueryWaylandBufferWL_func eglQueryWaylandBufferWL = nullptr;
}

class BufferInterface::Private
{
public:
    Private(BufferInterface *q, wl_resource *resource, SurfaceInterface *parent);
    ~Private();
    QImage::Format format() const;
    QImage createImage();
    wl_resource *buffer;
    wl_shm_buffer *shmBuffer;
    SurfaceInterface *surface;
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

BufferInterface::Private::Private(BufferInterface *q, wl_resource *resource, SurfaceInterface *parent)
    : buffer(resource)
    , shmBuffer(wl_shm_buffer_get(resource))
    , surface(parent)
    , refCount(0)
    , alpha(false)
    , q(q)
{
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
    } else if (parent) {
        EGLDisplay eglDisplay = parent->global()->display()->eglDisplay();
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

BufferInterface *BufferInterface::get(wl_resource *r)
{
    if (!r) {
        return nullptr;
    }
    // TODO: verify it's a buffer
    BufferInterface *b = Private::get(r);
    if (b) {
        return b;
    }
    return new BufferInterface(r, nullptr);
}

BufferInterface::BufferInterface(wl_resource *resource, SurfaceInterface *parent)
    : QObject()
    , d(new Private(this, resource, parent))
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
        deleteLater();
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
    return std::move(d->createImage());
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
    return std::move(QImage((const uchar*)wl_shm_buffer_get_data(shmBuffer),
                            size.width(),
                            size.height(),
                            wl_shm_buffer_get_stride(shmBuffer),
                            imageFormat,
                            &imageBufferCleanupHandler, this));
}

bool BufferInterface::isReferenced() const
{
    return d->refCount > 0;
}

SurfaceInterface *BufferInterface::surface() const
{
    return d->surface;
}

wl_shm_buffer *BufferInterface::shmBuffer()
{
    return d->shmBuffer;
}

wl_resource *BufferInterface::resource() const
{
    return d->buffer;
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
}
