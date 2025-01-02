/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2017 Martin Flöser <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "moving_client_x11_filter.h"
#include "workspace.h"
#include "x11window.h"
#include <KKeyServer>
#include <xcb/xcb.h>

namespace KWin
{

MovingClientX11Filter::MovingClientX11Filter()
    : X11EventFilter(QList<int>{XCB_KEY_PRESS, XCB_KEY_RELEASE, XCB_MOTION_NOTIFY, XCB_BUTTON_PRESS, XCB_BUTTON_RELEASE})
{
}

bool MovingClientX11Filter::event(xcb_generic_event_t *event)
{
    auto client = dynamic_cast<X11Window *>(workspace()->moveResizeWindow());
    if (!client) {
        return false;
    }
    auto testWindow = [client, event](xcb_window_t window) {
        return client->frameId() == window && client->windowEvent(event);
    };

    const uint8_t eventType = event->response_type & ~0x80;
    switch (eventType) {
    case XCB_KEY_PRESS: {
        int keyQt;
        xcb_key_press_event_t *keyEvent = reinterpret_cast<xcb_key_press_event_t *>(event);
        KKeyServer::xcbKeyPressEventToQt(keyEvent, &keyQt);
        client->keyPressEvent(keyQt, keyEvent->time);
        if (client->isInteractiveMove() || client->isInteractiveResize()) {
            const QPointF global = QPointF(Xcb::fromXNative(keyEvent->root_x), Xcb::fromXNative(keyEvent->root_y));
            client->updateInteractiveMoveResize(global, x11ToQtKeyboardModifiers(keyEvent->state));
        }
        return true;
    }
    case XCB_KEY_RELEASE: {
        if (client->isInteractiveMove() || client->isInteractiveResize()) {
            auto *keyEvent = reinterpret_cast<xcb_key_release_event_t *>(event);
            const QPointF global = QPointF(Xcb::fromXNative(keyEvent->root_x), Xcb::fromXNative(keyEvent->root_y));
            client->updateInteractiveMoveResize(global, x11ToQtKeyboardModifiers(keyEvent->state));
        }
        return true;
    }
    case XCB_BUTTON_PRESS:
    case XCB_BUTTON_RELEASE:
        return testWindow(reinterpret_cast<xcb_button_press_event_t *>(event)->event);
    case XCB_MOTION_NOTIFY:
        return testWindow(reinterpret_cast<xcb_motion_notify_event_t *>(event)->event);
    }
    return false;
}

}
