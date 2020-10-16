/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2019 Roman Gilg <subdiff@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "linux_dmabuf.h"

#include "wayland_server.h"

#include <unistd.h>

namespace KWin
{

DmabufBuffer::DmabufBuffer(const QVector<Plane> &planes,
                           uint32_t format,
                           const QSize &size,
                           Flags flags)
    : KWaylandServer::LinuxDmabufUnstableV1Buffer(format, size)
    , m_planes(planes)
    , m_format(format)
    , m_size(size)
    , m_flags(flags)
{
    waylandServer()->addLinuxDmabufBuffer(this);
}

DmabufBuffer::~DmabufBuffer()
{
    // Close all open file descriptors
    for (int i = 0; i < m_planes.count(); i++) {
        if (m_planes[i].fd != -1)
            ::close(m_planes[i].fd);
        m_planes[i].fd = -1;
    }
    if (waylandServer()) {
        waylandServer()->removeLinuxDmabufBuffer(this);
    }
}

LinuxDmabuf::LinuxDmabuf()
    : KWaylandServer::LinuxDmabufUnstableV1Interface::Impl()
{
    Q_ASSERT(waylandServer());
    waylandServer()->linuxDmabuf()->setImpl(this);
}

LinuxDmabuf::~LinuxDmabuf()
{
    waylandServer()->linuxDmabuf()->setImpl(nullptr);
}

using Plane = KWaylandServer::LinuxDmabufUnstableV1Interface::Plane;
using Flags = KWaylandServer::LinuxDmabufUnstableV1Interface::Flags;

KWaylandServer::LinuxDmabufUnstableV1Buffer* LinuxDmabuf::importBuffer(const QVector<Plane> &planes,
                                                                         uint32_t format,
                                                                         const QSize &size,
                                                                         Flags flags)
{
    Q_UNUSED(planes)
    Q_UNUSED(format)
    Q_UNUSED(size)
    Q_UNUSED(flags)

    return nullptr;
}

void LinuxDmabuf::setSupportedFormatsAndModifiers(QHash<uint32_t, QSet<uint64_t> > &set)
{
    waylandServer()->linuxDmabuf()->setSupportedFormatsWithModifiers(set);
}

}
