/*
    SPDX-FileCopyrightText: 2022 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include <QByteArray>
#include <QHash>
#include <QList>
#include <memory>
#include <stdint.h>

struct wl_registry;
struct zwp_linux_dmabuf_v1;

namespace KWin::WaylandClient
{

class LinuxDmabufFeedbackV1;

class LinuxDmabufV1
{
public:
    LinuxDmabufV1(wl_registry *registry, uint32_t name, uint32_t version);
    ~LinuxDmabufV1();

    zwp_linux_dmabuf_v1 *handle() const;
    QByteArray mainDevice() const;
    QHash<uint32_t, QList<uint64_t>> formats() const;

private:
    static void format(void *data, struct zwp_linux_dmabuf_v1 *zwp_linux_dmabuf_v1, uint32_t format);
    static void modifier(void *data, struct zwp_linux_dmabuf_v1 *zwp_linux_dmabuf_v1, uint32_t format, uint32_t modifier_hi, uint32_t modifier_lo);

    zwp_linux_dmabuf_v1 *m_dmabuf;
    std::unique_ptr<LinuxDmabufFeedbackV1> m_defaultFeedback;
};

}
