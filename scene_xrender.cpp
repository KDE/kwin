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
        WindowData& data = window_data[ c ];
        effects->transformWindow( c, data.effect ); // TODO remove, instead add initWindow() to effects
        if( isOpaque( c ))
            {
            Picture picture = windowPicture( c );
            XserverRegion shape = windowShape( c );
            if( picture != None && shape != None )
                {
                // Set the clip region for the buffer to the damage region, and
                // subtract the clients shape from the damage region
                XFixesSetPictureClipRegion( display(), buffer, 0, 0, damage );
                const Matrix& matrix = data.effect.matrix;
                if( !matrix.isIdentity())
                    {
                    assert( matrix.isOnlyTranslate());
                    XFixesTranslateRegion( display(), shape, int( matrix.xTranslate()), int( matrix.yTranslate()));
                    }
                XFixesSubtractRegion( display(), damage, damage, shape );
                XRenderComposite( display(), PictOpSrc, picture, None, buffer, 0, 0, 0, 0,
                    c->x() + int( matrix.xTranslate()), c->y() + int( matrix.yTranslate()), c->width(), c->height());
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
                const Matrix& matrix = window_data[ c ].effect.matrix;
                // TODO clip also using shape? also above?
                XRenderComposite( display(), PictOpOver, picture, alpha, buffer, 0, 0, 0, 0,
                    c->x() + int( matrix.xTranslate()), c->y() + int( matrix.yTranslate()), c->width(), c->height());
                }
            }
        XFixesDestroyRegion( display(), r );
        }
    // copy composed buffer to the root window
    XFixesSetPictureClipRegion( display(), buffer, 0, 0, None );
    XRenderComposite( display(), PictOpSrc, buffer, None, front, 0, 0, 0, 0, 0, 0, displayWidth(), displayHeight());
    XFlush( display());
    }

void SceneXrender::transformWindowDamage( Toplevel* c, XserverRegion r ) const
    {
    if( !window_data.contains( c ))
        return;
    const Matrix& matrix = window_data[ c ].effect.matrix;
    if( matrix.isIdentity())
        return;
    assert( matrix.isOnlyTranslate());
    // TODO the matrix here is not valid after it changes but before it's first painted
    // (i.e. a changes to state where it should be translated but the matrix is not yet updated)
    XFixesTranslateRegion( display(), r, int( matrix.xTranslate()), int( matrix.yTranslate()));
    }

void SceneXrender::updateTransformation( Toplevel* c )
    {
    // TODO maybe only mark as invalid and update on-demand
    checkWindowData( c );
    WindowData& data = window_data[ c ];
    effects->transformWindow( c, data.effect );
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
        XFixesDestroyRegion( display(), data.shape );
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

XserverRegion SceneXrender::windowShape( Toplevel* c )
    {
#if 0 // it probably doesn't make sense to cache this, and perhaps some others - they aren't roundtrips
    WindowData& data = window_data[ c ];
    if( data.shape == None )
        {
        data.shape = XFixesCreateRegionFromWindow( display(), c->handle(), WindowRegionBounding );
        XFixesTranslateRegion( display(), data.shape, c->x(), c->y());
        }
    return data.shape;
#else
    XserverRegion shape = XFixesCreateRegionFromWindow( display(), c->handle(), WindowRegionBounding );
    XFixesTranslateRegion( display(), shape, c->x(), c->y());
    return shape;
#endif
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

void SceneXrender::setPictureMatrix( Picture pic, const Matrix& m )
    {
    if( pic == None )
        return;
    XTransform t;
    // ignore z axis
    t.matrix[ 0 ][ 0 ] = XDoubleToFixed( m.m[ 0 ][ 0 ] );
    t.matrix[ 0 ][ 1 ] = XDoubleToFixed( m.m[ 0 ][ 1 ] );
    t.matrix[ 0 ][ 2 ] = -XDoubleToFixed( m.m[ 0 ][ 3 ] ); // translation seems to be inverted
    t.matrix[ 1 ][ 0 ] = XDoubleToFixed( m.m[ 1 ][ 0 ] );
    t.matrix[ 1 ][ 1 ] = XDoubleToFixed( m.m[ 1 ][ 1 ] );
    t.matrix[ 1 ][ 2 ] = -XDoubleToFixed( m.m[ 1 ][ 3 ] );
    t.matrix[ 2 ][ 0 ] = XDoubleToFixed( m.m[ 3 ][ 0 ] );
    t.matrix[ 2 ][ 1 ] = XDoubleToFixed( m.m[ 3 ][ 1 ] );
    t.matrix[ 2 ][ 2 ] = XDoubleToFixed( m.m[ 3 ][ 3 ] );
    XRenderSetPictureTransform( display(), pic, &t );
    if( t.matrix[ 0 ][ 0 ] != XDoubleToFixed( 1 ) // fast filter for identity or translation
     || t.matrix[ 1 ][ 1 ] != XDoubleToFixed( 1 )
     || t.matrix[ 2 ][ 2 ] != XDoubleToFixed( 1 )
     || t.matrix[ 0 ][ 1 ] != XDoubleToFixed( 0 )
     || t.matrix[ 1 ][ 0 ] != XDoubleToFixed( 0 ))
        {
        XRenderSetPictureFilter( display(), pic, const_cast< char* >( FilterGood ), 0, 0 );
        }
    else
        {
        XRenderSetPictureFilter( display(), pic, const_cast< char* >( FilterFast ), 0, 0 );
        }
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
