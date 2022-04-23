/*
    SPDX-FileCopyrightText: 2020 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "inputpanelv1integration.h"
#include "inputpanelv1window.h"
#include "wayland/display.h"
#include "wayland/inputmethod_v1_interface.h"
#include "wayland_server.h"

using namespace KWaylandServer;

namespace KWin
{

InputPanelV1Integration::InputPanelV1Integration(QObject *parent)
    : WaylandShellIntegration(parent)
{
    InputPanelV1Interface *shell = new InputPanelV1Interface(waylandServer()->display(), this);

    connect(shell, &InputPanelV1Interface::inputPanelSurfaceAdded,
            this, &InputPanelV1Integration::createWindow);
}

void InputPanelV1Integration::createWindow(InputPanelSurfaceV1Interface *shellSurface)
{
    Q_EMIT windowCreated(new InputPanelV1Window(shellSurface));
}

} // namespace KWin
