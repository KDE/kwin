/*
    SPDX-FileCopyrightText: 2020 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "inputpanelv1integration.h"
#include "inputpanelv1client.h"
#include "wayland_server.h"

#include <KWaylandServer/display.h>
#include <KWaylandServer/inputmethod_v1_interface.h>

using namespace KWaylandServer;

namespace KWin
{

InputPanelV1Integration::InputPanelV1Integration(QObject *parent)
    : WaylandShellIntegration(parent)
{
    InputPanelV1Interface *shell = waylandServer()->display()->createInputPanelInterface(this);

    connect(shell, &InputPanelV1Interface::inputPanelSurfaceAdded,
            this, &InputPanelV1Integration::createClient);
}

void InputPanelV1Integration::createClient(InputPanelSurfaceV1Interface *shellSurface)
{
    emit clientCreated(new InputPanelV1Client(shellSurface));
}

} // namespace KWin
