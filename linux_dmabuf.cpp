/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright © 2019 Roman Gilg <subdiff@gmail.com>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*********************************************************************/
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
