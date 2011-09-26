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

#include "notifications.h"
#include <knotification.h>

#include "client.h"

namespace KWin
{

static bool forgetIt = false;
QList< Notify::EventData > Notify::pending_events;

bool Notify::raise(Event e, const QString& message, Client* c)
{
    if (forgetIt)
        return false; // no connection was possible, don't try each time

    QString event;
    KNotification::NotificationFlags flags = KNotification::CloseOnTimeout;
    switch(e) {
    case Activate:
        event = "activate";
        break;
    case Close:
        event = "close";
        break;
    case Minimize:
        event = "minimize";
        break;
    case UnMinimize:
        event = "unminimize";
        break;
    case Maximize:
        event = "maximize";
        break;
    case UnMaximize:
        event = "unmaximize";
        break;
    case OnAllDesktops:
        event = "on_all_desktops";
        break;
    case NotOnAllDesktops:
        event = "not_on_all_desktops";
        break;
    case New:
        event = "new";
        break;
    case Delete:
        event = "delete";
        break;
    case TransNew:
        event = "transnew";
        break;
    case TransDelete:
        event = "transdelete";
        break;
    case ShadeUp:
        event = "shadeup";
        break;
    case ShadeDown:
        event = "shadedown";
        break;
    case MoveStart:
        event = "movestart";
        break;
    case MoveEnd:
        event = "moveend";
        break;
    case ResizeStart:
        event = "resizestart";
        break;
    case ResizeEnd:
        event = "resizeend";
        break;
    case DemandAttentionCurrent:
        event = "demandsattentioncurrent";
        break;
    case DemandAttentionOther:
        event = "demandsattentionother";
        break;
    case CompositingSuspendedDbus:
        event = "compositingsuspendeddbus";
        break;
    case TilingLayoutChanged:
        event = "tilinglayoutchanged";
        break;
    default:
        if ((e > DesktopChange) && (e <= DesktopChange + 20)) {
            event = QString("desktop%1").arg(e - DesktopChange);
        }
        break;
    }
    if (event.isNull())
        return false;

// There may be a deadlock if KNotify event is sent while KWin has X grabbed.
// If KNotify is not running, KLauncher may do X requests (startup notification, whatever)
// that will block it. And KNotifyClient waits for the launch to succeed, which means
// KLauncher waits for X and KWin waits for KLauncher. So postpone events in such case.
    if (grabbedXServer()) {
        EventData data;
        data.event = event;
        data.message = message;
        data.window = c ? c->window() : 0;
        data.flags = flags;
        pending_events.append(data);
        return true;
    }


    return KNotification::event(event, message, QPixmap(), NULL /* TODO c ? c->window() : 0*/, flags);
}

void Notify::sendPendingEvents()
{
    while (!pending_events.isEmpty()) {
        EventData data = pending_events.first();
        pending_events.pop_front();
        if (!forgetIt)
            KNotification::event(data.event, data.message, QPixmap(), NULL /* TODO data.window*/, data.flags);
    }
}

} // namespace
