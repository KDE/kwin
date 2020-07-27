/*
    SPDX-FileCopyrightText: 2019 Roman Gilg <subdiff@gmail.com>
    SPDX-FileCopyrightText: 2018 Fredrik Höglund <fredrik@kde.org>

    Based on the libweston implementation,
    SPDX-FileCopyrightText: 2014, 2015 Collabora, Ltd.

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/
#include "linuxdmabuf_v1_interface.h"

#include "drm_fourcc.h"
#include "global_p.h"
#include "wayland-linux-dmabuf-unstable-v1-server-protocol.h"
#include "wayland-server-protocol.h"

#include <KWaylandServer/kwaylandserver_export.h>

#include <QVector>

#include <array>
#include <assert.h>
#include <unistd.h>

namespace KWaylandServer
{

class LinuxDmabufBuffer::Private
{
public:
    Private(LinuxDmabufBuffer *_q) : q(_q) {
        q->d = this;
    }
    virtual ~Private() = default;

    virtual uint32_t format() const = 0;
    virtual QSize size() const = 0;

    LinuxDmabufBuffer *q;
};

LinuxDmabufBuffer::LinuxDmabufBuffer()
{
}

uint32_t LinuxDmabufBuffer::format() const
{
    return d->format();
}

QSize LinuxDmabufBuffer::size() const
{
    return d->size();
}

class LinuxDmabufUnstableV1Buffer::Private : LinuxDmabufBuffer::Private
{
public:
    Private(LinuxDmabufUnstableV1Buffer *_q)
        : LinuxDmabufBuffer::Private(_q)
        , q(_q)
    {
    }
    ~Private() override = default;

    uint32_t format() const override {
        return m_format;
    }
    QSize size() const override {
        return m_size;
    }

    uint32_t m_format;
    QSize m_size;

    LinuxDmabufUnstableV1Buffer *q;
};

LinuxDmabufUnstableV1Buffer::LinuxDmabufUnstableV1Buffer(uint32_t format, const QSize &size)
    : LinuxDmabufBuffer()
    , d(new LinuxDmabufUnstableV1Buffer::Private(this))
{
    d->m_format = format;
    d->m_size = size;
}

LinuxDmabufUnstableV1Buffer::~LinuxDmabufUnstableV1Buffer()
{
}

typedef LinuxDmabufUnstableV1Interface V1Iface;

class V1Iface::Private : public Global::Private
{
public:
    Private(V1Iface *q, Display *display);
    ~Private();

    static const struct wl_buffer_interface *bufferImplementation() { return &s_bufferImplementation; }
    V1Iface::Impl *impl;
    QHash<uint32_t, QSet<uint64_t> > supportedFormatsWithModifiers;
    V1Iface * const q;
    static const uint32_t s_version;

    void bind(wl_client *client, uint32_t version, uint32_t id) override final;
    void createParams(wl_client *client, wl_resource *resource, uint32_t id);

    static void unbind(wl_client *client, wl_resource *resource);
    static void createParamsCallback(wl_client *client, wl_resource *resource, uint32_t id);

private:
    class Params
    {
    public:
        Params(V1Iface::Private *dmabufInterface, wl_client *client, uint32_t version, uint32_t id);
        ~Params();

        void postNoMemory() { wl_resource_post_no_memory(m_resource); }

        wl_resource *resource() const { return m_resource; }

        void add(int fd, uint32_t plane_idx, uint32_t offset, uint32_t stride, uint64_t modifier);
        void create(wl_client *client, uint32_t bufferId, const QSize &size, uint32_t format, uint32_t flags);

        static void destroy(wl_client *client, wl_resource *resource);
        static void add(wl_client *client, wl_resource *resource, int fd, uint32_t plane_idx, uint32_t offset, uint32_t stride, uint32_t modifier_hi, uint32_t modifier_lo);
        static void create(wl_client *client, wl_resource *resource, int width, int height, uint32_t format, uint32_t flags);
        static void createImmed(wl_client *client, wl_resource *resource, uint32_t new_id, int width, int height, uint32_t format, uint32_t flags);

    private:
        static const struct zwp_linux_buffer_params_v1_interface s_interface;

        wl_resource *m_resource;
        V1Iface::Private *m_dmabufInterface;
        std::array<V1Iface::Plane, 4> m_planes;
        size_t m_planeCount = 0;
        bool m_createRequested = false;
    };

    static const struct zwp_linux_dmabuf_v1_interface s_implementation;
    static const struct wl_buffer_interface s_bufferImplementation;
};

void V1Iface::Private::Params::create(wl_client *client, wl_resource *resource,
                               int width, int height, uint32_t format, uint32_t flags)
{
    Q_UNUSED(client)

    V1Iface::Private::Params *params = static_cast<V1Iface::Private::Params *>(wl_resource_get_user_data(resource));
    assert(params->m_resource == resource);
    params->create(client, 0, QSize(width, height), format, flags);
}

void V1Iface::Private::Params::createImmed(wl_client *client, wl_resource *resource,
                                    uint32_t new_id, int width, int height,
                                    uint32_t format, uint32_t flags)
{
    Q_UNUSED(client)

    V1Iface::Private::Params *params = static_cast<V1Iface::Private::Params *>(wl_resource_get_user_data(resource));
    assert(params->m_resource == resource);
    params->create(client, new_id, QSize(width, height), format, flags);
}

#ifndef K_DOXYGEN
const struct zwp_linux_dmabuf_v1_interface V1Iface::Private::s_implementation = {
    [](wl_client *, wl_resource *resource) { wl_resource_destroy(resource); }, // unbind
    createParamsCallback
};

const struct wl_buffer_interface V1Iface::Private::s_bufferImplementation = {
    [](wl_client *, wl_resource *resource) { wl_resource_destroy(resource); } // destroy
};

#ifndef K_DOXYGEN
const struct zwp_linux_buffer_params_v1_interface V1Iface::Private::Params::s_interface = {
    destroy,
    add,
    create,
    createImmed
};
#endif

V1Iface::Private::Params::Params(V1Iface::Private *dmabufInterface, wl_client *client, uint32_t version, uint32_t id)
    : m_dmabufInterface(dmabufInterface)
{
    m_resource = wl_resource_create(client, &zwp_linux_buffer_params_v1_interface, version, id);
    if (!m_resource) {
        return;
    }

    wl_resource_set_implementation(m_resource, &s_interface, this,
                                   [](wl_resource *resource) {
                                       delete static_cast<V1Iface::Private::Params *>(wl_resource_get_user_data(resource));
                                   });

    for (auto &plane : m_planes) {
        plane.fd = -1;
        plane.offset = 0;
        plane.stride = 0;
        plane.modifier = 0;
    }
}

V1Iface::Private::Params::~Params()
{
    // Close the file descriptors
    for (auto &plane : m_planes) {
        if (plane.fd != -1) {
            ::close(plane.fd);
        }
    }
}

void V1Iface::Private::Params::create(wl_client *client, uint32_t bufferId, const QSize &size, uint32_t format, uint32_t flags)
{
    // Validate the parameters
    // -----------------------
    const uint32_t width = size.width();
    const uint32_t height = size.height();

    if (m_createRequested) {
        wl_resource_post_error(m_resource,
                               ZWP_LINUX_BUFFER_PARAMS_V1_ERROR_ALREADY_USED,
                               "params was already used to create a wl_buffer");
        return;
    }
    m_createRequested = true;

    if (m_planeCount == 0) {
        wl_resource_post_error(m_resource,
                               ZWP_LINUX_BUFFER_PARAMS_V1_ERROR_INCOMPLETE,
                               "no dmabuf has been added to the params");
        return;
    }

    // Check for holes in the dmabufs set (e.g. [0, 1, 3])
    for (uint32_t i = 0; i < m_planeCount; i++) {
        if (m_planes[i].fd != -1)
            continue;

        wl_resource_post_error(m_resource,
                               ZWP_LINUX_BUFFER_PARAMS_V1_ERROR_INCOMPLETE,
                               "no dmabuf has been added for plane %i", i);
        return;
    }

    if (width < 1 || height < 1) {
        wl_resource_post_error(m_resource,
                               ZWP_LINUX_BUFFER_PARAMS_V1_ERROR_INVALID_DIMENSIONS,
                               "invalid width %d or height %d", width, height);
        return;
    }

    for (uint32_t i = 0; i < m_planeCount; i++) {
        auto &plane = m_planes[i];

        if (uint64_t(plane.offset) + plane.stride > UINT32_MAX) {
            wl_resource_post_error(m_resource,
                                   ZWP_LINUX_BUFFER_PARAMS_V1_ERROR_OUT_OF_BOUNDS,
                                   "size overflow for plane %i", i);
            return;
        }

        if (i == 0 && uint64_t(plane.offset) + plane.stride * height > UINT32_MAX) {
            wl_resource_post_error(m_resource,
                                   ZWP_LINUX_BUFFER_PARAMS_V1_ERROR_OUT_OF_BOUNDS,
                                   "size overflow for plane %i", i);
            return;
        }

        // Don't report an error as it might be caused by the kernel not supporting seeking on dmabuf
        off_t size = ::lseek(plane.fd, 0, SEEK_END);
        if (size == -1)
            continue;

        if (plane.offset >= size) {
            wl_resource_post_error(m_resource,
                                   ZWP_LINUX_BUFFER_PARAMS_V1_ERROR_OUT_OF_BOUNDS,
                                   "invalid offset %i for plane %i",
                                   plane.offset, i);
            return;
        }

        if (plane.offset + plane.stride > size) {
            wl_resource_post_error(m_resource,
                                   ZWP_LINUX_BUFFER_PARAMS_V1_ERROR_OUT_OF_BOUNDS,
                                   "invalid stride %i for plane %i",
                                   plane.stride, i);
            return;
        }

        // Only valid for first plane as other planes might be
        // sub-sampled according to fourcc format
        if (i == 0 && plane.offset + plane.stride * height > size) {
            wl_resource_post_error(m_resource,
                                   ZWP_LINUX_BUFFER_PARAMS_V1_ERROR_OUT_OF_BOUNDS,
                                   "invalid buffer stride or height for plane %i", i);
            return;
        }
    }

    // Import the buffer
    // -----------------
    QVector<V1Iface::Plane> planes;
    planes.reserve(m_planeCount);
    for (uint32_t i = 0; i < m_planeCount; i++)
        planes << m_planes[i];

    LinuxDmabufUnstableV1Buffer *buffer = m_dmabufInterface->impl->importBuffer(planes,
                                                                                format,
                                                                                size,
                                                                                (V1Iface::Flags) flags);
    if (buffer) {
        // The buffer has ownership of the file descriptors now
        for (auto &plane : m_planes) {
            plane.fd = -1;
        }

        wl_resource *resource = wl_resource_create(client, &wl_buffer_interface, 1, bufferId);
        if (!resource ) {
            postNoMemory();
            delete buffer;
            return;
        }

        wl_resource_set_implementation(resource, m_dmabufInterface->q->bufferImplementation(), buffer,
                                       [](wl_resource *resource) { // Destructor
                                            delete static_cast<LinuxDmabufUnstableV1Buffer *>(wl_resource_get_user_data(resource));
                                       });

        // XXX Do we need this?
        //buffer->setResource(resource);

        // Send a 'created' event when the request is not for an immediate import, i.e. bufferId is zero
        if (bufferId == 0) {
            zwp_linux_buffer_params_v1_send_created(m_resource, resource);
        }
    } else {
        if (bufferId == 0) {
            zwp_linux_buffer_params_v1_send_failed(m_resource);
        } else {
            // since the behavior is left implementation defined by the
            // protocol in case of create_immed failure due to an unknown cause,
            // we choose to treat it as a fatal error and immediately kill the
            // client instead of creating an invalid handle and waiting for it
            // to be used.
            wl_resource_post_error(m_resource,
                                   ZWP_LINUX_BUFFER_PARAMS_V1_ERROR_INVALID_WL_BUFFER,
                                   "importing the supplied dmabufs failed");
        }
    }
}

void V1Iface::Private::Params::add(int fd, uint32_t plane_idx, uint32_t offset, uint32_t stride, uint64_t modifier)
{
    if (m_createRequested) {
        wl_resource_post_error(m_resource,
                               ZWP_LINUX_BUFFER_PARAMS_V1_ERROR_ALREADY_USED,
                               "params was already used to create a wl_buffer");
        ::close(fd);
        return;
    }

    if (plane_idx >= m_planes.size()) {
        wl_resource_post_error(m_resource,
                               ZWP_LINUX_BUFFER_PARAMS_V1_ERROR_PLANE_IDX,
                               "plane index %u is too high", plane_idx);
        ::close(fd);
        return;
    }

    auto &plane = m_planes[plane_idx];

    if (plane.fd != -1) {
        wl_resource_post_error(m_resource,
                               ZWP_LINUX_BUFFER_PARAMS_V1_ERROR_PLANE_SET,
                               "a dmabuf has already been added for plane %u",
                               plane_idx);
        ::close(fd);
        return;
    }

    plane.fd = fd;
    plane.offset = offset;
    plane.stride = stride;
    plane.modifier = modifier;

    m_planeCount++;
}

void V1Iface::Private::Params::destroy(wl_client *client, wl_resource *resource)
{
    Q_UNUSED(client)
    wl_resource_destroy(resource);
}

void V1Iface::Private::Params::add(wl_client *client, wl_resource *resource,
                            int fd, uint32_t plane_idx,
                            uint32_t offset, uint32_t stride,
                            uint32_t modifier_hi, uint32_t modifier_lo)
{
    Q_UNUSED(client)

    V1Iface::Private::Params *params = static_cast<V1Iface::Private::Params *>(wl_resource_get_user_data(resource));
    assert(params->m_resource == resource);
    params->add(fd, plane_idx, offset, stride, (uint64_t(modifier_hi) << 32) | modifier_lo);
}

const uint32_t V1Iface::Private::s_version = 3;
#endif

V1Iface::Private::Private(V1Iface *q, Display *display)
    : Global::Private(display, &zwp_linux_dmabuf_v1_interface, s_version),
      q(q)
{
}

V1Iface::Private::~Private() = default;

void V1Iface::Private::bind(wl_client *client, uint32_t version, uint32_t id)
{
    wl_resource *resource = wl_resource_create(client, &zwp_linux_dmabuf_v1_interface, std::min(s_version, version), id);
    if (!resource) {
        wl_client_post_no_memory(client);
        return;
    }

    wl_resource_set_implementation(resource, &s_implementation, this, nullptr);

    // Send formats & modifiers
    // ------------------------

    QHash<uint32_t, QSet<uint64_t>>::const_iterator it = supportedFormatsWithModifiers.constBegin();
    while (it != supportedFormatsWithModifiers.constEnd()) {
        QSet<uint64_t> modifiers = it.value();
        if (modifiers.isEmpty()) {
            modifiers << DRM_FORMAT_MOD_INVALID;
        }

        for (uint64_t modifier : qAsConst(modifiers)) {
            if (version >= ZWP_LINUX_DMABUF_V1_MODIFIER_SINCE_VERSION) {
                const uint32_t modifier_lo = modifier & 0xFFFFFFFF;
                const uint32_t modifier_hi = modifier >> 32;
                zwp_linux_dmabuf_v1_send_modifier(resource, it.key(), modifier_hi, modifier_lo);
            } else if (modifier == DRM_FORMAT_MOD_LINEAR || modifier == DRM_FORMAT_MOD_INVALID) {
                zwp_linux_dmabuf_v1_send_format(resource, it.key());
            }
        }
        it++;
    }
}

void V1Iface::Private::createParams(wl_client *client, wl_resource *resource, uint32_t id)
{
    Params *params = new Params(this, client, wl_resource_get_version(resource), id);
    if (!params->resource()) {
        wl_resource_post_no_memory(resource);
        delete params;
    }
}

void V1Iface::Private::createParamsCallback(wl_client *client, wl_resource *resource, uint32_t id)
{
    V1Iface::Private *global = static_cast<V1Iface::Private *>(wl_resource_get_user_data(resource));
    global->createParams(client, resource, id);
}


V1Iface::LinuxDmabufUnstableV1Interface(Display *display, QObject *parent)
    : Global(new Private(this, display), parent)
{
}

V1Iface::~LinuxDmabufUnstableV1Interface() = default;

void V1Iface::setImpl(V1Iface::Impl *impl)
{
    d_func()->impl = impl;
}

void V1Iface::setSupportedFormatsWithModifiers(QHash<uint32_t, QSet<uint64_t> > set)
{
    d_func()->supportedFormatsWithModifiers = set;
}

const struct wl_buffer_interface *V1Iface::bufferImplementation()
{
    return V1Iface::Private::bufferImplementation();
}

V1Iface::Private *V1Iface::d_func() const
{
    return reinterpret_cast<Private*>(d.data());
}

}
