/*
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <epoxy/glx.h>
#include <fixx11h.h>

namespace KWin
{

GLXFBConfig chooseGlxFbConfig(::Display *display, const int attributes[]);

} // namespace KWin
