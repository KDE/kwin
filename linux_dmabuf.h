/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2019 Roman Gilg <subdiff@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include <kwin_export.h>

#include <KWaylandServer/linuxdmabuf_v1_interface.h>

#include <QVector>

namespace KWin
{

class KWIN_EXPORT DmabufBuffer : public KWaylandServer::LinuxDmabufUnstableV1Buffer
{
public:
    using Plane = KWaylandServer::LinuxDmabufUnstableV1Interface::Plane;
    using Flags = KWaylandServer::LinuxDmabufUnstableV1Interface::Flags;

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

class KWIN_EXPORT LinuxDmabuf : public KWaylandServer::LinuxDmabufUnstableV1Interface::Impl
{
public:
    using Plane = KWaylandServer::LinuxDmabufUnstableV1Interface::Plane;
    using Flags = KWaylandServer::LinuxDmabufUnstableV1Interface::Flags;

    explicit LinuxDmabuf();
    ~LinuxDmabuf() override;

    KWaylandServer::LinuxDmabufUnstableV1Buffer *importBuffer(const QVector<Plane> &planes,
                                                                uint32_t format,
                                                                const QSize &size,
                                                                Flags flags) override;

protected:
    void setSupportedFormatsAndModifiers(QHash<uint32_t, QSet<uint64_t> > &set);
};

}
