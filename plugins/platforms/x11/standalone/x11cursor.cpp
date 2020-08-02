/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2013 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "x11cursor.h"
#include "input.h"
#include "keyboard_input.h"
#include "utils.h"
#include "xcbutils.h"
#include "xfixes_cursor_event_filter.h"

#include <QAbstractEventDispatcher>
#include <QTimer>

#include <xcb/xcb_cursor.h>

namespace KWin
{

X11Cursor::X11Cursor(QObject *parent, bool xInputSupport)
    : Cursor(parent)
    , m_timeStamp(XCB_TIME_CURRENT_TIME)
    , m_buttonMask(0)
    , m_resetTimeStampTimer(new QTimer(this))
    , m_mousePollingTimer(new QTimer(this))
    , m_hasXInput(xInputSupport)
    , m_needsPoll(false)
{
    Cursors::self()->setMouse(this);
    m_resetTimeStampTimer->setSingleShot(true);
    connect(m_resetTimeStampTimer, SIGNAL(timeout()), SLOT(resetTimeStamp()));
    // TODO: How often do we really need to poll?
    m_mousePollingTimer->setInterval(50);
    connect(m_mousePollingTimer, SIGNAL(timeout()), SLOT(mousePolled()));

    connect(this, &Cursor::themeChanged, this, [this] { m_cursors.clear(); });

    if (m_hasXInput) {
        connect(qApp->eventDispatcher(), &QAbstractEventDispatcher::aboutToBlock, this, &X11Cursor::aboutToBlock);
    }

#ifndef KCMRULES
    connect(kwinApp(), &Application::workspaceCreated, this,
        [this] {
            if (Xcb::Extensions::self()->isFixesAvailable()) {
                m_xfixesFilter = std::make_unique<XFixesCursorEventFilter>(this);
            }
        }
    );
#endif
}

X11Cursor::~X11Cursor()
{
}

void X11Cursor::doSetPos()
{
    const QPoint &pos = currentPos();
    xcb_warp_pointer(connection(), XCB_WINDOW_NONE, rootWindow(), 0, 0, 0, 0, pos.x(), pos.y());
    // call default implementation to emit signal
    Cursor::doSetPos();
}

void X11Cursor::doGetPos()
{
    if (m_timeStamp != XCB_TIME_CURRENT_TIME &&
            m_timeStamp == xTime()) {
        // time stamps did not change, no need to query again
        return;
    }
    m_timeStamp = xTime();
    Xcb::Pointer pointer(rootWindow());
    if (pointer.isNull()) {
        return;
    }
    m_buttonMask = pointer->mask;
    updatePos(pointer->root_x, pointer->root_y);
    m_resetTimeStampTimer->start(0);
}

void X11Cursor::resetTimeStamp()
{
    m_timeStamp = XCB_TIME_CURRENT_TIME;
}

void X11Cursor::aboutToBlock()
{
    if (m_needsPoll) {
        mousePolled();
        m_needsPoll = false;
    }
}

void X11Cursor::doStartMousePolling()
{
    if (!m_hasXInput) {
        m_mousePollingTimer->start();
    }
}

void X11Cursor::doStopMousePolling()
{
    if (!m_hasXInput) {
        m_mousePollingTimer->stop();
    }
}

void X11Cursor::doStartCursorTracking()
{
    xcb_xfixes_select_cursor_input(connection(), rootWindow(), XCB_XFIXES_CURSOR_NOTIFY_MASK_DISPLAY_CURSOR);
}

void X11Cursor::doStopCursorTracking()
{
    xcb_xfixes_select_cursor_input(connection(), rootWindow(), 0);
}

void X11Cursor::mousePolled()
{
    static QPoint lastPos = currentPos();
    static uint16_t lastMask = m_buttonMask;
    doGetPos(); // Update if needed
    if (lastPos != currentPos() || lastMask != m_buttonMask) {
        emit mouseChanged(currentPos(), lastPos,
            x11ToQtMouseButtons(m_buttonMask), x11ToQtMouseButtons(lastMask),
            x11ToQtKeyboardModifiers(m_buttonMask), x11ToQtKeyboardModifiers(lastMask));
        lastPos = currentPos();
        lastMask = m_buttonMask;
    }
}

xcb_cursor_t X11Cursor::getX11Cursor(CursorShape shape)
{
    return getX11Cursor(shape.name());
}

xcb_cursor_t X11Cursor::getX11Cursor(const QByteArray &name)
{
    auto it = m_cursors.constFind(name);
    if (it != m_cursors.constEnd()) {
        return it.value();
    }
    return createCursor(name);
}

xcb_cursor_t X11Cursor::createCursor(const QByteArray &name)
{
    if (name.isEmpty()) {
        return XCB_CURSOR_NONE;
    }
    xcb_cursor_context_t *ctx;
    if (xcb_cursor_context_new(kwinApp()->x11Connection(), kwinApp()->x11DefaultScreen(), &ctx) < 0) {
        return XCB_CURSOR_NONE;
    }
    xcb_cursor_t cursor = xcb_cursor_load_cursor(ctx, name.constData());
    if (cursor == XCB_CURSOR_NONE) {
        const auto &names = cursorAlternativeNames(name);
        for (auto cit = names.begin(); cit != names.end(); ++cit) {
            cursor = xcb_cursor_load_cursor(ctx, (*cit).constData());
            if (cursor != XCB_CURSOR_NONE) {
                break;
            }
        }
    }
    if (cursor != XCB_CURSOR_NONE) {
        m_cursors.insert(name, cursor);
    }
    xcb_cursor_context_free(ctx);
    return cursor;
}

void X11Cursor::notifyCursorChanged()
{
    if (!isCursorTracking()) {
        // cursor change tracking is currently disabled, so don't emit signal
        return;
    }
    emit cursorChanged();
}

}
