/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 1999, 2000 Matthias Ettrich <ettrich@kde.org>
Copyright (C) 2003 Lubos Lunak <l.lunak@kde.org>

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

/*

 This file is for (very) small utility functions/classes.

*/

#include "utils.h"

#include <QWidget>
#include <kkeyserver.h>

#ifndef KCMRULES
#include <assert.h>
#include <QApplication>
#include <QDebug>

#include <X11/Xlib.h>

#include <stdio.h>

#include "atoms.h"
#include "cursor.h"
#include "workspace.h"

#endif

namespace KWin
{

#ifndef KCMRULES

//************************************
// StrutRect
//************************************

StrutRect::StrutRect(QRect rect, StrutArea area)
    : QRect(rect)
    , m_area(area)
{
}

StrutRect::StrutRect(const StrutRect& other)
    : QRect(other)
    , m_area(other.area())
{
}

#endif

QByteArray getStringProperty(xcb_window_t w, xcb_atom_t prop, char separator)
{
    const xcb_get_property_cookie_t c = xcb_get_property_unchecked(connection(), false, w, prop,
                                                                   XCB_ATOM_STRING, 0, 10000);
    ScopedCPointer<xcb_get_property_reply_t> property(xcb_get_property_reply(connection(), c, NULL));
    if (property.isNull() || property->type == XCB_ATOM_NONE) {
        return QByteArray();
    }
    char *data = static_cast<char*>(xcb_get_property_value(property.data()));
    int length = property->value_len;
    if (data && separator) {
        for (uint32_t i = 0; i < property->value_len; ++i) {
            if (!data[i] && i + 1 < property->value_len) {
                data[i] = separator;
            } else {
                length = i;
            }
        }
    }
    return QByteArray(data, length);
}

#ifndef KCMRULES
/*
 Updates xTime(). This used to simply fetch current timestamp from the server,
 but that can cause xTime() to be newer than timestamp of events that are
 still in our events queue, thus e.g. making XSetInputFocus() caused by such
 event to be ignored. Therefore events queue is searched for first
 event with timestamp, and extra PropertyNotify is generated in order to make
 sure such event is found.
*/
void updateXTime()
{
    // NOTE: QX11Info::getTimestamp does not yet search the event queue as the old
    // solution did. This means there might be regressions currently. See the
    // documentation above on how it should be done properly.
    QX11Info::setAppTime(QX11Info::getTimestamp());
}

static int server_grab_count = 0;

void grabXServer()
{
    if (++server_grab_count == 1)
        xcb_grab_server(connection());
}

void ungrabXServer()
{
    assert(server_grab_count > 0);
    if (--server_grab_count == 0) {
        xcb_ungrab_server(connection());
        xcb_flush(connection());
    }
}

bool grabbedXServer()
{
    return server_grab_count > 0;
}

static bool keyboard_grabbed = false;

bool grabXKeyboard(xcb_window_t w)
{
    if (QWidget::keyboardGrabber() != NULL)
        return false;
    if (keyboard_grabbed)
        return false;
    if (qApp->activePopupWidget() != NULL)
        return false;
    if (w == XCB_WINDOW_NONE)
        w = rootWindow();
    const xcb_grab_keyboard_cookie_t c = xcb_grab_keyboard_unchecked(connection(), false, w, xTime(),
                                                                     XCB_GRAB_MODE_ASYNC, XCB_GRAB_MODE_ASYNC);
    ScopedCPointer<xcb_grab_keyboard_reply_t> grab(xcb_grab_keyboard_reply(connection(), c, NULL));
    if (grab.isNull()) {
        return false;
    }
    if (grab->status != XCB_GRAB_STATUS_SUCCESS) {
        return false;
    }
    keyboard_grabbed = true;
    return true;
}

void ungrabXKeyboard()
{
    if (!keyboard_grabbed) {
        // grabXKeyboard() may fail sometimes, so don't fail, but at least warn anyway
        qDebug() << "ungrabXKeyboard() called but keyboard not grabbed!";
    }
    keyboard_grabbed = false;
    xcb_ungrab_keyboard(connection(), XCB_TIME_CURRENT_TIME);
}

QPoint cursorPos()
{
    return Cursor::self()->pos();
}
#endif

// converting between X11 mouse/keyboard state mask and Qt button/keyboard states

int qtToX11Button(Qt::MouseButton button)
{
    if (button == Qt::LeftButton)
        return XCB_BUTTON_INDEX_1;
    else if (button == Qt::MidButton)
        return XCB_BUTTON_INDEX_2;
    else if (button == Qt::RightButton)
        return XCB_BUTTON_INDEX_3;
    return XCB_BUTTON_INDEX_ANY; // 0
}

Qt::MouseButton x11ToQtMouseButton(int button)
{
    if (button == XCB_BUTTON_INDEX_1)
        return Qt::LeftButton;
    if (button == XCB_BUTTON_INDEX_2)
        return Qt::MidButton;
    if (button == XCB_BUTTON_INDEX_3)
        return Qt::RightButton;
    if (button == XCB_BUTTON_INDEX_4)
        return Qt::XButton1;
    if (button == XCB_BUTTON_INDEX_5)
        return Qt::XButton2;
    return Qt::NoButton;
}

int qtToX11State(Qt::MouseButtons buttons, Qt::KeyboardModifiers modifiers)
{
    int ret = 0;
    if (buttons & Qt::LeftButton)
        ret |= XCB_KEY_BUT_MASK_BUTTON_1;
    if (buttons & Qt::MidButton)
        ret |= XCB_KEY_BUT_MASK_BUTTON_2;
    if (buttons & Qt::RightButton)
        ret |= XCB_KEY_BUT_MASK_BUTTON_3;
    if (modifiers & Qt::ShiftModifier)
        ret |= XCB_KEY_BUT_MASK_SHIFT;
    if (modifiers & Qt::ControlModifier)
        ret |= XCB_KEY_BUT_MASK_CONTROL;
    if (modifiers & Qt::AltModifier)
        ret |= KKeyServer::modXAlt();
    if (modifiers & Qt::MetaModifier)
        ret |= KKeyServer::modXMeta();
    return ret;
}

Qt::MouseButtons x11ToQtMouseButtons(int state)
{
    Qt::MouseButtons ret = 0;
    if (state & XCB_KEY_BUT_MASK_BUTTON_1)
        ret |= Qt::LeftButton;
    if (state & XCB_KEY_BUT_MASK_BUTTON_2)
        ret |= Qt::MidButton;
    if (state & XCB_KEY_BUT_MASK_BUTTON_3)
        ret |= Qt::RightButton;
    if (state & XCB_KEY_BUT_MASK_BUTTON_4)
        ret |= Qt::XButton1;
    if (state & XCB_KEY_BUT_MASK_BUTTON_5)
        ret |= Qt::XButton2;
    return ret;
}

Qt::KeyboardModifiers x11ToQtKeyboardModifiers(int state)
{
    Qt::KeyboardModifiers ret = 0;
    if (state & XCB_KEY_BUT_MASK_SHIFT)
        ret |= Qt::ShiftModifier;
    if (state & XCB_KEY_BUT_MASK_CONTROL)
        ret |= Qt::ControlModifier;
    if (state & KKeyServer::modXAlt())
        ret |= Qt::AltModifier;
    if (state & KKeyServer::modXMeta())
        ret |= Qt::MetaModifier;
    return ret;
}

} // namespace

#ifndef KCMRULES
#include "utils.moc"
#endif
