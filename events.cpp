 /*****************************************************************
kwin - the KDE window manager

Copyright (C) 1999, 2000 Matthias Ettrich <ettrich@kde.org>
******************************************************************/
#include "events.h"
#include <knotifyclient.h>

void Events::raise( Event e )
{
    static bool forgetIt = FALSE;
    if ( forgetIt )
	return; // no connection was possible, don't try each time

    QString event;
    switch ( e ) {
    case Activate:
	event = "activate";
	break;
    case Close:
	event = "close";
	break;
    case Iconify:
	event = "iconify";
	break;
    case DeIconify:
	event = "deiconify";
	break;
    case Maximize:
	event = "maximize";
	break;
    case UnMaximize:
	event = "unmaximize";
	break;
    case Sticky:
	event = "sticky";
	break;
    case UnSticky:
	event = "unsticky";
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
        if ((e > DesktopChange) && (e <= DesktopChange+16))
        {
           event = QString("desktop%1").arg(e-DesktopChange);
        }
        break;
    }

    if ( !event )
	return;

    forgetIt= !KNotifyClient::event( event );
}
