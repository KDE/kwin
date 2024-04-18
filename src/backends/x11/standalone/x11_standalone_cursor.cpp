/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2013 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "x11_standalone_cursor.h"
#include "input.h"
#include "keyboard_input.h"
#include "utils/common.h"
#include "utils/xcbutils.h"
#include "x11_standalone_xfixes_cursor_event_filter.h"

#include <QAbstractEventDispatcher>
#include <QTimer>

namespace KWin
{

X11Cursor::X11Cursor(bool xInputSupport)
    : Cursor()
    , m_timeStamp(XCB_TIME_CURRENT_TIME)
    , m_buttonMask(0)
    , m_hasXInput(xInputSupport)
{
    Cursors::self()->setMouse(this);
    if (m_hasXInput) {
        // with XInput we don't have to poll the mouse on a timer, Xorg will notify us when it moved
        m_resetTimeStampTimer.setSingleShot(true);
        m_mousePollingTimer.setSingleShot(true);
        m_mousePollingTimer.setInterval(50);
        connect(qApp->eventDispatcher(), &QAbstractEventDispatcher::aboutToBlock, this, &X11Cursor::aboutToBlock);
    } else {
        connect(&m_resetTimeStampTimer, &QTimer::timeout, this, &X11Cursor::resetTimeStamp);
        connect(&m_mousePollingTimer, &QTimer::timeout, this, &X11Cursor::pollMouse);
        m_resetTimeStampTimer.setSingleShot(true);
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
    if (m_timeStamp != XCB_TIME_CURRENT_TIME && m_timeStamp == xTime()) {
        // time stamps did not change, no need to query again
        return;
    }
    m_timeStamp = xTime();
    Xcb::Pointer pointer(rootWindow());
    if (pointer.isNull()) {
        return;
    }
    m_buttonMask = pointer->mask;
    updatePos(QPointF(pointer->root_x, pointer->root_y));
    m_resetTimeStampTimer.start(0);
}

void X11Cursor::resetTimeStamp()
{
    m_timeStamp = XCB_TIME_CURRENT_TIME;
}

void X11Cursor::aboutToBlock()
{
    if (!m_mousePollingTimer.isActive()) {
        m_mousePollingTimer.start();
    }
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
