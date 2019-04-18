/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2017 Martin Flöser <mgraesslin@kde.org>

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
#include "moving_client_x11_filter.h"
#include "client.h"
#include "workspace.h"
#include <KKeyServer>
#include <xcb/xcb.h>

namespace KWin
{

MovingClientX11Filter::MovingClientX11Filter()
    : X11EventFilter(QVector<int>{XCB_KEY_PRESS, XCB_MOTION_NOTIFY, XCB_BUTTON_PRESS, XCB_BUTTON_RELEASE})
{
}

bool MovingClientX11Filter::event(xcb_generic_event_t *event)
{
    auto client = dynamic_cast<Client*>(workspace()->moveResizeClient());
    if (!client) {
        return false;
    }
    auto testWindow = [client, event] (xcb_window_t window) {
        return client->moveResizeGrabWindow() == window && client->windowEvent(event);
    };

    const uint8_t eventType = event->response_type & ~0x80;
    switch (eventType) {
    case XCB_KEY_PRESS: {
        int keyQt;
        xcb_key_press_event_t *keyEvent = reinterpret_cast<xcb_key_press_event_t*>(event);
        KKeyServer::xcbKeyPressEventToQt(keyEvent, &keyQt);
        client->keyPressEvent(keyQt, keyEvent->time);
        return true;
    }
    case XCB_BUTTON_PRESS:
    case XCB_BUTTON_RELEASE:
        return testWindow(reinterpret_cast<xcb_button_press_event_t*>(event)->event);
    case XCB_MOTION_NOTIFY:
        return testWindow(reinterpret_cast<xcb_motion_notify_event_t*>(event)->event);
    }
    return false;
}

}
