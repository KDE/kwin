/*****************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2006 Lubos Lunak <l.lunak@kde.org>

You can Freely distribute this program under the GNU General Public
License. See the file "COPYING" for the exact licensing terms.
******************************************************************/

#include "scene_xrender.h"

#ifdef HAVE_XRENDER

#include "toplevel.h"

namespace KWinInternal
{

//****************************************
// SceneXrender
//****************************************

SceneXrender::SceneXrender( Workspace* ws )
    : Scene( ws )
    {
    format = XRenderFindVisualFormat( display(), DefaultVisual( display(), DefaultScreen( display())));
    XRenderPictureAttributes pa;
    pa.subwindow_mode = IncludeInferiors;
    front = XRenderCreatePicture( display(), rootWindow(), format, CPSubwindowMode, &pa );
    createBuffer();
    }

SceneXrender::~SceneXrender()
    {
    XRenderFreePicture( display(), front );
    XRenderFreePicture( display(), buffer );
    }

// TODO handle xrandr changes

void SceneXrender::createBuffer()
    {
    if( buffer != None )
        XRenderFreePicture( display(), buffer );
    Pixmap pixmap = XCreatePixmap( display(), rootWindow(), displayWidth(), displayHeight(), QX11Info::appDepth());
    buffer = XRenderCreatePicture( display(), pixmap, format, 0, 0 );
    XFreePixmap( display(), pixmap ); // The picture owns the pixmap now
    }

void SceneXrender::paint( XserverRegion damage )
    {
    // Use the damage region as the clip region for the root window
    XFixesSetPictureClipRegion( display(), front, 0, 0, damage );
    // Draw each opaque window top to bottom, subtracting the bounding rect of
    // each window from the clip region after it's been drawn.
    for( int i = windows.count() - 1;
         i >= 0;
         --i )
        {
        Toplevel* c = windows[ i ];
        XWindowAttributes attrs;
        if( !XGetWindowAttributes( display(), c->handle(), &attrs ))
            continue;
        if( XRenderPictFormat* clientFormat = XRenderFindVisualFormat( display(), attrs.visual ))
            {
            Picture picture = XRenderCreatePicture( display(), c->windowPixmap(), clientFormat, 0, 0 );
            // Set the clip region for the buffer to the damage region, and
            // subtract the clients shape from the damage region
            XFixesSetPictureClipRegion( display(), buffer, 0, 0, damage );
            XserverRegion cr = XFixesCreateRegionFromWindow( display(), c->handle(), WindowRegionBounding );
            XFixesTranslateRegion( display(), cr, c->x(), c->y());
            XFixesSubtractRegion( display(), damage, damage, cr );
            XFixesDestroyRegion( display(), cr );
            XRenderComposite( display(), PictOpSrc, picture, None, buffer, 0, 0, 0, 0,
                c->x(), c->y(), c->width(), c->height());
            XRenderFreePicture( display(), picture );
            }
        }
    // fill background
    XFixesSetPictureClipRegion( display(), buffer, 0, 0, damage );
    XRenderColor col = { 0xffff, 0xffff, 0xffff, 0xffff };
    XRenderFillRectangle( display(), PictOpSrc, buffer, &col, 0, 0, displayWidth(), displayHeight());
    // copy composed buffer to the root window
    XFixesSetPictureClipRegion( display(), buffer, 0, 0, None );
    XRenderComposite( display(), PictOpSrc, buffer, None, front, 0, 0, 0, 0, 0, 0, displayWidth(), displayHeight());
    XFlush( display());
    }

} // namespace
#endif
