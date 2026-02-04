/*
    SPDX-FileCopyrightText: 2026 KWin

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "inputpopupexv2integration.h"
#include "inputpopupexv2window.h"
#include "main.h"
#include "wayland/display.h"
#include "wayland/xx_input_method_v2.h"
#include "wayland_server.h"

namespace KWin
{

InputPopupExV2Integration::InputPopupExV2Integration(QObject *parent)
    : WaylandShellIntegration(parent)
{
    // Hook to InputMethod when the v2 backend becomes available.
    connect(kwinApp()->inputMethod(), &InputMethod::imV2Ready, this, [this](XXInputMethodV1Interface *im) {
        connect(im, &XXInputMethodV1Interface::inputPopupSurfaceAdded, this, &InputPopupExV2Integration::createWindow);
    });
}

void InputPopupExV2Integration::createWindow(XXInputPopupSurfaceV2Interface *popupSurface)
{
    Q_EMIT windowCreated(new InputPopupWindowExV2(popupSurface));
}

} // namespace KWin

#include "moc_inputpopupexv2integration.cpp"
