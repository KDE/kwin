/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2013 Martin Gräßlin <mgraesslin@kde.org>

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

#include "cursor.h"
// kwin
#include <kwinglobals.h>
#include "utils.h"
// Qt
#include <QTimer>
// Xlib
#include <X11/Xcursor/Xcursor.h>
#include <fixx11h.h>
// xcb
#include <xcb/xfixes.h>

namespace KWin
{

KWIN_SINGLETON_FACTORY_FACTORED(Cursor, X11Cursor)

Cursor::Cursor(QObject *parent)
    : QObject(parent)
    , m_mousePollingCounter(0)
    , m_cursorTrackingCounter(0)
{
}

Cursor::~Cursor()
{
    s_self = NULL;
}

QPoint Cursor::pos()
{
    s_self->doGetPos();
    return s_self->m_pos;
}

void Cursor::setPos(const QPoint &pos)
{
    // first query the current pos to not warp to the already existing pos
    if (pos == Cursor::pos()) {
        return;
    }
    s_self->m_pos = pos;
    s_self->doSetPos();
}

void Cursor::setPos(int x, int y)
{
    Cursor::setPos(QPoint(x, y));
}

xcb_cursor_t Cursor::getX11Cursor(Qt::CursorShape shape)
{
    Q_UNUSED(shape)
    return XCB_CURSOR_NONE;
}

xcb_cursor_t Cursor::x11Cursor(Qt::CursorShape shape)
{
    return s_self->getX11Cursor(shape);
}

void Cursor::doSetPos()
{
    emit posChanged(m_pos);
}

void Cursor::doGetPos()
{
}

void Cursor::updatePos(const QPoint &pos)
{
    if (m_pos == pos) {
        return;
    }
    m_pos = pos;
    emit posChanged(m_pos);
}

void Cursor::startMousePolling()
{
    ++m_mousePollingCounter;
    if (m_mousePollingCounter == 1) {
        doStartMousePolling();
    }
}

void Cursor::stopMousePolling()
{
    Q_ASSERT(m_mousePollingCounter > 0);
    --m_mousePollingCounter;
    if (m_mousePollingCounter == 0) {
        doStopMousePolling();
    }
}

void Cursor::doStartMousePolling()
{
}

void Cursor::doStopMousePolling()
{
}

void Cursor::startCursorTracking()
{
    ++m_cursorTrackingCounter;
    if (m_cursorTrackingCounter == 1) {
        doStartCursorTracking();
    }
}

void Cursor::stopCursorTracking()
{
    Q_ASSERT(m_cursorTrackingCounter > 0);
    --m_cursorTrackingCounter;
    if (m_cursorTrackingCounter == 0) {
        doStopCursorTracking();
    }
}

void Cursor::doStartCursorTracking()
{
}

void Cursor::doStopCursorTracking()
{
}

void Cursor::notifyCursorChanged(uint32_t serial)
{
    if (m_cursorTrackingCounter <= 0) {
        // cursor change tracking is currently disabled, so don't emit signal
        return;
    }
    emit cursorChanged(serial);
}

X11Cursor::X11Cursor(QObject *parent)
    : Cursor(parent)
    , m_timeStamp(XCB_TIME_CURRENT_TIME)
    , m_buttonMask(0)
    , m_resetTimeStampTimer(new QTimer(this))
    , m_mousePollingTimer(new QTimer(this))
{
    m_resetTimeStampTimer->setSingleShot(true);
    connect(m_resetTimeStampTimer, SIGNAL(timeout()), SLOT(resetTimeStamp()));
    // TODO: How often do we really need to poll?
    m_mousePollingTimer->setInterval(50);
    connect(m_mousePollingTimer, SIGNAL(timeout()), SLOT(mousePolled()));
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
            m_timeStamp == QX11Info::appTime()) {
        // time stamps did not change, no need to query again
        return;
    }
    m_timeStamp = QX11Info::appTime();
    ScopedCPointer<xcb_query_pointer_reply_t> pointer(xcb_query_pointer_reply(connection(),
        xcb_query_pointer_unchecked(connection(), rootWindow()), NULL));
    if (!pointer) {
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

void X11Cursor::doStartMousePolling()
{
    m_mousePollingTimer->start();
}

void X11Cursor::doStopMousePolling()
{
    m_mousePollingTimer->stop();
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

xcb_cursor_t X11Cursor::getX11Cursor(Qt::CursorShape shape)
{
    QHash<Qt::CursorShape, xcb_cursor_t>::const_iterator it = m_cursors.constFind(shape);
    if (it != m_cursors.constEnd()) {
        return it.value();
    }
    return createCursor(shape);
}

xcb_cursor_t X11Cursor::createCursor(Qt::CursorShape shape)
{
    const QByteArray name = cursorName(shape);
    if (name.isEmpty()) {
        return XCB_CURSOR_NONE;
    }
    // XCursor is an XLib only lib
    const char *theme = XcursorGetTheme(display());
    const int size = XcursorGetDefaultSize(display());
    XcursorImage *ximg = XcursorLibraryLoadImage(name.constData(), theme, size);
    if (!ximg) {
        return XCB_CURSOR_NONE;
    }
    xcb_cursor_t cursor = XcursorImageLoadCursor(display(), ximg);
    XcursorImageDestroy(ximg);
    m_cursors.insert(shape, cursor);
    return cursor;
}

QByteArray X11Cursor::cursorName(Qt::CursorShape shape) const
{
    switch (shape) {
    case Qt::ArrowCursor:
        return QByteArray("left_ptr");
    case Qt::UpArrowCursor:
        return QByteArray("up_arrow");
    case Qt::CrossCursor:
        return QByteArray("cross");
    case Qt::WaitCursor:
        return QByteArray("wait");
    case Qt::IBeamCursor:
        return QByteArray("ibeam");
    case Qt::SizeVerCursor:
        return QByteArray("size_ver");
    case Qt::SizeHorCursor:
        return QByteArray("size_hor");
    case Qt::SizeBDiagCursor:
        return QByteArray("size_bdiag");
    case Qt::SizeFDiagCursor:
        return QByteArray("size_fdiag");
    case Qt::SizeAllCursor:
        return QByteArray("size_all");
    case Qt::SplitVCursor:
        return QByteArray("split_v");
    case Qt::SplitHCursor:
        return QByteArray("split_h");
    case Qt::PointingHandCursor:
        return QByteArray("pointing_hand");
    case Qt::ForbiddenCursor:
        return QByteArray("forbidden");
    case Qt::OpenHandCursor:
        return QByteArray("openhand");
    case Qt::ClosedHandCursor:
        return QByteArray("closedhand");
    case Qt::WhatsThisCursor:
        return QByteArray("whats_this");
    case Qt::BusyCursor:
        return QByteArray("left_ptr_watch");
    case Qt::DragMoveCursor:
        return QByteArray("dnd-move");
    case Qt::DragCopyCursor:
        return QByteArray("dnd-copy");
    case Qt::DragLinkCursor:
        return QByteArray("dnd-link");
    default:
        return QByteArray();
    }
}

} // namespace
