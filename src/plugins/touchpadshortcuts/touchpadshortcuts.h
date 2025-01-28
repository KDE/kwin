/*
    SPDX-FileCopyrightText: 2025 Nicolas Fella <nicolas.fella@gmx.de>

    SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#pragma once

#include "plugin.h"

namespace KWin
{
class TouchpadShortcuts : public KWin::Plugin
{
    Q_OBJECT
public:
    explicit TouchpadShortcuts();

private:
    void enableOrDisableTouchpads(bool enable);
    void toggleTouchpads();
    void enableTouchpads();
    void disableTouchpads();
};
}
