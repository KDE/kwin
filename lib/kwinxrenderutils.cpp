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
#include <qpixmap.h>
#include <kdebug.h>

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

// XRenderFind(Standard)Format() is a roundtrip, so cache the results
static XRenderPictFormat* renderformats[ 33 ];

static Picture createPicture( Pixmap pix, int depth )
    {
    if( pix == None )
        return None;
    if( renderformats[ depth ] == NULL )
        {
        switch( depth)
            {
            case 1:
                renderformats[ 1 ] = XRenderFindStandardFormat( display(), PictStandardA1 );
              break;
            case 8:
                renderformats[ 8 ] = XRenderFindStandardFormat( display(), PictStandardA8 );
              break;
            case 24:
                renderformats[ 24 ] = XRenderFindStandardFormat( display(), PictStandardRGB24 );
              break;
            case 32:
                renderformats[ 32 ] = XRenderFindStandardFormat( display(), PictStandardARGB32 );
              break;
            default:
                {
                XRenderPictFormat req;
                long mask = PictFormatType | PictFormatDepth;
                req.type = PictTypeDirect;
                req.depth = depth;
                renderformats[ depth ] = XRenderFindFormat( display(), mask, &req, 0 );
              break;
                }
            }
        if( renderformats[ depth ] == NULL )
            {
            kWarning( 1212 ) << "Could not find XRender format for depth" << depth;
            return None;
            }
        }
    return XRenderCreatePicture( display(), pix, renderformats[ depth ], 0, NULL );
    }

XRenderPicture::XRenderPicture( QPixmap pix )
    : d( new XRenderPictureData( createPicture( pix.handle(), pix.depth())))
    {
    }

XRenderPicture::XRenderPicture( Pixmap pix, int depth )
    : d( new XRenderPictureData( createPicture( pix, depth )))
    {
    }

} // namespace

#endif
