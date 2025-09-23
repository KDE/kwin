/*
    SPDX-FileCopyrightText: 2022 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "linuxdmabuf.h"
#include "utils/memorymap.h"
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

class LinuxDmabufFeedbackV1
{
public:
    LinuxDmabufFeedbackV1(zwp_linux_dmabuf_feedback_v1 *feedback)
        : feedback(feedback)
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

    ~LinuxDmabufFeedbackV1()
    {
        zwp_linux_dmabuf_feedback_v1_destroy(feedback);
    }

    zwp_linux_dmabuf_feedback_v1 *feedback;
    QByteArray mainDevice;
    dev_t mainDeviceId = 0;
    dev_t trancheDeviceId = 0;
    MemoryMap formatTable;
    QHash<uint32_t, QList<uint64_t>> formats;

private:
    static void done(void *data, zwp_linux_dmabuf_feedback_v1 *zwp_linux_dmabuf_feedback_v1)
    {
        // Nothing to do
    }

    static void format_table(void *data, zwp_linux_dmabuf_feedback_v1 *zwp_linux_dmabuf_feedback_v1, int32_t fd, uint32_t size)
    {
        LinuxDmabufFeedbackV1 *feedback = static_cast<LinuxDmabufFeedbackV1 *>(data);

        feedback->formatTable = MemoryMap(size, PROT_READ, MAP_PRIVATE, fd, 0);
        ::close(fd);
    }

    static void main_device(void *data, zwp_linux_dmabuf_feedback_v1 *zwp_linux_dmabuf_feedback_v1, wl_array *deviceId)
    {
        LinuxDmabufFeedbackV1 *feedback = static_cast<LinuxDmabufFeedbackV1 *>(data);

        feedback->mainDeviceId = deserializeDeviceId(deviceId);

        drmDevice *device = nullptr;
        if (drmGetDeviceFromDevId(feedback->mainDeviceId, 0, &device) != 0) {
            qCWarning(KWIN_WAYLAND_CLIENT) << "drmGetDeviceFromDevId() failed";
            return;
        }

        if (device->available_nodes & (1 << DRM_NODE_RENDER)) {
            feedback->mainDevice = QByteArray(device->nodes[DRM_NODE_RENDER]);
        } else if (device->available_nodes & (1 << DRM_NODE_PRIMARY)) {
            // We can't reliably find the render node from the primary node if the display and
            // render devices are split, so just fallback to the primary node.
            feedback->mainDevice = QByteArray(device->nodes[DRM_NODE_PRIMARY]);
        }

        drmFreeDevice(&device);
    }

    static void tranche_done(void *data, zwp_linux_dmabuf_feedback_v1 *zwp_linux_dmabuf_feedback_v1)
    {
        LinuxDmabufFeedbackV1 *feedback = static_cast<LinuxDmabufFeedbackV1 *>(data);

        feedback->trancheDeviceId = 0;
    }

    static void tranche_target_device(void *data, zwp_linux_dmabuf_feedback_v1 *zwp_linux_dmabuf_feedback_v1, wl_array *deviceId)
    {
        LinuxDmabufFeedbackV1 *feedback = static_cast<LinuxDmabufFeedbackV1 *>(data);

        feedback->trancheDeviceId = deserializeDeviceId(deviceId);
    }

    static void tranche_formats(void *data, zwp_linux_dmabuf_feedback_v1 *zwp_linux_dmabuf_feedback_v1, wl_array *indices)
    {
        LinuxDmabufFeedbackV1 *feedback = static_cast<LinuxDmabufFeedbackV1 *>(data);
        if (!feedback->formatTable.isValid()) {
            return;
        }
        if (feedback->mainDeviceId != feedback->trancheDeviceId) {
            return;
        }

        struct linux_dmabuf_feedback_v1_table_entry
        {
            uint32_t format;
            uint32_t pad; // unused
            uint64_t modifier;
        };

        const auto entries = static_cast<linux_dmabuf_feedback_v1_table_entry *>(feedback->formatTable.data());
        for (const uint16_t &index : std::span(static_cast<uint16_t *>(indices->data), indices->size / sizeof(uint16_t))) {
            const linux_dmabuf_feedback_v1_table_entry &entry = entries[index];
            feedback->formats[entry.format].append(entry.modifier);
        }
    }

    static void tranche_flags(void *data, zwp_linux_dmabuf_feedback_v1 *zwp_linux_dmabuf_feedback_v1, uint32_t flags)
    {
        // Nothing to do
    }
};

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
    return m_defaultFeedback->mainDevice;
}

QHash<uint32_t, QList<uint64_t>> LinuxDmabufV1::formats() const
{
    return m_defaultFeedback->formats;
}

void LinuxDmabufV1::format(void *data, struct zwp_linux_dmabuf_v1 *zwp_linux_dmabuf_v1, uint32_t format)
{
    // Not sent in v4 and onward.
}

void LinuxDmabufV1::modifier(void *data, struct zwp_linux_dmabuf_v1 *zwp_linux_dmabuf_v1, uint32_t format, uint32_t modifier_hi, uint32_t modifier_lo)
{
    // Not sent in v4 and onward.
}

}
