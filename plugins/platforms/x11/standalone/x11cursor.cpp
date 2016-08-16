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
#include "x11cursor.h"
#include "input.h"
#include "keyboard_input.h"
#include "utils.h"
#include "x11eventfilter.h"
#include "xcbutils.h"

#include <QAbstractEventDispatcher>
#include <QTimer>

#include <config-kwin.h>
#include <xcb/xcb_cursor.h>
#if HAVE_X11_XINPUT
#include <X11/extensions/XInput2.h>
#include <X11/extensions/XI2proto.h>
#else
#define XI_RawMotion 0
#endif
#include <fixx11h.h>

namespace KWin
{

#ifndef KCMRULES
class XInputEventFilter : public X11EventFilter
{
public:
    XInputEventFilter(X11Cursor *parent, int xi_opcode)
        : X11EventFilter(XCB_GE_GENERIC, xi_opcode, QVector<int>{XI_RawMotion, XI_RawButtonPress, XI_RawButtonRelease, XI_RawKeyPress, XI_RawKeyRelease})
        , m_x11Cursor(parent)
        {}
    virtual ~XInputEventFilter() = default;

    bool event(xcb_generic_event_t *event) override {
        xcb_ge_generic_event_t *ge = reinterpret_cast<xcb_ge_generic_event_t *>(event);
        switch (ge->event_type) {
        case XI_RawKeyPress:
            input()->keyboard()->xkb()->updateKey(reinterpret_cast<xXIRawEvent*>(event)->detail - 8, InputRedirection::KeyboardKeyPressed);
            break;
        case XI_RawKeyRelease:
            input()->keyboard()->xkb()->updateKey(reinterpret_cast<xXIRawEvent*>(event)->detail - 8, InputRedirection::KeyboardKeyReleased);
            break;
        default:
            m_x11Cursor->schedulePoll();
            break;
        }
        return false;
    }

private:
    X11Cursor *m_x11Cursor;
};
#endif

X11Cursor::X11Cursor(QObject *parent)
    : Cursor(parent)
    , m_timeStamp(XCB_TIME_CURRENT_TIME)
    , m_buttonMask(0)
    , m_resetTimeStampTimer(new QTimer(this))
    , m_mousePollingTimer(new QTimer(this))
    , m_hasXInput(false)
    , m_xiOpcode(0)
    , m_needsPoll(false)
{
    initXInput();
    m_resetTimeStampTimer->setSingleShot(true);
    connect(m_resetTimeStampTimer, SIGNAL(timeout()), SLOT(resetTimeStamp()));
    // TODO: How often do we really need to poll?
    m_mousePollingTimer->setInterval(50);
    connect(m_mousePollingTimer, SIGNAL(timeout()), SLOT(mousePolled()));

    connect(this, &Cursor::themeChanged, this, [this] { m_cursors.clear(); });
}

X11Cursor::~X11Cursor()
{
}

void X11Cursor::initXInput()
{
#ifndef KCMRULES
#if HAVE_X11_XINPUT
    if (qEnvironmentVariableIsSet("KWIN_NO_XI2")) {
        return;
    }
    Display *dpy = display();
    int xi_opcode, event, error;
    // init XInput extension
    if (!XQueryExtension(dpy, "XInputExtension", &xi_opcode, &event, &error)) {
        return;
    }

    // verify that the XInput extension is at at least version 2.0
    int major = 2, minor = 0;
    int result = XIQueryVersion(dpy, &major, &minor);
    if (result == BadImplementation) {
        // Xinput 2.2 returns BadImplementation if checked against 2.0
        major = 2;
        minor = 2;
        if (XIQueryVersion(dpy, &major, &minor) != Success) {
            return;
        }
    } else if (result != Success) {
        return;
    }
    m_hasXInput = true;
    m_xiOpcode = xi_opcode;
    input()->keyboard()->xkb()->reconfigure();
#endif
#endif
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
    if (m_hasXInput) {
#ifndef KCMRULES
#if HAVE_X11_XINPUT
        m_xiEventFilter.reset(new XInputEventFilter(this, m_xiOpcode));

        // this assumes KWin is the only one setting events on the root window
        // given Qt's source code this seems to be true. If it breaks, we need to change
        XIEventMask evmasks[1];
        unsigned char mask1[XIMaskLen(XI_LASTEVENT)];

        memset(mask1, 0, sizeof(mask1));

        XISetMask(mask1, XI_RawMotion);
        XISetMask(mask1, XI_RawButtonPress);
        XISetMask(mask1, XI_RawButtonRelease);
        XISetMask(mask1, XI_RawKeyPress);
        XISetMask(mask1, XI_RawKeyRelease);

        evmasks[0].deviceid = XIAllMasterDevices;
        evmasks[0].mask_len = sizeof(mask1);
        evmasks[0].mask = mask1;
        XISelectEvents(display(), rootWindow(), evmasks, 1);
        connect(qApp->eventDispatcher(), &QAbstractEventDispatcher::aboutToBlock, this, &X11Cursor::aboutToBlock);
#endif
#endif
    } else {
        m_mousePollingTimer->start();
    }
}

void X11Cursor::doStopMousePolling()
{
    if (m_hasXInput) {
#ifndef KCMRULES
#if HAVE_X11_XINPUT
        m_xiEventFilter.reset();

        XIEventMask evmasks[1];
        unsigned char mask1[(XI_LASTEVENT + 7)/8];

        memset(mask1, 0, sizeof(mask1));

        evmasks[0].deviceid = XIAllMasterDevices;
        evmasks[0].mask_len = sizeof(mask1);
        evmasks[0].mask = mask1;
        XISelectEvents(display(), rootWindow(), evmasks, 1);
        disconnect(qApp->eventDispatcher(), &QAbstractEventDispatcher::aboutToBlock, this, &X11Cursor::aboutToBlock);
#endif
#endif
    } else {
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

xcb_cursor_t X11Cursor::getX11Cursor(Qt::CursorShape shape)
{
    return getX11Cursor(cursorName(shape));
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
    if (xcb_cursor_context_new(connection(), defaultScreen(), &ctx) < 0) {
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

}
