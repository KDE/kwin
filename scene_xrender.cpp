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

void SceneXrender::paint( XserverRegion damage, ToplevelList windows )
    {
#if 1
    XRectangle r;
    r.x = 0;
    r.y = 0;
    r.width = displayWidth();
    r.height = displayHeight();
    XFixesSetRegion( display(), damage, &r, 1 );
#endif
    // Use the damage region as the clip region for the root window
    XFixesSetPictureClipRegion( display(), front, 0, 0, damage );
    // Prepare pass for windows
    // Go top to bottom so that clipping is computed properly for phase1
    for( int i = windows.count() - 1;
         i >= 0;
         --i )
        {
        Toplevel* c = windows[ i ];
        resetWindowData( c );
        WindowData& data = window_data[ c ];
        Picture picture = data.picture();
        if( picture == None ) // The render format can be null for GL and/or Xv visuals
            {
            windows.removeAt( i );
            continue;
            }
        effects->transformWindow( c, data.matrix, data.effect ); // TODO remove, instead add initWindow() to effects
        effects->transformWorkspace( data.matrix, data.effect );
        data.saveClipRegion( damage );
        if( data.simpleTransformation() && data.isOpaque())
            { // is opaque, has simple shape, can be clipped, will be painted using simpler faster method
            // Subtract the clients shape from the damage region
            XserverRegion shape = data.shape();
            assert( shape != None );
            XFixesSubtractRegion( display(), damage, damage, shape );
            data.phase = 1;
            }
        else
            data.phase = 2; // will be painted later bottom to top
        }
    // Fill any areas of the root window not covered by windows
    XFixesSetPictureClipRegion( display(), buffer, 0, 0, damage );
    XRenderColor col = { 0xffff, 0xffff, 0xffff, 0xffff };
    XRenderFillRectangle( display(), PictOpSrc, buffer, &col, 0, 0, displayWidth(), displayHeight());
    // Draw each opaque window top to bottom, subtracting the bounding rect of
    // each window from the clip region after it's been drawn.
    for( int i = windows.count() - 1;
         i >= 0;
         --i )
        {
        Toplevel* c = windows[ i ];
        WindowData& data = window_data[ c ];
        if( data.phase != 1 )
            continue;
        XFixesSetPictureClipRegion( display(), buffer, 0, 0, data.savedClipRegion());
        Picture picture = data.picture();
        XRenderComposite( display(), PictOpSrc, picture, None, buffer, 0, 0, 0, 0,
            c->x() + int( data.matrix.xTranslate()), c->y() + int( data.matrix.yTranslate()), c->width(), c->height());
        }
    // Now walk the list bottom to top, drawing translucent and complicated windows.
    // That we draw bottom to top is important now since we're drawing translucent objects
    // and also are clipping only by opaque windows.
    for( int i = 0;
         i < windows.count();
         ++i )
        {
        Toplevel* c = windows[ i ];
        WindowData& data = window_data[ c ];
        if( data.phase != 2 )
            continue;
        XFixesSetPictureClipRegion( display(), buffer, 0, 0, data.savedClipRegion());
        Picture picture = data.picture();
        Picture alpha = data.alphaMask();
        if( data.simpleTransformation())
            {
            XRenderComposite( display(), PictOpOver, picture, alpha, buffer, 0, 0, 0, 0,
                c->x() + int( data.matrix.xTranslate()), c->y() + int( data.matrix.yTranslate()), c->width(), c->height());
            }
        else
            {
            // TODO Okay, I'm at loss here. Whoever wants advanced transformations can implement
            // it themselves. If not, they actually don't want it that badly *shrug*.
//            setPictureMatrix( picture, data.matrix );
            XRenderComposite( display(), PictOpSrc, picture, alpha, buffer, 0, 0, 0, 0,
                c->x(), c->y(), c->width(), c->height());
//            setPictureMatrix( picture, Matrix());
            }
        }
    // cleanup
    for( int i = 0;
         i < windows.count();
         ++i )
        {
        Toplevel* c = windows[ i ];
        WindowData& data = window_data[ c ];
        data.cleanup();
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
    const Matrix& matrix = window_data[ c ].matrix;
    if( matrix.isIdentity())
        return;
    if( matrix.isOnlyTranslate())
        XFixesTranslateRegion( display(), r, int( matrix.xTranslate()), int( matrix.yTranslate()));
    else
        {
        // The region here should be translated using the matrix, but that's not possible
        // (well, maybe fetch the region and transform manually - TODO check). So simply
        // mark whole screen as damaged.
        XRectangle s;
        s.x = s.y = 0;
        s.width = displayWidth();
        s.height = displayHeight();
        XFixesSetRegion( display(), r, &s, 1 );
        }
    }

void SceneXrender::updateTransformation( Toplevel* c )
    {
    // TODO maybe only mark as invalid and update on-demand
    resetWindowData( c );
    WindowData& data = window_data[ c ];
    effects->transformWindow( c, data.matrix, data.effect );
    effects->transformWorkspace( data.matrix, data.effect );
    }

void SceneXrender::resetWindowData( Toplevel* c )
    {
    if( !window_data.contains( c ))
        window_data[ c ] = WindowData( c, XRenderFindVisualFormat( display(), c->visual()));
    WindowData& data = window_data[ c ];
    data.matrix = Matrix();
    data.effect.opacity = c->opacity();
    }

void SceneXrender::windowGeometryShapeChanged( Toplevel* c )
    {
    if( !window_data.contains( c ))
        return;
    window_data[ c ].geometryShapeChanged();
    }
    
void SceneXrender::windowOpacityChanged( Toplevel* c )
    {
    if( !window_data.contains( c ))
        return;
    window_data[ c ].opacityChanged();
    }

void SceneXrender::windowDeleted( Toplevel* c )
    {
    if( !window_data.contains( c ))
        return;
    window_data[ c ].free();
    window_data.remove( c );
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

void SceneXrender::setPictureMatrix( Picture pic, const Matrix& )
    {
    if( pic == None )
        return;
#if 0
    XTransform t;
    // ignore z axis
    t.matrix[ 0 ][ 0 ] = XDoubleToFixed( 1 / m.m[ 0 ][ 0 ] );
    t.matrix[ 0 ][ 1 ] = XDoubleToFixed( m.m[ 0 ][ 1 ] );
    t.matrix[ 0 ][ 2 ] = -XDoubleToFixed( m.m[ 0 ][ 3 ] ); // translation seems to be inverted
    t.matrix[ 1 ][ 0 ] = XDoubleToFixed( m.m[ 1 ][ 0 ] );
    t.matrix[ 1 ][ 1 ] = XDoubleToFixed( 1 / m.m[ 1 ][ 1 ] );
    t.matrix[ 1 ][ 2 ] = -XDoubleToFixed( m.m[ 1 ][ 3 ] );
    t.matrix[ 2 ][ 0 ] = XDoubleToFixed( m.m[ 3 ][ 0 ] );
    t.matrix[ 2 ][ 1 ] = XDoubleToFixed( m.m[ 3 ][ 1 ] );
    t.matrix[ 2 ][ 2 ] = XDoubleToFixed( m.m[ 3 ][ 3 ] );
    // and scaling seems to be wrong too
    // or maybe I just don't get it, but anyway, for now
    if( m.m[ 3 ][ 3 ] != 1 )
        {
        t.matrix[ 0 ][ 0 ] = XDoubleToFixed( 1 / m.m[ 0 ][ 0 ] * m.m[ 3 ][ 3 ] );
        t.matrix[ 1 ][ 1 ] = XDoubleToFixed( 1 / m.m[ 1 ][ 1 ] * m.m[ 3 ][ 3 ] );
        t.matrix[ 2 ][ 2 ] = XDoubleToFixed( 1 );
        }
    XRenderSetPictureTransform( display(), pic, &t );
    if( t.matrix[ 0 ][ 0 ] != XDoubleToFixed( 1 )
     || t.matrix[ 1 ][ 1 ] != XDoubleToFixed( 1 )
     || t.matrix[ 2 ][ 2 ] != XDoubleToFixed( 1 )
     || t.matrix[ 0 ][ 1 ] != XDoubleToFixed( 0 )
     || t.matrix[ 1 ][ 0 ] != XDoubleToFixed( 0 ))
        {
        XRenderSetPictureFilter( display(), pic, const_cast< char* >( FilterGood ), 0, 0 );
        }
    else // fast filter for identity or translation
        {
        XRenderSetPictureFilter( display(), pic, const_cast< char* >( FilterFast ), 0, 0 );
        }
#endif
    }

SceneXrender::WindowData::WindowData( Toplevel* c, XRenderPictFormat* f )
    : window( c )
    , _picture( None )
    , format( f )
    , saved_clip_region( None )
    , alpha( None )
    , _shape( None )
    {
    }

void SceneXrender::WindowData::free()
    {
    if( _picture != None )
        XRenderFreePicture( display(), _picture );
    if( alpha != None )
        XRenderFreePicture( display(), alpha );
    if( _shape != None )
        XRenderFreePicture( display(), _shape );
    }

bool SceneXrender::WindowData::simpleTransformation() const
    {
    return ( matrix.isIdentity() || matrix.isOnlyTranslate());
    }

Picture SceneXrender::WindowData::picture()
    {
    if( _picture == None && format != NULL )
        _picture = XRenderCreatePicture( display(), window->windowPixmap(), format, 0, 0 );
    return _picture;
    }

void SceneXrender::WindowData::saveClipRegion( XserverRegion r )
    {
    saved_clip_region = XFixesCreateRegion( display(), NULL, 0 );
    XFixesCopyRegion( display(), saved_clip_region, r );
    }

XserverRegion SceneXrender::WindowData::savedClipRegion()
    {
    return saved_clip_region;
    }

void SceneXrender::WindowData::cleanup()
    {
    XFixesDestroyRegion( display(), saved_clip_region );
    saved_clip_region = None;
    }

bool SceneXrender::WindowData::isOpaque() const
    {
    if( format->type == PictTypeDirect && format->direct.alphaMask )
        return false;
    if( effect.opacity != 1.0 )
        return false;
    return true;
    }

Picture SceneXrender::WindowData::alphaMask()
    {
    if( isOpaque())
        return None;
    if( alpha != None && alpha_cached_opacity != effect.opacity )
        {
        if( alpha != None )
            XRenderFreePicture( display(), alpha );
        alpha = None;
        }
    if( alpha != None )
        return alpha;
    Pixmap pixmap = XCreatePixmap( display(), rootWindow(), 1, 1, 8 );
    XRenderPictFormat* format = XRenderFindStandardFormat( display(), PictStandardA8 );
    XRenderPictureAttributes pa;
    pa.repeat = True;
    alpha = XRenderCreatePicture( display(), pixmap, format, CPRepeat, &pa );
    XFreePixmap( display(), pixmap );
    XRenderColor col;
    col.alpha = int( effect.opacity * 0xffff );
    alpha_cached_opacity = effect.opacity;
    XRenderFillRectangle( display(), PictOpSrc, alpha, &col, 0, 0, 1, 1 );
    return alpha;
    }


XserverRegion SceneXrender::WindowData::shape()
    {
#if 0 // it probably doesn't make sense to cache this, and perhaps some others - they aren't roundtrips
    if( shape == None )
        {
        shape = XFixesCreateRegionFromWindow( display(), window->handle(), WindowRegionBounding );
        XFixesTranslateRegion( display(), shape, window->x(), window->y());
        }
    return shape;
#else
    if( !simpleTransformation())
        {
        // The region here should be translated using the matrix, but that's not possible
        // (well, maybe fetch the region and transform manually - TODO check).
        return None;
        }
    XserverRegion shape = XFixesCreateRegionFromWindow( display(), window->handle(), WindowRegionBounding );
    XFixesTranslateRegion( display(), shape,
        window->x() + int( matrix.xTranslate()), window->y() + int( matrix.yTranslate()));
    return shape;
#endif
    }

void SceneXrender::WindowData::geometryShapeChanged()
    {
    if( _picture != None )
        XRenderFreePicture( display(), _picture );
    _picture = None;
    if( alpha != None )
        XRenderFreePicture( display(), alpha );
    alpha = None;
    if( _shape != None )
        XFixesDestroyRegion( display(), _shape );
    _shape = None;
    }

void SceneXrender::WindowData::opacityChanged()
    {
    if( alpha != None )
        XRenderFreePicture( display(), alpha );
    alpha = None;
    }

} // namespace
#endif
