/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2018 Martin Flöser <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
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
