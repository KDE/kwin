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
#include "client.h"
#include "effects.h"

namespace KWinInternal
{

//****************************************
// SceneXrender
//****************************************

struct RegionDebug
   {   
   RegionDebug( XserverRegion r ) : rr( r ) {}   
   XserverRegion rr;   
   };   
      
#ifdef NDEBUG
inline
kndbgstream& operator<<( kndbgstream& stream, RegionDebug ) { return stream; }
#else
kdbgstream& operator<<( kdbgstream& stream, RegionDebug r )
    {       
    if( r.rr == None )
        return stream << "EMPTY";
    int num;
    XRectangle* rects = XFixesFetchRegion( display(), r.rr, &num );
    if( rects == NULL || num == 0 )
        return stream << "EMPTY";
    for( int i = 0;
         i < num;
         ++i )
       stream << "[" << rects[ i ].x << "+" << rects[ i ].y << " " << rects[ i ].width << "x" << rects[ i ].height << "]";
    return stream;
    }
#endif

Picture SceneXrender::buffer;
ScreenPaintData SceneXrender::screen_paint;

SceneXrender::SceneXrender( Workspace* ws )
    : Scene( ws )
    {
    format = XRenderFindVisualFormat( display(), DefaultVisual( display(), DefaultScreen( display())));
    XRenderPictureAttributes pa;
    pa.subwindow_mode = IncludeInferiors;
    front = XRenderCreatePicture( display(), rootWindow(), format, CPSubwindowMode, &pa );
    XRectangle xr;
    xr.x = SHRT_MIN / 2;
    xr.y = SHRT_MIN / 2;
    xr.width = SHRT_MAX;
    xr.height = SHRT_MAX;
    createBuffer();
    }

SceneXrender::~SceneXrender()
    {
    XRenderFreePicture( display(), front );
    XRenderFreePicture( display(), buffer );
    for( QMap< Toplevel*, Window >::Iterator it = windows.begin();
         it != windows.end();
         ++it )
        (*it).free();
    }

void SceneXrender::paint( QRegion damage, ToplevelList toplevels )
    {
    foreach( Toplevel* c, toplevels )
        {
        assert( windows.contains( c ));
        stacking_order.append( &windows[ c ] );
        }
    int mask = 0;
    paintScreen( &mask, &damage );
    stacking_order.clear();
    if( mask & PAINT_SCREEN_REGION )
        {
        // Use the damage region as the clip region for the root window
        XserverRegion front_region = toXserverRegion( damage );
        XFixesSetPictureClipRegion( display(), front, 0, 0, front_region );
        XFixesDestroyRegion( display(), front_region );
        // copy composed buffer to the root window
        XFixesSetPictureClipRegion( display(), buffer, 0, 0, None );
        XRenderComposite( display(), PictOpSrc, buffer, None, front, 0, 0, 0, 0, 0, 0, displayWidth(), displayHeight());
        XFixesSetPictureClipRegion( display(), front, 0, 0, None );
        XFlush( display());
        }
    else
        {
        // copy composed buffer to the root window
        XRenderComposite( display(), PictOpSrc, buffer, None, front, 0, 0, 0, 0, 0, 0, displayWidth(), displayHeight());
        XFlush( display());
        }
    }

void SceneXrender::paintGenericScreen( int mask, ScreenPaintData data )
    {
    screen_paint = data; // save, transformations will be done when painting windows
    Scene::paintGenericScreen( mask, data );
    }

void SceneXrender::paintBackground( QRegion region )
    {
    if( region != infiniteRegion())
        {
        XserverRegion background_region = toXserverRegion( region );
        XFixesSetPictureClipRegion( display(), buffer, 0, 0, background_region );
        XFixesDestroyRegion( display(), background_region );
        }
    XRenderColor col = { 0xffff, 0xffff, 0xffff, 0xffff };
    XRenderFillRectangle( display(), PictOpSrc, buffer, &col, 0, 0, displayWidth(), displayHeight());
    XFixesSetPictureClipRegion( display(), buffer, 0, 0, None );
    }

void SceneXrender::windowGeometryShapeChanged( Toplevel* c )
    {
    if( !windows.contains( c )) // this is ok, shape is not valid by default
        return;
    Window& w = windows[ c ];
    w.discardPicture();
    w.discardShape();
    w.discardAlpha();
    }
    
void SceneXrender::windowOpacityChanged( Toplevel* c )
    {
    if( !windows.contains( c )) // this is ok, alpha is created on demand
        return;
    Window& w = windows[ c ];
    w.discardAlpha();
    }

void SceneXrender::windowDeleted( Toplevel* c )
    {
    assert( windows.contains( c ));
    windows[ c ].free();
    windows.remove( c );
    }

void SceneXrender::windowAdded( Toplevel* c )
    {
    assert( !windows.contains( c ));
    windows[ c ] = Window( c );
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

XserverRegion SceneXrender::toXserverRegion( QRegion region )
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

//****************************************
// SceneXrender::Window
//****************************************

SceneXrender::Window::Window( Toplevel* c )
    : Scene::Window( c )
    , _picture( None )
    , format( XRenderFindVisualFormat( display(), c->visual()))
    , alpha( None )
    {
    }

void SceneXrender::Window::free()
    {
    discardPicture();
    discardAlpha();
    discardShape();
    }

Picture SceneXrender::Window::picture()
    {
    if( !toplevel->damage().isEmpty() && _picture != None )
        {
        XRenderFreePicture( display(), _picture );
        _picture = None;
        }
    if( _picture == None && format != NULL )
        {
        Pixmap window_pix = toplevel->createWindowPixmap();
        Pixmap pix = window_pix;
        // HACK the same like with opengl
        Client* c = dynamic_cast< Client* >( toplevel );
        bool alpha_clear = c != NULL && c->hasAlpha() && !c->noBorder();
#define ALPHA_CLEAR_COPY
#ifdef ALPHA_CLEAR_COPY
        if( alpha_clear )
            {
            Pixmap p2 = XCreatePixmap( display(), pix, c->width(), c->height(), 32 );
            GC gc = XCreateGC( display(), pix, 0, NULL );
            XCopyArea( display(), pix, p2, gc, 0, 0, c->width(), c->height(), 0, 0 );
            pix = p2;
            XFreeGC( display(), gc );
            }
#endif
        if( alpha_clear )
            {
            XGCValues gcv;
            gcv.foreground = 0xff000000;
            gcv.plane_mask = 0xff000000;
            GC gc = XCreateGC( display(), pix, GCPlaneMask | GCForeground, &gcv );
            XFillRectangle( display(), pix, gc, 0, 0, c->width(), c->clientPos().y());
            XFillRectangle( display(), pix, gc, 0, 0, c->clientPos().x(), c->height());
            int tw = c->clientPos().x() + c->clientSize().width();
            int th = c->clientPos().y() + c->clientSize().height();
            XFillRectangle( display(), pix, gc, 0, th, c->width(), c->height() - th );
            XFillRectangle( display(), pix, gc, tw, 0, c->width() - tw, c->height());
            XFreeGC( display(), gc );
            }
        _picture = XRenderCreatePicture( display(), pix, format, 0, 0 );
        XFreePixmap( display(), pix ); // the picture owns the pixmap
#ifdef ALPHA_CLEAR_COPY
        if( alpha_clear )
            XFreePixmap( display(), window_pix );
#endif
        }
    return _picture;
    }


void SceneXrender::Window::discardPicture()
    {
    if( _picture != None )
        XRenderFreePicture( display(), _picture );
    _picture = None;
    }

void SceneXrender::Window::discardAlpha()
    {
    if( alpha != None )
        XRenderFreePicture( display(), alpha );
    alpha = None;
    }

Picture SceneXrender::Window::alphaMask( double opacity )
    {
    if( isOpaque() && opacity == 1.0 )
        return None;
    if( alpha != None && alpha_cached_opacity != opacity )
        {
        if( alpha != None )
            XRenderFreePicture( display(), alpha );
        alpha = None;
        }
    if( alpha != None )
        return alpha;
    if( opacity == 1.0 )
        { // no need to create alpha mask
        alpha_cached_opacity = 1.0;
        return None;
        }
    Pixmap pixmap = XCreatePixmap( display(), rootWindow(), 1, 1, 8 );
    XRenderPictFormat* format = XRenderFindStandardFormat( display(), PictStandardA8 );
    XRenderPictureAttributes pa;
    pa.repeat = True;
    alpha = XRenderCreatePicture( display(), pixmap, format, CPRepeat, &pa );
    XFreePixmap( display(), pixmap );
    XRenderColor col;
    col.alpha = int( opacity * 0xffff );
    alpha_cached_opacity = opacity;
    XRenderFillRectangle( display(), PictOpSrc, alpha, &col, 0, 0, 1, 1 );
    return alpha;
    }

void SceneXrender::Window::performPaint( int mask, QRegion region, WindowPaintData data )
    {
    bool opaque = isOpaque() && data.opacity == 1.0;
    if( mask & ( PAINT_WINDOW_OPAQUE | PAINT_WINDOW_TRANSLUCENT ))
        {}
    else if( mask & PAINT_WINDOW_OPAQUE )
        {
        if( !opaque )
            return;
        }
    else if( mask & PAINT_WINDOW_TRANSLUCENT )
        {
        if( opaque )
            return;
        }
    if( region != infiniteRegion())
        {
        XserverRegion clip_region = toXserverRegion( region );
        XFixesSetPictureClipRegion( display(), buffer, 0, 0, clip_region );
        XFixesDestroyRegion( display(), clip_region );
        }
    Picture pic = picture();
    if( pic == None ) // The render format can be null for GL and/or Xv visuals
        return;
    int x = toplevel->x();
    int y = toplevel->y();
    if( mask & PAINT_SCREEN_TRANSFORMED )
        {
        x += screen_paint.xTranslate;
        y += screen_paint.yTranslate;
        }
    if( mask & PAINT_WINDOW_TRANSFORMED )
        {
        x += data.xTranslate;
        y += data.yTranslate;
        }
    // TODO xScale,yScale
    if( opaque )
        {
        XRenderComposite( display(), PictOpSrc, pic, None, buffer, 0, 0, 0, 0,
            x, y, toplevel->width(), toplevel->height());
        }
    else
        {
        Picture alpha = alphaMask( data.opacity );
        XRenderComposite( display(), PictOpOver, pic, alpha, buffer, 0, 0, 0, 0,
            x, y, toplevel->width(), toplevel->height());
        }
    XFixesSetPictureClipRegion( display(), buffer, 0, 0, None );
    }

} // namespace
#endif
