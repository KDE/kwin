/*
    SPDX-FileCopyrightText: 2022 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "linuxdmabuf.h"
#include "core/graphicsbuffer.h"
#include "wayland-client/waylandclientlogging.h"

// Generated in src/wayland.
#include "wayland-linux-dmabuf-unstable-v1-client-protocol.h"

#include <span>
#include <unistd.h>
#include <wayland-client.h>
#include <xf86drm.h>

namespace KWin::WaylandClient
{

static dev_t deserializeDeviceId(wl_array *data)
{
    Q_ASSERT(sizeof(dev_t) == data->size);
    dev_t ret;
    std::memcpy(&ret, data->data, data->size);
    return ret;
}

LinuxDmabufFeedbackV1::LinuxDmabufFeedbackV1(zwp_linux_dmabuf_feedback_v1 *feedback)
    : m_feedback(feedback)
{
    static const struct zwp_linux_dmabuf_feedback_v1_listener feedbackListener = {
        .done = done,
        .format_table = format_table,
        .main_device = main_device,
        .tranche_done = tranche_done,
        .tranche_target_device = tranche_target_device,
        .tranche_formats = tranche_formats,
        .tranche_flags = tranche_flags,
    };
    zwp_linux_dmabuf_feedback_v1_add_listener(feedback, &feedbackListener, this);
}

LinuxDmabufFeedbackV1::~LinuxDmabufFeedbackV1()
{
    zwp_linux_dmabuf_feedback_v1_destroy(m_feedback);
}

QHash<uint32_t, QList<uint64_t>> LinuxDmabufFeedbackV1::formats() const
{
    return m_formats;
}

QByteArray LinuxDmabufFeedbackV1::devicePath() const
{
    return m_mainDevice;
}

QList<LinuxDmabufFeedbackV1::Tranche> LinuxDmabufFeedbackV1::tranches() const
{
    return m_tranches;
}

void LinuxDmabufFeedbackV1::done(void *data, zwp_linux_dmabuf_feedback_v1 *zwp_linux_dmabuf_feedback_v1)
{
    LinuxDmabufFeedbackV1 *feedback = static_cast<LinuxDmabufFeedbackV1 *>(data);
    feedback->m_tranches = feedback->m_pendingTranches;
    feedback->m_pendingTranches.clear();
    Q_EMIT feedback->changed();
}

void LinuxDmabufFeedbackV1::format_table(void *data, zwp_linux_dmabuf_feedback_v1 *zwp_linux_dmabuf_feedback_v1, int32_t fd, uint32_t size)
{
    LinuxDmabufFeedbackV1 *feedback = static_cast<LinuxDmabufFeedbackV1 *>(data);

    feedback->m_formatTable = MemoryMap(size, PROT_READ, MAP_PRIVATE, fd, 0);
    ::close(fd);
}

void LinuxDmabufFeedbackV1::main_device(void *data, zwp_linux_dmabuf_feedback_v1 *zwp_linux_dmabuf_feedback_v1, wl_array *deviceId)
{
    LinuxDmabufFeedbackV1 *feedback = static_cast<LinuxDmabufFeedbackV1 *>(data);

    feedback->m_mainDeviceId = deserializeDeviceId(deviceId);

    drmDevice *device = nullptr;
    if (drmGetDeviceFromDevId(feedback->m_mainDeviceId, 0, &device) != 0) {
        qCWarning(KWIN_WAYLAND_CLIENT) << "drmGetDeviceFromDevId() failed";
        return;
    }

    if (device->available_nodes & (1 << DRM_NODE_RENDER)) {
        feedback->m_mainDevice = QByteArray(device->nodes[DRM_NODE_RENDER]);
    } else if (device->available_nodes & (1 << DRM_NODE_PRIMARY)) {
        // We can't reliably find the render node from the primary node if the display and
        // render devices are split, so just fallback to the primary node.
        feedback->m_mainDevice = QByteArray(device->nodes[DRM_NODE_PRIMARY]);
    }

    drmFreeDevice(&device);
}

void LinuxDmabufFeedbackV1::tranche_done(void *data, zwp_linux_dmabuf_feedback_v1 *zwp_linux_dmabuf_feedback_v1)
{
    LinuxDmabufFeedbackV1 *feedback = static_cast<LinuxDmabufFeedbackV1 *>(data);

    feedback->m_trancheDeviceId = 0;
    feedback->m_pendingTranches.push_back(feedback->m_pendingTranche);
    feedback->m_pendingTranche = {};
}

void LinuxDmabufFeedbackV1::tranche_target_device(void *data, zwp_linux_dmabuf_feedback_v1 *zwp_linux_dmabuf_feedback_v1, wl_array *deviceId)
{
    LinuxDmabufFeedbackV1 *feedback = static_cast<LinuxDmabufFeedbackV1 *>(data);

    feedback->m_trancheDeviceId = deserializeDeviceId(deviceId);
}

void LinuxDmabufFeedbackV1::tranche_formats(void *data, zwp_linux_dmabuf_feedback_v1 *zwp_linux_dmabuf_feedback_v1, wl_array *indices)
{
    LinuxDmabufFeedbackV1 *feedback = static_cast<LinuxDmabufFeedbackV1 *>(data);
    if (!feedback->m_formatTable.isValid()) {
        qCWarning(KWIN_WAYLAND_CLIENT, "Didn't receive a valid format table for tranche!");
        return;
    }

    drmDevice *mainDevice = nullptr;
    drmDevice *trancheDevice = nullptr;
    const auto cleanup = qScopeGuard([&]() {
        drmFreeDevice(&mainDevice);
        drmFreeDevice(&trancheDevice);
    });
    if (drmGetDeviceFromDevId(feedback->m_mainDeviceId, 0, &mainDevice) != 0
        || drmGetDeviceFromDevId(feedback->m_trancheDeviceId, 0, &trancheDevice) != 0) {
        qCWarning(KWIN_WAYLAND_CLIENT, "drmGetDeviceFromDevId() failed");
        return;
    }
    if (!drmDevicesEqual(mainDevice, trancheDevice)) {
        // TODO take multi GPU handling into account as well
        qCDebug(KWIN_WAYLAND_CLIENT, "Ignoring tranche from non-main device");
        return;
    }

    struct linux_dmabuf_feedback_v1_table_entry
    {
        uint32_t format;
        uint32_t pad; // unused
        uint64_t modifier;
    };

    const auto entries = static_cast<linux_dmabuf_feedback_v1_table_entry *>(feedback->m_formatTable.data());
    for (const uint16_t &index : std::span(static_cast<uint16_t *>(indices->data), indices->size / sizeof(uint16_t))) {
        const linux_dmabuf_feedback_v1_table_entry &entry = entries[index];
        feedback->m_formats[entry.format].append(entry.modifier);
        feedback->m_pendingTranche.formats[entry.format].append(entry.modifier);
    }
}

void LinuxDmabufFeedbackV1::tranche_flags(void *data, zwp_linux_dmabuf_feedback_v1 *zwp_linux_dmabuf_feedback_v1, uint32_t flags)
{
    LinuxDmabufFeedbackV1 *feedback = static_cast<LinuxDmabufFeedbackV1 *>(data);
    feedback->m_pendingTranche.scanout = flags & ZWP_LINUX_DMABUF_FEEDBACK_V1_TRANCHE_FLAGS_SCANOUT;
}

LinuxDmabufV1::LinuxDmabufV1(wl_registry *registry, uint32_t name, uint32_t version)
{
    m_dmabuf = static_cast<zwp_linux_dmabuf_v1 *>(wl_registry_bind(registry, name, &zwp_linux_dmabuf_v1_interface, version));

    static const struct zwp_linux_dmabuf_v1_listener dmabufListener = {
        .format = format,
        .modifier = modifier,
    };
    zwp_linux_dmabuf_v1_add_listener(m_dmabuf, &dmabufListener, this);

    m_defaultFeedback = std::make_unique<LinuxDmabufFeedbackV1>(zwp_linux_dmabuf_v1_get_default_feedback(m_dmabuf));
}

LinuxDmabufV1::~LinuxDmabufV1()
{
    zwp_linux_dmabuf_v1_destroy(m_dmabuf);
}

zwp_linux_dmabuf_v1 *LinuxDmabufV1::handle() const
{
    return m_dmabuf;
}

QByteArray LinuxDmabufV1::mainDevice() const
{
    return m_defaultFeedback->devicePath();
}

QHash<uint32_t, QList<uint64_t>> LinuxDmabufV1::formats() const
{
    return m_defaultFeedback->formats();
}

void LinuxDmabufV1::format(void *data, struct zwp_linux_dmabuf_v1 *zwp_linux_dmabuf_v1, uint32_t format)
{
    // Not sent in v4 and onward.
}

void LinuxDmabufV1::modifier(void *data, struct zwp_linux_dmabuf_v1 *zwp_linux_dmabuf_v1, uint32_t format, uint32_t modifier_hi, uint32_t modifier_lo)
{
    // Not sent in v4 and onward.
}

wl_buffer *LinuxDmabufV1::importBuffer(GraphicsBuffer *graphicsBuffer) const
{
    Q_ASSERT(graphicsBuffer->dmabufAttributes());
    const DmaBufAttributes *attributes = graphicsBuffer->dmabufAttributes();

    zwp_linux_buffer_params_v1 *params = zwp_linux_dmabuf_v1_create_params(m_dmabuf);
    for (int i = 0; i < attributes->planeCount; ++i) {
        zwp_linux_buffer_params_v1_add(params,
                                       attributes->fd[i].get(),
                                       i,
                                       attributes->offset[i],
                                       attributes->pitch[i],
                                       attributes->modifier >> 32,
                                       attributes->modifier & 0xffffffff);
    }

    wl_buffer *buffer = zwp_linux_buffer_params_v1_create_immed(params, attributes->width, attributes->height, attributes->format, 0);
    zwp_linux_buffer_params_v1_destroy(params);

    return buffer;
}

std::unique_ptr<LinuxDmabufFeedbackV1> LinuxDmabufV1::getSurfaceFeedback(wl_surface *surface) const
{
    return std::make_unique<LinuxDmabufFeedbackV1>(zwp_linux_dmabuf_v1_get_surface_feedback(m_dmabuf, surface));
}
}

#include "moc_linuxdmabuf.cpp"
