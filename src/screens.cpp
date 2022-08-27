/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2013 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "screens.h"

#include <config-kwin.h>

#include "cursor.h"
#include "output.h"
#include "platform.h"
#include "settings.h"
#include "utils/common.h"
#include <window.h>
#include <workspace.h>

namespace KWin
{

Screens::Screens()
{
    connect(kwinApp()->platform(), &Platform::screensQueried, this, &Screens::changed);
}

} // namespace
