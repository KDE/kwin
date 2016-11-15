/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 1999, 2000 Matthias Ettrich <ettrich@kde.org>
Copyright (C) 2003 Lubos Lunak <l.lunak@kde.org>
Copyright (C) 2012 Martin Gräßlin <mgraesslin@kde.org>

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
#include "windowselector.h"
#include "client.h"
#include "cursor.h"
#include "unmanaged.h"
#include "workspace.h"
#include "xcbutils.h"
// XLib
#include <X11/cursorfont.h>
#include <X11/Xutil.h>
#include <fixx11h.h>
// XCB
#include <xcb/xcb_keysyms.h>

namespace KWin
{

WindowSelector::WindowSelector()
    : X11EventFilter(QVector<int>{XCB_BUTTON_PRESS,
                                  XCB_BUTTON_RELEASE,
                                  XCB_MOTION_NOTIFY,
                                  XCB_ENTER_NOTIFY,
                                  XCB_LEAVE_NOTIFY,
                                  XCB_KEY_PRESS,
                                  XCB_KEY_RELEASE,
                                  XCB_FOCUS_IN,
                                  XCB_FOCUS_OUT
    })
    , m_active(false)
{
}

WindowSelector::~WindowSelector()
{
}

void WindowSelector::start(std::function<void(KWin::Toplevel*)> callback, const QByteArray &cursorName)
{
    xcb_cursor_t cursor = createCursor(cursorName);
    if (m_active) {
        callback(nullptr);
        return;
    }

    xcb_connection_t *c = connection();
    ScopedCPointer<xcb_grab_pointer_reply_t> grabPointer(xcb_grab_pointer_reply(c, xcb_grab_pointer_unchecked(c, false, rootWindow(),
        XCB_EVENT_MASK_BUTTON_PRESS | XCB_EVENT_MASK_BUTTON_RELEASE |
        XCB_EVENT_MASK_POINTER_MOTION |
        XCB_EVENT_MASK_ENTER_WINDOW | XCB_EVENT_MASK_LEAVE_WINDOW,
        XCB_GRAB_MODE_ASYNC, XCB_GRAB_MODE_ASYNC, XCB_WINDOW_NONE,
        cursor, XCB_TIME_CURRENT_TIME), NULL));
    if (grabPointer.isNull() || grabPointer->status != XCB_GRAB_STATUS_SUCCESS) {
        callback(nullptr);
        return;
    }
    m_active = grabXKeyboard();
    if (!m_active) {
        xcb_ungrab_pointer(connection(), XCB_TIME_CURRENT_TIME);
        callback(nullptr);
        return;
    }
    grabXServer();
    m_callback = callback;
}

xcb_cursor_t WindowSelector::createCursor(const QByteArray &cursorName)
{
    if (cursorName.isEmpty()) {
        return Cursor::x11Cursor(Qt::CrossCursor);
    }
    xcb_cursor_t cursor = Cursor::x11Cursor(cursorName);
    if (cursor != XCB_CURSOR_NONE) {
        return cursor;
    }
    if (cursorName == QByteArrayLiteral("pirate")) {
        // special handling for font pirate cursor
        static xcb_cursor_t kill_cursor = XCB_CURSOR_NONE;
        if (kill_cursor != XCB_CURSOR_NONE) {
            return kill_cursor;
        }
        // fallback on font
        xcb_connection_t *c = connection();
        const xcb_font_t cursorFont = xcb_generate_id(c);
        xcb_open_font(c, cursorFont, strlen ("cursor"), "cursor");
        cursor = xcb_generate_id(c);
        xcb_create_glyph_cursor(c, cursor, cursorFont, cursorFont,
                                XC_pirate,         /* source character glyph */
                                XC_pirate + 1,     /* mask character glyph */
                                0, 0, 0, 0, 0, 0);  /* r b g r b g */
        kill_cursor = cursor;
    }
    return cursor;
}

void WindowSelector::processEvent(xcb_generic_event_t *event)
{
    if (event->response_type == XCB_BUTTON_RELEASE) {
        xcb_button_release_event_t *buttonEvent = reinterpret_cast<xcb_button_release_event_t*>(event);
        handleButtonRelease(buttonEvent->detail, buttonEvent->child);
    } else if (event->response_type == XCB_KEY_PRESS) {
        xcb_key_press_event_t *keyEvent = reinterpret_cast<xcb_key_press_event_t*>(event);
        handleKeyPress(keyEvent->detail, keyEvent->state);
    }
}

bool WindowSelector::event(xcb_generic_event_t *event)
{
    if (!m_active) {
        return false;
    }
    processEvent(event);

    return true;
}

void WindowSelector::handleButtonRelease(xcb_button_t button, xcb_window_t window)
{
    if (button == XCB_BUTTON_INDEX_3) {
        m_callback(nullptr);
        release();
        return;
    }
    if (button == XCB_BUTTON_INDEX_1 || button == XCB_BUTTON_INDEX_2) {
        selectWindowId(window);
        release();
        return;
    }
}

void WindowSelector::handleKeyPress(xcb_keycode_t keycode, uint16_t state)
{
    xcb_key_symbols_t *symbols = xcb_key_symbols_alloc(connection());
    xcb_keysym_t kc = xcb_key_symbols_get_keysym(symbols, keycode, 0);
    int mx = 0;
    int my = 0;
    const bool returnPressed = (kc == XK_Return) || (kc == XK_space);
    const bool escapePressed = (kc == XK_Escape);
    if (kc == XK_Left) {
        mx = -10;
    }
    if (kc == XK_Right) {
        mx = 10;
    }
    if (kc == XK_Up) {
        my = -10;
    }
    if (kc == XK_Down) {
        my = 10;
    }
    if (state & XCB_MOD_MASK_CONTROL) {
        mx /= 10;
        my /= 10;
    }
    Cursor::setPos(Cursor::pos() + QPoint(mx, my));
    if (returnPressed) {
        selectWindowUnderPointer();
    }
    if (returnPressed || escapePressed) {
        if (escapePressed) {
            m_callback(nullptr);
        }
        release();
    }
    xcb_key_symbols_free(symbols);
}

void WindowSelector::selectWindowUnderPointer()
{
    Xcb::Pointer pointer(rootWindow());
    if (!pointer.isNull() && pointer->child != XCB_WINDOW_NONE) {
        selectWindowId(pointer->child);
    }
}

void WindowSelector::release()
{
    ungrabXKeyboard();
    xcb_ungrab_pointer(connection(), XCB_TIME_CURRENT_TIME);
    ungrabXServer();
    m_active = false;
    m_callback = std::function<void(KWin::Toplevel*)>();
}

void WindowSelector::selectWindowId(xcb_window_t window_to_select)
{
    if (window_to_select == XCB_WINDOW_NONE) {
        m_callback(nullptr);
        return;
    }
    xcb_window_t window = window_to_select;
    Client* client = NULL;
    while (true) {
        client = Workspace::self()->findClient(Predicate::FrameIdMatch, window);
        if (client) {
            break; // Found the client
        }
        Xcb::Tree tree(window);
        if (window == tree->root) {
            // We didn't find the client, probably an override-redirect window
            break;
        }
        window = tree->parent;  // Go up
    }
    if (client) {
        m_callback(client);
    } else {
        m_callback(Workspace::self()->findUnmanaged(window));
    }
}

} // namespace
