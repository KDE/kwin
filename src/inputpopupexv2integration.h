/*
    SPDX-FileCopyrightText: 2026 KWin

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "waylandshellintegration.h"

namespace KWin
{

class XXInputMethodManagerV2Interface;
class XXInputMethodV1Interface;
class XXInputPopupSurfaceV2Interface;

class InputPopupWindowExV2;

class InputPopupExV2Integration : public WaylandShellIntegration
{
    Q_OBJECT

public:
    explicit InputPopupExV2Integration(QObject *parent = nullptr);

private:
    void createWindow(XXInputPopupSurfaceV2Interface *popupSurface);
};

} // namespace KWin
