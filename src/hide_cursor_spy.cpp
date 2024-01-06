/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2018 Martin Flöser <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "hide_cursor_spy.h"
#include "cursor.h"
#include "input_event.h"
#include "main.h"

#include <backends/libinput/device.h>

namespace KWin
{

void HideCursorSpy::pointerMotion(PointerMotionEvent *event)
{
    showCursor();
}

void HideCursorSpy::pointerButton(PointerButtonEvent *event)
{
    showCursor();
}

void HideCursorSpy::pointerAxis(KWin::PointerAxisEvent *event)
{
    showCursor();
}

void HideCursorSpy::touchDown(qint32 id, const QPointF &pos, std::chrono::microseconds time)
{
    hideCursor();
}

void HideCursorSpy::tabletToolProximityEvent(TabletEvent *event)
{
    // If the tablet is in relative/mouse mode, keep it on the screen even if the pen is no longer in proximity
    if (auto libInputDevice = qobject_cast<LibInput::Device *>(event->device())) {
        if (libInputDevice->isRelative()) {
            return;
        }
    }
    if (event->type() == QEvent::Type::TabletLeaveProximity) {
        hideCursor();
    } else {
        showCursor();
    }
}

void HideCursorSpy::showCursor()
{
    if (!m_cursorHidden) {
        return;
    }
    m_cursorHidden = false;
    Cursors::self()->showCursor();
}

void HideCursorSpy::hideCursor()
{
    if (m_cursorHidden) {
        return;
    }
    m_cursorHidden = true;
    Cursors::self()->hideCursor();
}

}
