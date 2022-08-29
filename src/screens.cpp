/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2013 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "screens.h"

#include "main.h"
#include "platform.h"

namespace KWin
{

Screens::Screens()
{
    connect(kwinApp()->platform(), &Platform::screensQueried, this, &Screens::changed);
}

void Screens::init()
{
    Q_EMIT changed();
}

} // namespace
