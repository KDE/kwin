/*****************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 1999, 2000 Matthias Ettrich <ettrich@kde.org>
Copyright (C) 2003 Lubos Lunak <l.lunak@kde.org>

You can Freely distribute this program under the GNU General Public
License. See the file "COPYING" for the exact licensing terms.
******************************************************************/

#ifndef KWIN_NOTIFICATIONS_H
#define KWIN_NOTIFICATIONS_H

#include <stdlib.h>
#include <QString>
#include <QList>

namespace KWinInternal
{

class Client;

class Notify
    {
    public:

        enum Event 
            {
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
            DesktopChange = 100
            };

        static bool raise( Event, const QString& message = QString(), Client* c = NULL );
        static bool makeDemandAttention( Event );
        static void sendPendingEvents();
    private:
        static QString eventToName( Event );
        struct EventData
            {
            QString event;
            QString message;
            long window;
            };
        static QList< EventData > pending_events;
    };

} // namespace

#endif
