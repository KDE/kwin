/*
    SPDX-FileCopyrightText: 2022 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once
#include "kwin_export.h"
#include "utils/memorymap.h"

#include <QByteArray>
#include <QHash>
#include <QList>
#include <QObject>
#include <memory>
#include <stdint.h>
#include <sys/types.h>

struct wl_array;
struct wl_buffer;
struct wl_registry;
struct wl_surface;
struct zwp_linux_dmabuf_feedback_v1;
struct zwp_linux_dmabuf_v1;

namespace KWin
{
class GraphicsBuffer;

namespace WaylandClient
{

class KWIN_EXPORT LinuxDmabufFeedbackV1 : public QObject
{
    Q_OBJECT
public:
    explicit LinuxDmabufFeedbackV1(zwp_linux_dmabuf_feedback_v1 *feedback);
    ~LinuxDmabufFeedbackV1();

    QHash<uint32_t, QList<uint64_t>> formats() const;
    QByteArray devicePath() const;

    struct Tranche
    {
        QHash<uint32_t, QList<uint64_t>> formats;
        bool scanout = false;
    };

    QList<Tranche> tranches() const;

Q_SIGNALS:
    void changed();

private:
    static void done(void *data, zwp_linux_dmabuf_feedback_v1 *zwp_linux_dmabuf_feedback_v1);
    static void format_table(void *data, zwp_linux_dmabuf_feedback_v1 *zwp_linux_dmabuf_feedback_v1, int32_t fd, uint32_t size);
    static void main_device(void *data, zwp_linux_dmabuf_feedback_v1 *zwp_linux_dmabuf_feedback_v1, wl_array *deviceId);
    static void tranche_done(void *data, zwp_linux_dmabuf_feedback_v1 *zwp_linux_dmabuf_feedback_v1);
    static void tranche_target_device(void *data, zwp_linux_dmabuf_feedback_v1 *zwp_linux_dmabuf_feedback_v1, wl_array *deviceId);
    static void tranche_formats(void *data, zwp_linux_dmabuf_feedback_v1 *zwp_linux_dmabuf_feedback_v1, wl_array *indices);
    static void tranche_flags(void *data, zwp_linux_dmabuf_feedback_v1 *zwp_linux_dmabuf_feedback_v1, uint32_t flags);

    zwp_linux_dmabuf_feedback_v1 *const m_feedback;
    QByteArray m_mainDevice;
    dev_t m_mainDeviceId = 0;
    dev_t m_trancheDeviceId = 0;
    MemoryMap m_formatTable;
    QHash<uint32_t, QList<uint64_t>> m_formats;
    QList<Tranche> m_tranches;
    QList<Tranche> m_pendingTranches;
    Tranche m_pendingTranche;
};

class KWIN_EXPORT LinuxDmabufV1
{
public:
    LinuxDmabufV1(wl_registry *registry, uint32_t name, uint32_t version);
    ~LinuxDmabufV1();

    zwp_linux_dmabuf_v1 *handle() const;
    QByteArray mainDevice() const;
    QHash<uint32_t, QList<uint64_t>> formats() const;
    /**
     * NOTE that it's up to the calling client to take care of the
     * life time of the graphics buffer and wl_buffer!
     */
    wl_buffer *importBuffer(GraphicsBuffer *graphicsBuffer) const;

    std::unique_ptr<LinuxDmabufFeedbackV1> getSurfaceFeedback(wl_surface *surface) const;

private:
    static void format(void *data, struct zwp_linux_dmabuf_v1 *zwp_linux_dmabuf_v1, uint32_t format);
    static void modifier(void *data, struct zwp_linux_dmabuf_v1 *zwp_linux_dmabuf_v1, uint32_t format, uint32_t modifier_hi, uint32_t modifier_lo);

    zwp_linux_dmabuf_v1 *m_dmabuf;
    std::unique_ptr<LinuxDmabufFeedbackV1> m_defaultFeedback;
};

}
}
