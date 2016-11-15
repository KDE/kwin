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
#include "killwindow.h"
#include "client.h"
#include "cursor.h"
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

KillWindow::KillWindow()
    : m_active(false)
{
}

KillWindow::~KillWindow()
{
}

void KillWindow::start()
{
    static xcb_cursor_t kill_cursor = XCB_CURSOR_NONE;
    if (kill_cursor == XCB_CURSOR_NONE) {
        kill_cursor = createCursor();
    }
    if (m_active) {
        return;
    }

    xcb_connection_t *c = connection();
    ScopedCPointer<xcb_grab_pointer_reply_t> grabPointer(xcb_grab_pointer_reply(c, xcb_grab_pointer_unchecked(c, false, rootWindow(),
        XCB_EVENT_MASK_BUTTON_PRESS | XCB_EVENT_MASK_BUTTON_RELEASE |
        XCB_EVENT_MASK_POINTER_MOTION |
        XCB_EVENT_MASK_ENTER_WINDOW | XCB_EVENT_MASK_LEAVE_WINDOW,
        XCB_GRAB_MODE_ASYNC, XCB_GRAB_MODE_ASYNC, XCB_WINDOW_NONE,
        kill_cursor, XCB_TIME_CURRENT_TIME), NULL));
    if (grabPointer.isNull() || grabPointer->status != XCB_GRAB_STATUS_SUCCESS) {
        return;
    }
    m_active = grabXKeyboard();
    if (!m_active) {
        xcb_ungrab_pointer(connection(), XCB_TIME_CURRENT_TIME);
        return;
    }
    grabXServer();
}

xcb_cursor_t KillWindow::createCursor()
{
    xcb_cursor_t cursor = Cursor::x11Cursor(QByteArrayLiteral("pirate"));
    if (cursor != XCB_CURSOR_NONE) {
        return cursor;
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
    return cursor;
}

bool KillWindow::isResponsibleForEvent(int eventType) const
{
    switch (eventType) {
        case XCB_BUTTON_PRESS:
        case XCB_BUTTON_RELEASE:
        case XCB_MOTION_NOTIFY:
        case XCB_ENTER_NOTIFY:
        case XCB_LEAVE_NOTIFY:
        case XCB_KEY_PRESS:
        case XCB_KEY_RELEASE:
        case XCB_FOCUS_IN:
        case XCB_FOCUS_OUT:
            return true;
        default:
            return false;
    }
}

void KillWindow::processEvent(xcb_generic_event_t *event)
{
    if (event->response_type == XCB_BUTTON_RELEASE) {
        xcb_button_release_event_t *buttonEvent = reinterpret_cast<xcb_button_release_event_t*>(event);
        handleButtonRelease(buttonEvent->detail, buttonEvent->child);
    } else if (event->response_type == XCB_KEY_PRESS) {
        xcb_key_press_event_t *keyEvent = reinterpret_cast<xcb_key_press_event_t*>(event);
        handleKeyPress(keyEvent->detail, keyEvent->state);
    }
}

void KillWindow::handleButtonRelease(xcb_button_t button, xcb_window_t window)
{
    if (button == XCB_BUTTON_INDEX_3) {
        release();
        return;
    }
    if (button == XCB_BUTTON_INDEX_1 || button == XCB_BUTTON_INDEX_2) {
        killWindowId(window);
        release();
        return;
    }
}

void KillWindow::handleKeyPress(xcb_keycode_t keycode, uint16_t state)
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
        performKill();
    }
    if (returnPressed || escapePressed) {
        release();
    }
    xcb_key_symbols_free(symbols);
}

void KillWindow::performKill()
{
    Xcb::Pointer pointer(rootWindow());
    if (!pointer.isNull() && pointer->child != XCB_WINDOW_NONE) {
        killWindowId(pointer->child);
    }
}

void KillWindow::release()
{
    ungrabXKeyboard();
    xcb_ungrab_pointer(connection(), XCB_TIME_CURRENT_TIME);
    ungrabXServer();
    m_active = false;
}

void KillWindow::killWindowId(xcb_window_t window_to_kill)
{
    if (window_to_kill == XCB_WINDOW_NONE)
        return;
    xcb_window_t window = window_to_kill;
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
    if (client)
        client->killWindow();
    else
        xcb_kill_client(connection(), window_to_kill);
}

} // namespace
