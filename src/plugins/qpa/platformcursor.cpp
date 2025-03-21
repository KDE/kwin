/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2016 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "platformcursor.h"
#include "pointer_input.h"

namespace KWin
{
namespace QPA
{

PlatformCursor::PlatformCursor()
    : QPlatformCursor()
{
}

PlatformCursor::~PlatformCursor() = default;

QPoint PlatformCursor::pos() const
{
    return input()->pointer()->pos().toPoint();
}

void PlatformCursor::setPos(const QPoint &pos)
{
    input()->pointer()->warp(pos);
}

void PlatformCursor::changeCursor(QCursor *windowCursor, QWindow *window)
{
    // TODO: implement
}

}
}
