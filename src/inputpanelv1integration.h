/*
    SPDX-FileCopyrightText: 2020 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "waylandshellintegration.h"

namespace KWin
{

class InputPanelSurfaceV1Interface;

class InputPanelV1Integration : public WaylandShellIntegration
{
    Q_OBJECT

public:
    explicit InputPanelV1Integration(QObject *parent = nullptr);

    void createWindow(InputPanelSurfaceV1Interface *shellSurface);
};

} // namespace KWin
