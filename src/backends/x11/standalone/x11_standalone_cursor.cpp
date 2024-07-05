/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2013 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "x11_standalone_cursor.h"
#include "utils/common.h"
#include "utils/xcbutils.h"
#include "x11_standalone_xfixes_cursor_event_filter.h"

#include <QAbstractEventDispatcher>
#include <QTimer>

namespace KWin
{

X11Cursor::X11Cursor(bool xInputSupport)
    : Cursor()
    , m_buttonMask(0)
    , m_hasXInput(xInputSupport)
{
    Cursors::self()->setMouse(this);
    if (!m_hasXInput) {
        // without XInput we don't get events about cursor movement, so we have to poll instead
        connect(&m_mousePollingTimer, &QTimer::timeout, this, &X11Cursor::pollMouse);
        m_mousePollingTimer.setSingleShot(false);
        m_mousePollingTimer.setInterval(50);
        m_mousePollingTimer.start();
    }
    if (Xcb::Extensions::self()->isFixesAvailable()) {
        xcb_xfixes_select_cursor_input(connection(), rootWindow(), XCB_XFIXES_CURSOR_NOTIFY_MASK_DISPLAY_CURSOR);
    }

#ifndef KCMRULES
    connect(kwinApp(), &Application::workspaceCreated, this, [this]() {
        if (Xcb::Extensions::self()->isFixesAvailable()) {
            m_xfixesFilter = std::make_unique<XFixesCursorEventFilter>(this);
        }
    });
#endif
}

X11Cursor::~X11Cursor()
{
}

void X11Cursor::doSetPos()
{
    const QPointF &pos = currentPos();
    xcb_warp_pointer(connection(), XCB_WINDOW_NONE, rootWindow(), 0, 0, 0, 0, pos.x(), pos.y());
    // call default implementation to emit signal
    Cursor::doSetPos();
}

void X11Cursor::doGetPos()
{
    Xcb::Pointer pointer(rootWindow());
    if (pointer.isNull()) {
        return;
    }
    m_buttonMask = pointer->mask;
    updatePos(QPointF(pointer->root_x, pointer->root_y));
}

void X11Cursor::pollMouse()
{
    static QPointF lastPos = currentPos();
    static uint16_t lastMask = m_buttonMask;
    doGetPos(); // Update if needed
    if (lastPos != currentPos() || lastMask != m_buttonMask) {
        Q_EMIT mouseChanged(currentPos(), lastPos,
                            x11ToQtMouseButtons(m_buttonMask), x11ToQtMouseButtons(lastMask),
                            x11ToQtKeyboardModifiers(m_buttonMask), x11ToQtKeyboardModifiers(lastMask));
        lastPos = currentPos();
        lastMask = m_buttonMask;
    }
}

void X11Cursor::notifyCursorChanged()
{
    Q_EMIT cursorChanged();
}

void X11Cursor::notifyCursorPosChanged()
{
    pollMouse();
}
}

#include "moc_x11_standalone_cursor.cpp"
