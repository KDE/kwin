/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2016 Martin Gräßlin <mgraesslin@kde.org>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*********************************************************************/
#include "platformcursor.h"
#include "../../cursor.h"

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
    return Cursors::self()->mouse()->pos();
}

void PlatformCursor::setPos(const QPoint &pos)
{
    Cursors::self()->mouse()->setPos(pos);
}

void PlatformCursor::changeCursor(QCursor *windowCursor, QWindow *window)
{
    Q_UNUSED(windowCursor)
    Q_UNUSED(window)
    // TODO: implement
}

}
}
