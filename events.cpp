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
    case Close:
	event = "Window Close";
	break;
    case Iconify:
	event = "Window Iconify";
	break;
    case DeIconify:
	event = "Window DeIconify";
	break;
    case Maximize:
	event = "Window Maximize";
	break;
    case UnMaximize:
	event = "Window UnMaximize";
	break;
    case Sticky:
	event = "Window Sticky";
	break;
    case UnSticky:
	event = "Window UnSticky";
	break;
    case New:
	event = "Window New";
	break;
    case Delete:
	event = "Window Delete";
	break;
    case TransNew:
	event = "Window TransNew";
	break;
    case TransDelete:
	event = "Window TransDelete";
	break;
    case ShadeUp:
	event = "Window ShadeUp";
	break;
    case ShadeDown:
	event = "Window ShadeDown";
	break;
    case MoveStart:
	event = "Window Move Start";
	break;
    case MoveEnd:
	event = "Window Move End";
	break;
    case ResizeStart:
	event = "Window Resize Start";
	break;
    case ResizeEnd:
	event = "Window Resize End";
	break;
    }
    if ( !event )
	return;
    

    if ( !KNotifyClient::event( event ) )
	forgetIt = TRUE;
}
