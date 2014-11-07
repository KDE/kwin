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
#include "surface_interface.h"
// Wayland
#include <wayland-server.h>

namespace KWayland
{
namespace Server
{

class BufferInterface::Private
{
public:
    Private(BufferInterface *q, wl_resource *resource, SurfaceInterface *parent);
    ~Private();
    QImage::Format format() const;
    void createImage();
    void releaseImage();
    wl_resource *buffer;
    wl_shm_buffer *shmBuffer;
    SurfaceInterface *surface;
    int refCount;
    QImage image;

private:
    static void destroyListenerCallback(wl_listener *listener, void *data);
    static Private *cast(wl_resource *r);
    static QList<Private*> s_buffers;

    BufferInterface *q;
    wl_listener listener;
};

QList<BufferInterface::Private*> BufferInterface::Private::s_buffers;

BufferInterface::Private *BufferInterface::Private::cast(wl_resource *r)
{
    auto it = std::find_if(s_buffers.constBegin(), s_buffers.constEnd(), [r](Private *d) { return d->buffer == r; });
    if (it == s_buffers.constEnd()) {
        return nullptr;
    }
    return *it;
}

BufferInterface::Private::Private(BufferInterface *q, wl_resource *resource, SurfaceInterface *parent)
    : buffer(resource)
    , shmBuffer(wl_shm_buffer_get(resource))
    , surface(parent)
    , refCount(0)
    , q(q)
{
    s_buffers << this;
    listener.notify = destroyListenerCallback;
    wl_resource_add_destroy_listener(resource, &listener);
}

BufferInterface::Private::~Private()
{
    s_buffers.removeAll(this);
}

BufferInterface::BufferInterface(wl_resource *resource, SurfaceInterface *parent)
    : QObject(parent)
    , d(new Private(this, resource, parent))
{
}

BufferInterface::~BufferInterface()
{
    Q_ASSERT(d->refCount == 0);
    d->releaseImage();
}

void BufferInterface::Private::releaseImage()
{
    if (image.isNull()) {
        return;
    }
    // first destroy it
    image = QImage();
    wl_shm_buffer_end_access(shmBuffer);
}

void BufferInterface::Private::destroyListenerCallback(wl_listener *listener, void *data)
{
    Q_UNUSED(listener);
    auto b = cast(reinterpret_cast<wl_resource*>(data));
    b->buffer = nullptr;
    emit b->q->aboutToBeDestroyed(b->q);
    b->q->deleteLater();
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
        d->releaseImage();
        if (d->buffer) {
            wl_buffer_send_release(d->buffer);
        }
        deleteLater();
    }
}

QImage::Format BufferInterface::Private::format() const
{
    switch (wl_shm_buffer_get_format(shmBuffer)) {
    case WL_SHM_FORMAT_ARGB8888:
        return QImage::Format_ARGB32;
    case WL_SHM_FORMAT_XRGB8888:
        return QImage::Format_RGB32;
    default:
        return QImage::Format_Invalid;
    }
}

QImage BufferInterface::data()
{
    if (d->image.isNull()) {
        d->createImage();
    }
    return d->image;
}

void BufferInterface::Private::createImage()
{
    if (!shmBuffer) {
        return;
    }
    const QImage::Format imageFormat = format();
    if (imageFormat == QImage::Format_Invalid) {
        return;
    }
    wl_shm_buffer_begin_access(shmBuffer);
    image = QImage((const uchar*)wl_shm_buffer_get_data(shmBuffer),
                   wl_shm_buffer_get_width(shmBuffer),
                   wl_shm_buffer_get_height(shmBuffer),
                   wl_shm_buffer_get_stride(shmBuffer),
                   imageFormat);
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

}
}
