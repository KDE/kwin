/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2018 Martin Flöser <mgraesslin@kde.org>

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
#include "touch_hide_cursor_spy.h"
#include "main.h"
#include "platform.h"

namespace KWin
{

void TouchHideCursorSpy::pointerEvent(MouseEvent *event)
{
    Q_UNUSED(event)
    showCursor();
}

void TouchHideCursorSpy::wheelEvent(KWin::WheelEvent *event)
{
    Q_UNUSED(event)
    showCursor();
}

void TouchHideCursorSpy::touchDown(qint32 id, const QPointF &pos, quint32 time)
{
    Q_UNUSED(id)
    Q_UNUSED(pos)
    Q_UNUSED(time)
    hideCursor();
}

void TouchHideCursorSpy::showCursor()
{
    if (!m_cursorHidden) {
        return;
    }
    m_cursorHidden = false;
    kwinApp()->platform()->showCursor();
}

void TouchHideCursorSpy::hideCursor()
{
    if (m_cursorHidden) {
        return;
    }
    m_cursorHidden = true;
    kwinApp()->platform()->hideCursor();
}

}
