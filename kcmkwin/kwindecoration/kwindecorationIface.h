/* 
	This is the new kwindecoration kcontrol module

	Copyright (c) 2001
		Karol Szwed (gallium) <karlmail@usa.net>
		http://gallium.n3.net/

	Supports new kwin configuration plugins, and titlebar button position
	modification via dnd interface.

	Based on original "kwintheme" (Window Borders) 
	Copyright (C) 2001 Rik Hemsley (rikkus) <rik@kde.org>
*/

#ifndef __KWINDECORATIONIFACE_H
#define __KWINDECORATIONIFACE_H

#include <dcopobject.h>

class KWinDecorationIface: virtual public DCOPObject
{
	K_DCOP
	public:

	k_dcop:
		virtual void dcopUpdateClientList()=0;
};

#endif
