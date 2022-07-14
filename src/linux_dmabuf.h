/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2019 Roman Gilg <subdiff@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include <kwin_export.h>

#include "wayland/linuxdmabufv1clientbuffer.h"

namespace KWin
{

class KWIN_EXPORT LinuxDmaBufV1ClientBuffer : public KWaylandServer::LinuxDmaBufV1ClientBuffer
{
public:
    LinuxDmaBufV1ClientBuffer(DmaBufAttributes &&attrs, quint32 flags);
    ~LinuxDmaBufV1ClientBuffer() override;
};

class KWIN_EXPORT LinuxDmaBufV1RendererInterface : public KWaylandServer::LinuxDmaBufV1ClientBufferIntegration::RendererInterface
{
public:
    explicit LinuxDmaBufV1RendererInterface();
    ~LinuxDmaBufV1RendererInterface() override;

    KWaylandServer::LinuxDmaBufV1ClientBuffer *importBuffer(DmaBufAttributes &&attrs, quint32 flags) override;

protected:
    void setSupportedFormatsAndModifiers(const QVector<KWaylandServer::LinuxDmaBufV1Feedback::Tranche> &tranches);
};

}
