/*
    SPDX-FileCopyrightText: 2025 Nicolas Fella <nicolas.fella@gmx.de>

    SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#pragma once

#include "plugin.h"

#include <qtdbusglobal.h>

namespace KWin
{
class TouchpadShortcuts : public KWin::Plugin
{
    Q_OBJECT
    // Keep this DBus interface in sync with the X11 KDED one (https://invent.kde.org/plasma/plasma-desktop/-/blob/master/kcms/touchpad/kded/kded.h?ref_type=heads#L22)
    Q_CLASSINFO("D-Bus Interface", "org.kde.touchpad")
public:
    explicit TouchpadShortcuts();

public Q_SLOTS:
    Q_SCRIPTABLE Q_NOREPLY void toggle();
    Q_SCRIPTABLE Q_NOREPLY void disable();
    Q_SCRIPTABLE Q_NOREPLY void enable();

private:
    void enableOrDisableTouchpads(bool enable);
    void toggleTouchpads();
    void enableTouchpads();
    void disableTouchpads();
};
}
