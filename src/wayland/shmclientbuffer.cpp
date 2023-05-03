/*
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#include "shmclientbuffer.h"
#include "display.h"

#include <QHash>
#include <QImage>

#include <wayland-server-core.h>
#include <wayland-server-protocol.h>

namespace KWaylandServer
{
static const ShmClientBuffer *s_accessedBuffer = nullptr;
static int s_accessCounter = 0;
static QHash<wl_resource *, ShmClientBuffer *> s_buffers;

class ShmClientBufferPrivate
{
public:
    ShmClientBufferPrivate(ShmClientBuffer *q);

    static void buffer_destroy_callback(wl_listener *listener, void *data);

    ShmClientBuffer *q;
    wl_resource *resource = nullptr;
    QImage::Format format = QImage::Format_Invalid;
    uint32_t width = 0;
    uint32_t height = 0;
    bool hasAlphaChannel = false;
    QImage savedData;

    struct DestroyListener
    {
        wl_listener listener;
        ShmClientBufferPrivate *receiver;
    };
    DestroyListener destroyListener;
};

ShmClientBufferPrivate::ShmClientBufferPrivate(ShmClientBuffer *q)
    : q(q)
{
}

static void cleanupShmPool(void *poolHandle)
{
    wl_shm_pool_unref(static_cast<wl_shm_pool *>(poolHandle));
}

void ShmClientBufferPrivate::buffer_destroy_callback(wl_listener *listener, void *data)
{
    auto bufferPrivate = reinterpret_cast<ShmClientBufferPrivate::DestroyListener *>(listener)->receiver;
    wl_shm_buffer *buffer = wl_shm_buffer_get(bufferPrivate->resource);
    wl_shm_pool *pool = wl_shm_buffer_ref_pool(buffer);

    s_buffers.remove(bufferPrivate->resource);

    wl_list_remove(&bufferPrivate->destroyListener.listener.link);
    wl_list_init(&bufferPrivate->destroyListener.listener.link);

    bufferPrivate->resource = nullptr;
    bufferPrivate->savedData = QImage(static_cast<const uchar *>(wl_shm_buffer_get_data(buffer)),
                                      bufferPrivate->width,
                                      bufferPrivate->height,
                                      wl_shm_buffer_get_stride(buffer),
                                      bufferPrivate->format,
                                      cleanupShmPool,
                                      pool);

    bufferPrivate->q->drop();
}

static bool alphaChannelFromFormat(uint32_t format)
{
    switch (format) {
    case WL_SHM_FORMAT_ABGR16161616:
    case WL_SHM_FORMAT_ABGR2101010:
    case WL_SHM_FORMAT_ARGB2101010:
    case WL_SHM_FORMAT_ARGB8888:
        return true;
    case WL_SHM_FORMAT_XBGR16161616:
    case WL_SHM_FORMAT_XBGR2101010:
    case WL_SHM_FORMAT_XRGB2101010:
    case WL_SHM_FORMAT_XRGB8888:
    default:
        return false;
    }
}

static QImage::Format imageFormatForShmFormat(uint32_t format)
{
    switch (format) {
#if Q_BYTE_ORDER == Q_LITTLE_ENDIAN
    case WL_SHM_FORMAT_ABGR16161616:
        return QImage::Format_RGBA64_Premultiplied;
    case WL_SHM_FORMAT_XBGR16161616:
        return QImage::Format_RGBX64;
    case WL_SHM_FORMAT_ARGB2101010:
        return QImage::Format_A2RGB30_Premultiplied;
    case WL_SHM_FORMAT_XRGB2101010:
        return QImage::Format_RGB30;
    case WL_SHM_FORMAT_ABGR2101010:
        return QImage::Format_A2BGR30_Premultiplied;
    case WL_SHM_FORMAT_XBGR2101010:
        return QImage::Format_BGR30;
#endif
    case WL_SHM_FORMAT_ARGB8888:
        return QImage::Format_ARGB32_Premultiplied;
    case WL_SHM_FORMAT_XRGB8888:
        return QImage::Format_RGB32;
    default:
        return QImage::Format_Invalid;
    }
}

ShmClientBuffer::ShmClientBuffer(wl_resource *resource)
    : d(std::make_unique<ShmClientBufferPrivate>(this))
{
    wl_shm_buffer *buffer = wl_shm_buffer_get(resource);
    d->resource = resource;
    d->width = wl_shm_buffer_get_width(buffer);
    d->height = wl_shm_buffer_get_height(buffer);
    d->hasAlphaChannel = alphaChannelFromFormat(wl_shm_buffer_get_format(buffer));
    d->format = imageFormatForShmFormat(wl_shm_buffer_get_format(buffer));

    d->destroyListener.receiver = d.get();
    d->destroyListener.listener.notify = ShmClientBufferPrivate::buffer_destroy_callback;
    wl_resource_add_destroy_listener(resource, &d->destroyListener.listener);

    connect(this, &GraphicsBuffer::released, [this]() {
        wl_buffer_send_release(d->resource);
    });
}

ShmClientBuffer::~ShmClientBuffer()
{
}

QSize ShmClientBuffer::size() const
{
    return QSize(d->width, d->height);
}

bool ShmClientBuffer::hasAlphaChannel() const
{
    return d->hasAlphaChannel;
}

static void cleanupShmData(void *bufferHandle)
{
    Q_ASSERT_X(s_accessCounter > 0, "cleanup", "access counter must be positive");
    s_accessCounter--;
    if (s_accessCounter == 0) {
        s_accessedBuffer = nullptr;
    }
    wl_shm_buffer_end_access(static_cast<wl_shm_buffer *>(bufferHandle));
}

QImage ShmClientBuffer::data() const
{
    if (s_accessedBuffer && s_accessedBuffer != this) {
        return QImage();
    }

    if (wl_shm_buffer *buffer = wl_shm_buffer_get(d->resource)) {
        s_accessedBuffer = this;
        s_accessCounter++;
        wl_shm_buffer_begin_access(buffer);
        const uchar *data = static_cast<const uchar *>(wl_shm_buffer_get_data(buffer));
        const uint32_t stride = wl_shm_buffer_get_stride(buffer);
        return QImage(data, d->width, d->height, stride, d->format, cleanupShmData, buffer);
    }
    return d->savedData;
}

ShmClientBuffer *ShmClientBuffer::get(wl_resource *resource)
{
    if (auto buffer = s_buffers.value(resource)) {
        return buffer;
    }

    if (wl_shm_buffer_get(resource)) {
        auto buffer = new ShmClientBuffer(resource);
        s_buffers[resource] = buffer;
        return buffer;
    }

    return nullptr;
}

ShmClientBufferIntegration::ShmClientBufferIntegration(Display *display)
    : QObject(display)
{
#if Q_BYTE_ORDER == Q_LITTLE_ENDIAN
    wl_display_add_shm_format(*display, WL_SHM_FORMAT_ARGB2101010);
    wl_display_add_shm_format(*display, WL_SHM_FORMAT_XRGB2101010);
    wl_display_add_shm_format(*display, WL_SHM_FORMAT_ABGR2101010);
    wl_display_add_shm_format(*display, WL_SHM_FORMAT_XBGR2101010);
    wl_display_add_shm_format(*display, WL_SHM_FORMAT_ABGR16161616);
    wl_display_add_shm_format(*display, WL_SHM_FORMAT_XBGR16161616);
#endif
    wl_display_init_shm(*display);
}

} // namespace KWaylandServer
