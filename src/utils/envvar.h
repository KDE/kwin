/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2025 Xaver Hugl <xaver.hugl@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include <QtEnvironmentVariables>
#include <optional>

namespace KWin
{

inline std::optional<int> environmentVariableIntValue(const char *name)
{
    bool ok = false;
    int value = qEnvironmentVariableIntValue(name, &ok);
    if (ok) {
        return value;
    } else {
        return std::nullopt;
    }
}

}
