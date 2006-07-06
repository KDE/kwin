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
#include "effects.h"

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
    for( QMap< Toplevel*, WindowData >::Iterator it = window_data.begin();
         it != window_data.end();
         ++it )
        (*it).free();
    }

void SceneXrender::paint( XserverRegion damage )
    {
#if 0
    XRectangle r;
    r.x = 0;
    r.y = 0;
    r.width = displayWidth();
    r.height = displayHeight();
    XFixesSetRegion( display(), damage, &r, 1 );
#endif
    // Use the damage region as the clip region for the root window
    XFixesSetPictureClipRegion( display(), front, 0, 0, damage );
    // Client list for clients that are either translucent or have a shadow
    ToplevelList translucents;
    // Draw each opaque window top to bottom, subtracting the bounding rect of
    // each window from the clip region after it's been drawn.
    for( int i = windows.count() - 1;
         i >= 0;
         --i )
        {
        Toplevel* c = windows[ i ];
        checkWindowData( c );
        effects->paintWindow( c, window_data[ c ].effect );
        if( isOpaque( c ))
            {
            Picture picture = windowPicture( c );
            Picture shape = windowShape( c );
            if( picture != None && shape != None )
                {
                // Set the clip region for the buffer to the damage region, and
                // subtract the clients shape from the damage region
                XFixesSetPictureClipRegion( display(), buffer, 0, 0, damage );
                XFixesSubtractRegion( display(), damage, damage, shape );
                XRenderComposite( display(), PictOpSrc, picture, None, buffer, 0, 0, 0, 0,
                    c->x(), c->y(), c->width(), c->height());
                }
            }
        saveWindowClipRegion( c, damage );
        translucents.prepend( c );
        }
    // Fill any areas of the root window not covered by windows
    XFixesSetPictureClipRegion( display(), buffer, 0, 0, damage );
    XRenderColor col = { 0xffff, 0xffff, 0xffff, 0xffff };
    XRenderFillRectangle( display(), PictOpSrc, buffer, &col, 0, 0, displayWidth(), displayHeight());
    // Now walk the list bottom to top, drawing translucent windows and shadows.
    // That we draw bottom to top is important now since we're drawing translucent objects.
    for( int i = 0;
         i < translucents.count();
         ++i )
        {
        Toplevel* c = translucents[ i ];
        // Restore the previously saved clip region
        XserverRegion r = savedWindowClipRegion( c );
        XFixesSetPictureClipRegion( display(), buffer, 0, 0, r );
        if( !isOpaque( c ))
            {
            Picture picture = windowPicture( c );
            Picture alpha = windowAlphaMask( c );
            if( picture != None )
                {
                // TODO clip also using shape? also above?
                XRenderComposite( display(), PictOpOver, picture, alpha, buffer, 0, 0, 0, 0,
                    c->x(), c->y(), c->width(), c->height());
                }
            }
        XFixesDestroyRegion( display(), r );
        }
    // copy composed buffer to the root window
    XFixesSetPictureClipRegion( display(), buffer, 0, 0, None );
    XRenderComposite( display(), PictOpSrc, buffer, None, front, 0, 0, 0, 0, 0, 0, displayWidth(), displayHeight());
    XFlush( display());
    }

void SceneXrender::checkWindowData( Toplevel* c )
    {
    if( !window_data.contains( c ))
        {
        window_data[ c ] = WindowData();
        window_data[ c ].format = XRenderFindVisualFormat( display(), c->visual());
        }
    WindowData& data = window_data[ c ];
    data.effect.matrix = Matrix();
    data.effect.opacity = c->opacity();
    }

void SceneXrender::windowGeometryShapeChanged( Toplevel* c )
    {
    if( !window_data.contains( c ))
        return;
    WindowData& data = window_data[ c ];
    if( data.picture != None )
        XRenderFreePicture( display(), data.picture );
    data.picture = None;
    if( data.alpha != None )
        XRenderFreePicture( display(), data.alpha );
    data.alpha = None;
    if( data.shape != None )
        XRenderFreePicture( display(), data.shape );
    data.shape = None;
    }

void SceneXrender::windowOpacityChanged( Toplevel* c )
    {
    if( !window_data.contains( c ))
        return;
    WindowData& data = window_data[ c ];
    if( data.alpha != None )
        XRenderFreePicture( display(), data.alpha );
    data.alpha = None;
    }

void SceneXrender::windowDeleted( Toplevel* c )
    {
    if( !window_data.contains( c ))
        return;
    window_data[ c ].free();
    window_data.remove( c );
    }

Picture SceneXrender::windowPicture( Toplevel* c )
    {
    WindowData& data = window_data[ c ];
    if( data.picture == None && data.format != NULL )
        data.picture = XRenderCreatePicture( display(), c->windowPixmap(), data.format, 0, 0 );
    return data.picture;
    }

void SceneXrender::saveWindowClipRegion( Toplevel* c, XserverRegion r )
    {
    WindowData& data = window_data[ c ];
    data.saved_clip_region = XFixesCreateRegion( display(), NULL, 0 );
    XFixesCopyRegion( display(), data.saved_clip_region, r );
    }

XserverRegion SceneXrender::savedWindowClipRegion( Toplevel* c )
    { // always called after saveWindowClipRegion(), also resets
    WindowData& data = window_data[ c ];
    XserverRegion r = data.saved_clip_region;
    data.saved_clip_region = None;
    return r;
    }

bool SceneXrender::isOpaque( Toplevel* c ) const
    {
    const WindowData& data = window_data[ c ];
    if( data.format->type == PictTypeDirect && data.format->direct.alphaMask )
        return false;
    if( data.effect.opacity != 1.0 )
        return false;
    return true;
    }

Picture SceneXrender::windowAlphaMask( Toplevel* c )
    {
    if( isOpaque( c ))
        return None;
    WindowData& data = window_data[ c ];
    if( data.alpha != None && data.alpha_cached_opacity != data.effect.opacity )
        {
        if( data.alpha != None )
            XRenderFreePicture( display(), data.alpha );
        data.alpha = None;
        }
    if( data.alpha != None )
        return data.alpha;
    Pixmap pixmap = XCreatePixmap( display(), rootWindow(), 1, 1, 8 );
    XRenderPictFormat* format = XRenderFindStandardFormat( display(), PictStandardA8 );
    XRenderPictureAttributes pa;
    pa.repeat = True;
    data.alpha = XRenderCreatePicture( display(), pixmap, format, CPRepeat, &pa );
    XFreePixmap( display(), pixmap );
    XRenderColor col;
    col.alpha = int( data.effect.opacity * 0xffff );
    data.alpha_cached_opacity = data.effect.opacity;
    XRenderFillRectangle( display(), PictOpSrc, data.alpha, &col, 0, 0, 1, 1 );
    return data.alpha;
    }

Picture SceneXrender::windowShape( Toplevel* c )
    {
    WindowData& data = window_data[ c ];
    if( data.shape == None )
        {
        data.shape = XFixesCreateRegionFromWindow( display(), c->handle(), WindowRegionBounding );
        XFixesTranslateRegion( display(), data.shape, c->x(), c->y());
        }
    return data.shape;
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

SceneXrender::WindowData::WindowData()
    : picture( None )
    , format( NULL )
    , saved_clip_region( None )
    , alpha( None )
    , shape( None )
    {
    }

void SceneXrender::WindowData::free()
    {
    if( picture != None )
        XRenderFreePicture( display(), picture );
    if( alpha != None )
        XRenderFreePicture( display(), alpha );
    if( shape != None )
        XRenderFreePicture( display(), shape );
    }

} // namespace
#endif
