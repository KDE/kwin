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
#include "screenedges_filter.h"
#include "atoms.h"
#include "screenedge.h"

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
        const auto mouseEvent = reinterpret_cast<xcb_motion_notify_event_t*>(event);
        const QPoint rootPos(mouseEvent->root_x, mouseEvent->root_y);
        if (QWidget::mouseGrabber()) {
            ScreenEdges::self()->check(rootPos, QDateTime::fromMSecsSinceEpoch(xTime()), true);
        } else {
            ScreenEdges::self()->check(rootPos, QDateTime::fromMSecsSinceEpoch(mouseEvent->time));
        }
        // not filtered out
        break;
    }
    case XCB_ENTER_NOTIFY: {
        const auto enter = reinterpret_cast<xcb_enter_notify_event_t*>(event);
        return ScreenEdges::self()->handleEnterNotifiy(enter->event, QPoint(enter->root_x, enter->root_y), QDateTime::fromMSecsSinceEpoch(enter->time));
    }
    case XCB_CLIENT_MESSAGE: {
        const auto ce = reinterpret_cast<xcb_client_message_event_t*>(event);
        if (ce->type != atoms->xdnd_position) {
            return false;
        }
        return ScreenEdges::self()->handleDndNotify(ce->window, QPoint(ce->data.data32[2] >> 16, ce->data.data32[2] & 0xffff));
    }
    }
    return false;
}

}
