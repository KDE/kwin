/*****************************************************************
kwin - the KDE window manager

Copyright (C) 1999, 2000 Matthias Ettrich <ettrich@kde.org>
******************************************************************/

#ifndef EVENTS_H
#define EVENTS_H

class Events
{
public:

    enum Event {
	Activate,
	Close,
	Iconify,
	DeIconify,
	Maximize,
	UnMaximize,
	Sticky,
	UnSticky,
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
	DesktopChange = 100
    };
	
    static void raise( Event );
};

#endif
