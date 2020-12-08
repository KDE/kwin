/*
    SPDX-FileCopyrightText: 2020 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "waylandshellintegration.h"

namespace KWaylandServer
{
class InputPanelSurfaceV1Interface;
}

namespace KWin
{

class InputPanelV1Integration : public WaylandShellIntegration
{
    Q_OBJECT

public:
    explicit InputPanelV1Integration(QObject *parent = nullptr);

    void createClient(KWaylandServer::InputPanelSurfaceV1Interface *shellSurface);
};

} // namespace KWin
