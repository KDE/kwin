/*
    SPDX-FileCopyrightText: 2018 Fredrik Höglund <fredrik@kde.org>
    SPDX-FileCopyrightText: 2019 Roman Gilg <subdiff@gmail.com>
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>
    SPDX-FileCopyrightText: 2021 Xaver Hugl <xaver.hugl@gmail.com>

    Based on the libweston implementation,
    SPDX-FileCopyrightText: 2014, 2015 Collabora, Ltd.

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#include "linuxdmabufv1clientbuffer.h"
#include "linuxdmabufv1clientbuffer_p.h"
#include "surface_interface_p.h"
#include "utils/common.h"

#include <errno.h>
#include <fcntl.h>
#include <unistd.h>

namespace KWaylandServer
{
static const int s_version = 4;

LinuxDmaBufV1ClientBufferIntegrationPrivate::LinuxDmaBufV1ClientBufferIntegrationPrivate(LinuxDmaBufV1ClientBufferIntegration *q, Display *display)
    : QtWaylandServer::zwp_linux_dmabuf_v1(*display, s_version)
    , q(q)
    , defaultFeedback(new LinuxDmaBufV1Feedback(this))
{
}

void LinuxDmaBufV1ClientBufferIntegrationPrivate::zwp_linux_dmabuf_v1_bind_resource(Resource *resource)
{
    if (resource->version() < ZWP_LINUX_DMABUF_V1_GET_DEFAULT_FEEDBACK_SINCE_VERSION) {
        for (auto it = supportedModifiers.constBegin(); it != supportedModifiers.constEnd(); ++it) {
            const uint32_t &format = it.key();
            const auto &modifiers = it.value();
            for (const uint64_t &modifier : std::as_const(modifiers)) {
                if (resource->version() >= ZWP_LINUX_DMABUF_V1_MODIFIER_SINCE_VERSION) {
                    const uint32_t modifier_lo = modifier & 0xffffffff;
                    const uint32_t modifier_hi = modifier >> 32;
                    send_modifier(resource->handle, format, modifier_hi, modifier_lo);
                } else if (modifier == DRM_FORMAT_MOD_LINEAR || modifier == DRM_FORMAT_MOD_INVALID) {
                    send_format(resource->handle, format);
                }
            }
        }
    }
}

void LinuxDmaBufV1ClientBufferIntegrationPrivate::zwp_linux_dmabuf_v1_get_default_feedback(Resource *resource, uint32_t id)
{
    LinuxDmaBufV1FeedbackPrivate::get(defaultFeedback.get())->add(resource->client(), id, resource->version());
}

void LinuxDmaBufV1ClientBufferIntegrationPrivate::zwp_linux_dmabuf_v1_get_surface_feedback(Resource *resource, uint32_t id, wl_resource *surfaceResource)
{
    auto surface = SurfaceInterface::get(surfaceResource);
    if (!surface) {
        qCWarning(KWIN_CORE) << "requested surface feedback for nonexistant surface!";
        return;
    }
    auto surfacePrivate = SurfaceInterfacePrivate::get(surface);
    if (!surfacePrivate->dmabufFeedbackV1) {
        surfacePrivate->dmabufFeedbackV1.reset(new LinuxDmaBufV1Feedback(this));
    }
    LinuxDmaBufV1FeedbackPrivate::get(surfacePrivate->dmabufFeedbackV1.get())->add(resource->client(), id, resource->version());
}

void LinuxDmaBufV1ClientBufferIntegrationPrivate::zwp_linux_dmabuf_v1_destroy(Resource *resource)
{
    wl_resource_destroy(resource->handle);
}

void LinuxDmaBufV1ClientBufferIntegrationPrivate::zwp_linux_dmabuf_v1_create_params(Resource *resource, uint32_t params_id)
{
    wl_resource *paramsResource = wl_resource_create(resource->client(), &zwp_linux_buffer_params_v1_interface, resource->version(), params_id);
    if (!paramsResource) {
        wl_resource_post_no_memory(resource->handle);
        return;
    }
    new LinuxDmaBufParamsV1(q, paramsResource);
}

LinuxDmaBufParamsV1::LinuxDmaBufParamsV1(LinuxDmaBufV1ClientBufferIntegration *integration, ::wl_resource *resource)
    : QtWaylandServer::zwp_linux_buffer_params_v1(resource)
    , m_integration(integration)
{
}

void LinuxDmaBufParamsV1::zwp_linux_buffer_params_v1_destroy_resource(Resource *resource)
{
    delete this;
}

void LinuxDmaBufParamsV1::zwp_linux_buffer_params_v1_destroy(Resource *resource)
{
    wl_resource_destroy(resource->handle);
}

void LinuxDmaBufParamsV1::zwp_linux_buffer_params_v1_add(Resource *resource,
                                                         int32_t fd,
                                                         uint32_t plane_idx,
                                                         uint32_t offset,
                                                         uint32_t stride,
                                                         uint32_t modifier_hi,
                                                         uint32_t modifier_lo)
{
    if (Q_UNLIKELY(m_isUsed)) {
        wl_resource_post_error(resource->handle, error_already_used, "the params object has already been used to create a wl_buffer");
        close(fd);
        return;
    }

    if (Q_UNLIKELY(plane_idx >= 4)) {
        wl_resource_post_error(resource->handle, error_plane_idx, "plane index %d is out of bounds", plane_idx);
        close(fd);
        return;
    }

    if (Q_UNLIKELY(m_attrs.fd[plane_idx].isValid())) {
        wl_resource_post_error(resource->handle, error_plane_set, "the plane index %d was already set", plane_idx);
        close(fd);
        return;
    }
    m_attrs.fd[plane_idx] = KWin::FileDescriptor{fd};
    m_attrs.offset[plane_idx] = offset;
    m_attrs.pitch[plane_idx] = stride;
    m_attrs.modifier = (quint64(modifier_hi) << 32) | modifier_lo;
    m_attrs.planeCount++;
}

void LinuxDmaBufParamsV1::zwp_linux_buffer_params_v1_create(Resource *resource, int32_t width, int32_t height, uint32_t format, uint32_t flags)
{
    if (Q_UNLIKELY(m_isUsed)) {
        wl_resource_post_error(resource->handle, error_already_used, "the params object has already been used to create a wl_buffer");
        return;
    }

    if (Q_UNLIKELY(!test(resource, width, height))) {
        return;
    }

    m_isUsed = true;

    m_attrs.width = width;
    m_attrs.height = height;
    m_attrs.format = format;

    LinuxDmaBufV1ClientBuffer *clientBuffer = m_integration->rendererInterface()->importBuffer(std::move(m_attrs), flags);
    if (!clientBuffer) {
        send_failed(resource->handle);
        return;
    }

    wl_resource *bufferResource = wl_resource_create(resource->client(), &wl_buffer_interface, 1, 0);
    if (!bufferResource) {
        delete clientBuffer;
        wl_resource_post_no_memory(resource->handle);
        return;
    }

    clientBuffer->initialize(bufferResource);
    send_created(resource->handle, bufferResource);

    DisplayPrivate *displayPrivate = DisplayPrivate::get(m_integration->display());
    displayPrivate->registerClientBuffer(clientBuffer);
}

void LinuxDmaBufParamsV1::zwp_linux_buffer_params_v1_create_immed(Resource *resource,
                                                                  uint32_t buffer_id,
                                                                  int32_t width,
                                                                  int32_t height,
                                                                  uint32_t format,
                                                                  uint32_t flags)
{
    if (Q_UNLIKELY(m_isUsed)) {
        wl_resource_post_error(resource->handle, error_already_used, "the params object has already been used to create a wl_buffer");
        return;
    }

    if (Q_UNLIKELY(!test(resource, width, height))) {
        return;
    }

    m_isUsed = true;

    m_attrs.width = width;
    m_attrs.height = height;
    m_attrs.format = format;

    LinuxDmaBufV1ClientBuffer *clientBuffer = m_integration->rendererInterface()->importBuffer(std::move(m_attrs), flags);
    if (!clientBuffer) {
        wl_resource_post_error(resource->handle, error_invalid_wl_buffer, "importing the supplied dmabufs failed");
        return;
    }

    wl_resource *bufferResource = wl_resource_create(resource->client(), &wl_buffer_interface, 1, buffer_id);
    if (!bufferResource) {
        delete clientBuffer;
        wl_resource_post_no_memory(resource->handle);
        return;
    }

    clientBuffer->initialize(bufferResource);

    DisplayPrivate *displayPrivate = DisplayPrivate::get(m_integration->display());
    displayPrivate->registerClientBuffer(clientBuffer);
}

bool LinuxDmaBufParamsV1::test(Resource *resource, uint32_t width, uint32_t height)
{
    if (Q_UNLIKELY(!m_attrs.planeCount)) {
        wl_resource_post_error(resource->handle, error_incomplete, "no planes have been specified");
        return false;
    }

    // Check for holes in the dmabuf set (e.g. [0, 1, 3]).
    for (int i = 0; i < m_attrs.planeCount; ++i) {
        if (!m_attrs.fd[i].isValid()) {
            wl_resource_post_error(resource->handle, error_incomplete, "no dmabuf has been added for plane %d", i);
            return false;
        }
    }

    if (Q_UNLIKELY(width == 0 || height == 0)) {
        wl_resource_post_error(resource->handle, error_invalid_dimensions, "invalid width %d or height %d", width, height);
        return false;
    }

    for (int i = 0; i < m_attrs.planeCount; ++i) {
        // Check for overflows.
        if (Q_UNLIKELY(uint64_t(m_attrs.offset[i]) + m_attrs.pitch[i] > UINT32_MAX)) {
            wl_resource_post_error(resource->handle, error_out_of_bounds, "size overflow for plane %d", i);
            return false;
        }

        if (Q_UNLIKELY(i == 0 && uint64_t(m_attrs.offset[i]) + uint64_t(m_attrs.pitch[i]) * height > UINT32_MAX)) {
            wl_resource_post_error(resource->handle, error_out_of_bounds, "size overflow for plane %d", i);
            return false;
        }

        // Don't report an error as it might be caused by the kernel not supporting
        // seeking on dmabuf.
        const off_t size = lseek(m_attrs.fd[i].get(), 0, SEEK_END);
        if (size == -1) {
            continue;
        }

        if (Q_UNLIKELY(m_attrs.offset[i] >= size)) {
            wl_resource_post_error(resource->handle, error_out_of_bounds, "invalid offset %i for plane %d", m_attrs.offset[i], i);
            return false;
        }

        if (Q_UNLIKELY(m_attrs.offset[i] + m_attrs.pitch[i] > size)) {
            wl_resource_post_error(resource->handle, error_out_of_bounds, "invalid stride %i for plane %d", m_attrs.pitch[i], i);
            return false;
        }

        // Only valid for first plane as other planes might be sub-sampled according to
        // fourcc format.
        if (Q_UNLIKELY(i == 0 && m_attrs.offset[i] + m_attrs.pitch[i] * height > size)) {
            wl_resource_post_error(resource->handle, error_out_of_bounds, "invalid buffer stride of height for plane %d", i);
            return false;
        }
    }

    return true;
}

LinuxDmaBufV1ClientBufferIntegration::LinuxDmaBufV1ClientBufferIntegration(Display *display)
    : ClientBufferIntegration(display)
    , d(new LinuxDmaBufV1ClientBufferIntegrationPrivate(this, display))
{
}

LinuxDmaBufV1ClientBufferIntegration::~LinuxDmaBufV1ClientBufferIntegration()
{
}

LinuxDmaBufV1ClientBufferIntegration::RendererInterface *LinuxDmaBufV1ClientBufferIntegration::rendererInterface() const
{
    return d->rendererInterface;
}

void LinuxDmaBufV1ClientBufferIntegration::setRendererInterface(RendererInterface *rendererInterface)
{
    d->rendererInterface = rendererInterface;
}

bool operator==(const LinuxDmaBufV1Feedback::Tranche &t1, const LinuxDmaBufV1Feedback::Tranche &t2)
{
    return t1.device == t2.device && t1.flags == t2.flags && t1.formatTable == t2.formatTable;
}

void LinuxDmaBufV1ClientBufferIntegration::setSupportedFormatsWithModifiers(const QVector<LinuxDmaBufV1Feedback::Tranche> &tranches)
{
    if (LinuxDmaBufV1FeedbackPrivate::get(d->defaultFeedback.get())->m_tranches != tranches) {
        QHash<uint32_t, QVector<uint64_t>> set;
        for (const auto &tranche : tranches) {
            set.insert(tranche.formatTable);
        }
        d->supportedModifiers = set;
        d->mainDevice = tranches.first().device;
        d->table.reset(new LinuxDmaBufV1FormatTable(set));
        d->defaultFeedback->setTranches(tranches);
    }
}

static bool testAlphaChannel(uint32_t drmFormat)
{
    switch (drmFormat) {
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
        return true;
    default:
        return false;
    }
}

void LinuxDmaBufV1ClientBufferPrivate::buffer_destroy(Resource *resource)
{
    wl_resource_destroy(resource->handle);
}

LinuxDmaBufV1ClientBuffer::LinuxDmaBufV1ClientBuffer(KWin::DmaBufAttributes &&attrs, quint32 flags)
    : ClientBuffer(*new LinuxDmaBufV1ClientBufferPrivate)
{
    Q_D(LinuxDmaBufV1ClientBuffer);
    d->attrs = std::move(attrs);
    d->flags = flags;
    d->hasAlphaChannel = testAlphaChannel(attrs.format);
}

LinuxDmaBufV1ClientBuffer::~LinuxDmaBufV1ClientBuffer() = default;

void LinuxDmaBufV1ClientBuffer::initialize(wl_resource *resource)
{
    Q_D(LinuxDmaBufV1ClientBuffer);
    d->init(resource);
    ClientBuffer::initialize(resource);
}

quint32 LinuxDmaBufV1ClientBuffer::format() const
{
    Q_D(const LinuxDmaBufV1ClientBuffer);
    return d->attrs.format;
}

quint32 LinuxDmaBufV1ClientBuffer::flags() const
{
    Q_D(const LinuxDmaBufV1ClientBuffer);
    return d->flags;
}

const KWin::DmaBufAttributes &LinuxDmaBufV1ClientBuffer::attributes() const
{
    Q_D(const LinuxDmaBufV1ClientBuffer);
    return d->attrs;
}

QSize LinuxDmaBufV1ClientBuffer::size() const
{
    Q_D(const LinuxDmaBufV1ClientBuffer);
    return QSize(d->attrs.width, d->attrs.height);
}

bool LinuxDmaBufV1ClientBuffer::hasAlphaChannel() const
{
    Q_D(const LinuxDmaBufV1ClientBuffer);
    return d->hasAlphaChannel;
}

ClientBuffer::Origin LinuxDmaBufV1ClientBuffer::origin() const
{
    Q_D(const LinuxDmaBufV1ClientBuffer);
    if (d->flags & QtWaylandServer::zwp_linux_buffer_params_v1::flags_y_invert) {
        return ClientBuffer::Origin::BottomLeft;
    } else {
        return ClientBuffer::Origin::TopLeft;
    }
}

LinuxDmaBufV1Feedback::LinuxDmaBufV1Feedback(LinuxDmaBufV1ClientBufferIntegrationPrivate *integration)
    : d(new LinuxDmaBufV1FeedbackPrivate(integration))
{
}

LinuxDmaBufV1Feedback::~LinuxDmaBufV1Feedback() = default;

void LinuxDmaBufV1Feedback::setTranches(const QVector<Tranche> &tranches)
{
    if (d->m_tranches != tranches) {
        d->m_tranches = tranches;
        const auto &map = d->resourceMap();
        for (const auto &resource : map) {
            d->send(resource);
        }
    }
}

LinuxDmaBufV1FeedbackPrivate *LinuxDmaBufV1FeedbackPrivate::get(LinuxDmaBufV1Feedback *q)
{
    return q->d.get();
}

LinuxDmaBufV1FeedbackPrivate::LinuxDmaBufV1FeedbackPrivate(LinuxDmaBufV1ClientBufferIntegrationPrivate *bufferintegration)
    : m_bufferintegration(bufferintegration)
{
}

void LinuxDmaBufV1FeedbackPrivate::send(Resource *resource)
{
    send_format_table(resource->handle, m_bufferintegration->table->file.fd(), m_bufferintegration->table->file.size());
    QByteArray bytes;
    bytes.append(reinterpret_cast<const char *>(&m_bufferintegration->mainDevice), sizeof(dev_t));
    send_main_device(resource->handle, bytes);
    const auto &sendTranche = [this, resource](const LinuxDmaBufV1Feedback::Tranche &tranche) {
        QByteArray targetDevice;
        targetDevice.append(reinterpret_cast<const char *>(&tranche.device), sizeof(dev_t));
        QByteArray indices;
        for (auto it = tranche.formatTable.begin(); it != tranche.formatTable.end(); it++) {
            const uint32_t format = it.key();
            for (const auto &mod : std::as_const(it.value())) {
                uint16_t index = m_bufferintegration->table->indices[std::pair<uint32_t, uint64_t>(format, mod)];
                indices.append(reinterpret_cast<const char *>(&index), 2);
            }
        }
        send_tranche_target_device(resource->handle, targetDevice);
        send_tranche_formats(resource->handle, indices);
        send_tranche_flags(resource->handle, static_cast<uint32_t>(tranche.flags));
        send_tranche_done(resource->handle);
    };
    for (const auto &tranche : std::as_const(m_tranches)) {
        sendTranche(tranche);
    }
    // send default hints as the last fallback tranche
    const auto defaultFeedbackPrivate = get(m_bufferintegration->defaultFeedback.get());
    if (this != defaultFeedbackPrivate) {
        for (const auto &tranche : std::as_const(defaultFeedbackPrivate->m_tranches)) {
            sendTranche(tranche);
        }
    }
    send_done(resource->handle);
}

void LinuxDmaBufV1FeedbackPrivate::zwp_linux_dmabuf_feedback_v1_bind_resource(Resource *resource)
{
    send(resource);
}

void LinuxDmaBufV1FeedbackPrivate::zwp_linux_dmabuf_feedback_v1_destroy(Resource *resource)
{
    wl_resource_destroy(resource->handle);
}

struct linux_dmabuf_feedback_v1_table_entry
{
    uint32_t format;
    uint32_t pad; // unused
    uint64_t modifier;
};

LinuxDmaBufV1FormatTable::LinuxDmaBufV1FormatTable(const QHash<uint32_t, QVector<uint64_t>> &supportedModifiers)
{
    QVector<linux_dmabuf_feedback_v1_table_entry> data;
    for (auto it = supportedModifiers.begin(); it != supportedModifiers.end(); it++) {
        const uint32_t format = it.key();
        for (const uint64_t &mod : *it) {
            indices.insert({format, mod}, data.size());
            data.append({format, 0, mod});
        }
    }

    const auto size = data.size() * sizeof(linux_dmabuf_feedback_v1_table_entry);
    file = KWin::RamFile("kwin-dmabuf-feedback-table", data.constData(), size, KWin::RamFile::Flag::SealWrite);
    if (!file.isValid()) {
        qCCritical(KWIN_CORE) << "Failed to create RamFile for LinuxDmaBufV1FormatTable";
        return;
    }
}

} // namespace KWaylandServer
