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

#ifndef KWIN_NOTIFICATIONS_H
#define KWIN_NOTIFICATIONS_H

#include <knotification.h>
#include <stdlib.h>
#include <QString>
#include <QList>

namespace KWin
{

class Client;

class Notify
{
public:

    enum Event {
        Activate,
        Close,
        Minimize,
        UnMinimize,
        Maximize,
        UnMaximize,
        OnAllDesktops,
        NotOnAllDesktops,
        New,
        Delete,
        TransNew,
        TransDelete,
        ShadeUp,
        ShadeDown,
        MoveStart,
        MoveEnd,
        ResizeStart,
        ResizeEnd,
        DemandAttentionCurrent,
        DemandAttentionOther,
        CompositingSuspendedDbus,
        FullScreen,
        UnFullScreen,
        DesktopChange = 100
    };

    static bool raise(Event, const QString& message = QString(), Client* c = NULL);
    static void sendPendingEvents();
private:
    struct EventData {
        QString event;
        QString message;
        long window;
        KNotification::NotificationFlags flags;
    };
    static QList< EventData > pending_events;
};

} // namespace

#endif
