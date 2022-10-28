/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2019 Roman Gilg <subdiff@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "linux_dmabuf.h"

#include "wayland_server.h"

namespace KWin
{

LinuxDmaBufV1ClientBuffer::LinuxDmaBufV1ClientBuffer(DmaBufAttributes &&attrs, quint32 flags)
    : KWaylandServer::LinuxDmaBufV1ClientBuffer(std::move(attrs), flags)
{
    waylandServer()->addLinuxDmabufBuffer(this);
}

LinuxDmaBufV1ClientBuffer::~LinuxDmaBufV1ClientBuffer()
{
    if (waylandServer()) {
        waylandServer()->removeLinuxDmabufBuffer(this);
    }
}

LinuxDmaBufV1RendererInterface::LinuxDmaBufV1RendererInterface()
{
    Q_ASSERT(waylandServer());
    waylandServer()->linuxDmabuf()->setRendererInterface(this);
}

LinuxDmaBufV1RendererInterface::~LinuxDmaBufV1RendererInterface()
{
    waylandServer()->linuxDmabuf()->setRendererInterface(nullptr);
}

KWaylandServer::LinuxDmaBufV1ClientBuffer *LinuxDmaBufV1RendererInterface::importBuffer(DmaBufAttributes &&attrs, quint32 flags)
{
    return nullptr;
}

void LinuxDmaBufV1RendererInterface::setSupportedFormatsAndModifiers(const QVector<KWaylandServer::LinuxDmaBufV1Feedback::Tranche> &tranches)
{
    waylandServer()->linuxDmabuf()->setSupportedFormatsWithModifiers(tranches);
}

}
