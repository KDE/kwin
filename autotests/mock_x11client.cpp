/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2014 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "mock_x11client.h"

namespace KWin
{

X11Client::X11Client(QObject *parent)
    : AbstractClient(parent)
{
}

X11Client::~X11Client() = default;

void X11Client::showOnScreenEdge()
{
    setKeepBelow(false);
    setHiddenInternal(false);
}

}
