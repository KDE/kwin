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

namespace KWinInternal
{

void Notify::raise( Event e )
    {
    static bool forgetIt = FALSE;
    if ( forgetIt )
        return; // no connection was possible, don't try each time

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
        default:
            if ((e > DesktopChange) && (e <= DesktopChange+20))
            {
            event = QString("desktop%1").arg(e-DesktopChange);
            }
        break;
        }

    if ( !event )
        return;

    forgetIt= !KNotifyClient::event( 0, event, event );
    }

} // namespace
