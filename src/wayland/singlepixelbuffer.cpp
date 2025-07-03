/*
    SPDX-FileCopyrightText: 2025 Xaver Hugl <xaver.hugl@gmail.com>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/
#include "singlepixelbuffer.h"

#include "display.h"
#include "utils/common.h"

#include <drm_fourcc.h>

namespace KWin
{

static constexpr uint32_t s_version = 1;

SinglePixelBufferManagerV1::SinglePixelBufferManagerV1(Display *display, QObject *parent)
    : QObject(parent)
    , QtWaylandServer::wp_single_pixel_buffer_manager_v1(*display, s_version)
{
}

void SinglePixelBufferManagerV1::wp_single_pixel_buffer_manager_v1_destroy(Resource *resource)
{
    wl_resource_destroy(resource->handle);
}

void SinglePixelBufferManagerV1::wp_single_pixel_buffer_manager_v1_create_u32_rgba_buffer(Resource *resource, uint32_t id, uint32_t r, uint32_t g, uint32_t b, uint32_t a)
{
    new SinglePixelClientBuffer(r, g, b, a, resource->client(), id);
}

const struct wl_buffer_interface SinglePixelClientBuffer::implementation{
    .destroy = buffer_destroy,
};

SinglePixelClientBuffer::SinglePixelClientBuffer(uint32_t r, uint32_t g, uint32_t b, uint32_t a, wl_client *client, uint32_t id)
    : m_attributes(SinglePixelAttributes{
          .red = r,
          .green = g,
          .blue = b,
          .alpha = a,
      })
    , m_argb8888(qRgba(r >> 24, g >> 24, b >> 24, a >> 24))
    , m_resource(wl_resource_create(client, &wl_buffer_interface, 1, id))
{
    connect(this, &GraphicsBuffer::released, [this]() {
        wl_buffer_send_release(m_resource);
    });
    wl_resource_set_implementation(m_resource, &implementation, this, buffer_destroy_resource);
}

GraphicsBuffer::Map SinglePixelClientBuffer::map(MapFlags flags)
{
    if (flags & GraphicsBuffer::MapFlag::Write) {
        qCWarning(KWIN_CORE, "Can't write to a single pixel buffer");
        return GraphicsBuffer::Map{
            .data = nullptr,
        };
    }
    return Map{
        .data = static_cast<void *>(&m_argb8888),
        .stride = sizeof(m_argb8888),
    };
}

QSize SinglePixelClientBuffer::size() const
{
    return QSize(1, 1);
}

bool SinglePixelClientBuffer::hasAlphaChannel() const
{
    return m_attributes.alpha != std::numeric_limits<uint32_t>::max();
}

const SinglePixelAttributes *SinglePixelClientBuffer::singlePixelAttributes() const
{
    return &m_attributes;
}

void SinglePixelClientBuffer::buffer_destroy_resource(wl_resource *resource)
{
    const auto buffer = get(resource);
    buffer->m_resource = nullptr;
    buffer->drop();
}

void SinglePixelClientBuffer::buffer_destroy(wl_client *client, wl_resource *resource)
{
    wl_resource_destroy(resource);
}

SinglePixelClientBuffer *SinglePixelClientBuffer::get(wl_resource *resource)
{
    if (wl_resource_instance_of(resource, &wl_buffer_interface, &implementation)) {
        return reinterpret_cast<SinglePixelClientBuffer *>(wl_resource_get_user_data(resource));
    } else {
        return nullptr;
    }
}
}
#include "wayland/moc_singlepixelbuffer.cpp"
