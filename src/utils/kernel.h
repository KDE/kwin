/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2023 Xaver Hugl <xaver.hugl@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once
#include "libkwineffects/version.h"

#include <sys/utsname.h>

namespace KWin
{

inline static Version linuxKernelVersion()
{
    struct utsname name;
    uname(&name);

    if (qstrcmp(name.sysname, "Linux") == 0) {
        return Version::parseString(name.release);
    }
    return Version(0, 0, 0);
}

}
