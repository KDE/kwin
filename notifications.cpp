/*****************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 1999, 2000 Matthias Ettrich <ettrich@kde.org>
Copyright (C) 2003 Lubos Lunak <l.lunak@kde.org>

You can Freely distribute this program under the GNU General Public
License. See the file "COPYING" for the exact licensing terms.
******************************************************************/

#include "notifications.h"
#include <knotifyclient.h>

#include "client.h"

namespace KWinInternal
{

QString Notify::eventToName( Event e )
    {
    QString event;
    switch ( e ) 
        {
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
        default:
            if ((e > DesktopChange) && (e <= DesktopChange+20))
            {
            event = QString("desktop%1").arg(e-DesktopChange);
            }
        break;
        }
    return event;
    }

static bool forgetIt = FALSE;
QList< Notify::EventData > Notify::pending_events;

bool Notify::raise( Event e, const QString& message, Client* c )
    {
    if ( forgetIt )
        return false; // no connection was possible, don't try each time

    QString event = eventToName( e );
    if ( event.isNull() )
        return false;

// There may be a deadlock if KNotify event is sent while KWin has X grabbed.
// If KNotify is not running, KLauncher may do X requests (startup notification, whatever)
// that will block it. And KNotifyClient waits for the launch to succeed, which means
// KLauncher waits for X and KWin waits for KLauncher. So postpone events in such case.
    if( grabbedXServer())
        {
        EventData data;
        data.event = event;
        data.message = message;
        data.window = c ? c->window() : 0;
        pending_events.append( data );
        return true;
        }

    forgetIt= !KNotifyClient::event( c ? c->window() : 0, event, message );
    return !forgetIt;
    }

void Notify::sendPendingEvents()
    {
    while( !pending_events.isEmpty())
        {
        EventData data = pending_events.first();
        pending_events.pop_front();
        if( !forgetIt )
            forgetIt= !KNotifyClient::event( data.window, data.event, data.message );
        }
    }

bool Notify::makeDemandAttention( Event e )
    {
    QString event = eventToName( e );
    if( event.isNull() )
        return false;
    int rep = KNotifyClient::getPresentation( event );
    if( rep == -1 )
        rep = KNotifyClient::getDefaultPresentation( event );
    return rep != -1 && ( rep & KNotifyClient::Taskbar );
    }

} // namespace
