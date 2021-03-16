/*
    SPDX-FileCopyrightText: 2021 Aleix Pol Gonzalez <aleixpol@kde.org>

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
class FullscreenShellV1Integration : public WaylandShellIntegration
{
    Q_OBJECT

public:
    explicit FullscreenShellV1Integration(QObject *parent = nullptr);
};

} // namespace KWin
