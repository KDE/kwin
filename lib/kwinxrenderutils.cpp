/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2008 Lubos Lunak <l.lunak@kde.org>

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

#include "kwinxrenderutils.h"

#ifdef KWIN_HAVE_XRENDER_COMPOSITING

#include <qvector.h>

namespace KWin
{

// Convert QRegion to XserverRegion. All code uses XserverRegion
// only when really necessary as the shared implementation uses
// QRegion.
XserverRegion toXserverRegion( QRegion region )
    {
    QVector< QRect > rects = region.rects();
    XRectangle* xr = new XRectangle[ rects.count() ];
    for( int i = 0;
         i < rects.count();
         ++i )
        {
        xr[ i ].x = rects[ i ].x();
        xr[ i ].y = rects[ i ].y();
        xr[ i ].width = rects[ i ].width();
        xr[ i ].height = rects[ i ].height();
        }
    XserverRegion ret = XFixesCreateRegion( display(), xr, rects.count());
    delete[] xr;
    return ret;
    }

} // namespace

#endif
