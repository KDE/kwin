/*
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "core/output.h"
#include "wayland/output_interface.h"
#include "wayland/utils.h"
#include "wayland/xdgoutput_v1_interface.h"

namespace KWin
{

class WaylandOutput : public QObject
{
    Q_OBJECT

public:
    explicit WaylandOutput(Output *output, QObject *parent = nullptr);

private:
    Output *m_platformOutput;
    KWaylandServer::ScopedGlobalPointer<KWaylandServer::OutputInterface> m_waylandOutput;
    KWaylandServer::XdgOutputV1Interface *m_xdgOutputV1;
};

} // namespace KWin
