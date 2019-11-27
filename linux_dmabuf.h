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
#pragma once

#include <kwin_export.h>

#include <KWayland/Server/linuxdmabuf_v1_interface.h>

#include <QVector>

namespace KWin
{

class KWIN_EXPORT DmabufBuffer : public KWayland::Server::LinuxDmabufUnstableV1Buffer
{
public:
    using Plane = KWayland::Server::LinuxDmabufUnstableV1Interface::Plane;
    using Flags = KWayland::Server::LinuxDmabufUnstableV1Interface::Flags;

    DmabufBuffer(const QVector<Plane> &planes,
                 uint32_t format,
                 const QSize &size,
                 Flags flags);

    ~DmabufBuffer() override;

    const QVector<Plane> &planes() const { return m_planes; }
    uint32_t format() const { return m_format; }
    QSize size() const { return m_size; }
    Flags flags() const { return m_flags; }

private:
    QVector<Plane> m_planes;
    uint32_t m_format;
    QSize m_size;
    Flags m_flags;
};

class KWIN_EXPORT LinuxDmabuf : public KWayland::Server::LinuxDmabufUnstableV1Interface::Impl
{
public:
    using Plane = KWayland::Server::LinuxDmabufUnstableV1Interface::Plane;
    using Flags = KWayland::Server::LinuxDmabufUnstableV1Interface::Flags;

    explicit LinuxDmabuf();
    ~LinuxDmabuf() override;

    KWayland::Server::LinuxDmabufUnstableV1Buffer *importBuffer(const QVector<Plane> &planes,
                                                                uint32_t format,
                                                                const QSize &size,
                                                                Flags flags) override;

protected:
    void setSupportedFormatsAndModifiers(QHash<uint32_t, QSet<uint64_t> > &set);
};

}
