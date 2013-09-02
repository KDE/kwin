/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2006 Lubos Lunak <l.lunak@kde.org>

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

#include "kwinglobals.h"

#include <config-X11.h>

#include <assert.h>

#include <X11/Xlib.h>
#include <X11/extensions/shape.h>

#include <X11/extensions/Xdamage.h>
#include <X11/extensions/Xrandr.h>
#ifdef HAVE_XSYNC
#include <X11/extensions/sync.h>
#endif

namespace KWin
{

void Extensions::init()
{
    static bool initPerformed = false;
    if (initPerformed) {
        return;
    }
    //  it seems like no events are delivered to XLib when the extension is not queried
    int event_base, error_base;
    XShapeQueryExtension(display(), &event_base, &error_base);
    XRRQueryExtension(display(), &event_base, &error_base);
    XDamageQueryExtension(display(), &event_base, &error_base);
#ifdef HAVE_XSYNC
    XSyncQueryExtension(display(), &event_base, &error_base);
#endif
    initPerformed = true;
}

} // namespace

