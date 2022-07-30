/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2017 Martin Flöser <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "x11_standalone_screenedges_filter.h"
#include "atoms.h"
#include "screenedge.h"
#include "workspace.h"

#include <QWidget>
#include <xcb/xcb.h>

namespace KWin
{

ScreenEdgesFilter::ScreenEdgesFilter()
    : X11EventFilter(QVector<int>{XCB_MOTION_NOTIFY, XCB_ENTER_NOTIFY, XCB_CLIENT_MESSAGE})
{
}

bool ScreenEdgesFilter::event(xcb_generic_event_t *event)
{
    const uint8_t eventType = event->response_type & ~0x80;
    switch (eventType) {
    case XCB_MOTION_NOTIFY: {
        const auto mouseEvent = reinterpret_cast<xcb_motion_notify_event_t *>(event);
        const QPoint rootPos(mouseEvent->root_x, mouseEvent->root_y);
        if (QWidget::mouseGrabber()) {
            workspace()->screenEdges()->check(rootPos, QDateTime::fromMSecsSinceEpoch(xTime(), Qt::UTC), true);
        } else {
            workspace()->screenEdges()->check(rootPos, QDateTime::fromMSecsSinceEpoch(mouseEvent->time, Qt::UTC));
        }
        // not filtered out
        break;
    }
    case XCB_ENTER_NOTIFY: {
        const auto enter = reinterpret_cast<xcb_enter_notify_event_t *>(event);
        return workspace()->screenEdges()->handleEnterNotifiy(enter->event, QPoint(enter->root_x, enter->root_y), QDateTime::fromMSecsSinceEpoch(enter->time, Qt::UTC));
    }
    case XCB_CLIENT_MESSAGE: {
        const auto ce = reinterpret_cast<xcb_client_message_event_t *>(event);
        if (ce->type != atoms->xdnd_position) {
            return false;
        }
        return workspace()->screenEdges()->handleDndNotify(ce->window, QPoint(ce->data.data32[2] >> 16, ce->data.data32[2] & 0xffff));
    }
    }
    return false;
}

}
