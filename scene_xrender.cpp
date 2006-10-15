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

void SceneXrender::paint( QRegion damage, ToplevelList windows )
    {
    if( /*generic case*/ false )
        paintGenericScreen( windows );
    else
        paintSimpleScreen( damage, windows );
    }
    
void SceneXrender::paintGenericScreen( ToplevelList windows )
    {
    paintBackground( infiniteRegion());
    foreach( Toplevel* c, windows ) // bottom to top
        {
        assert( this->windows.contains( c ));
        Window& w = this->windows[ c ];
        if( !w.isVisible())
            continue;
        w.paint( infiniteRegion(), PAINT_OPAQUE | PAINT_TRANSLUCENT );
        }
    // copy composed buffer to the root window
    XRenderComposite( display(), PictOpSrc, buffer, None, front, 0, 0, 0, 0, 0, 0, displayWidth(), displayHeight());
    XFlush( display());
    }

void SceneXrender::paintSimpleScreen( QRegion damage, ToplevelList windows )
    {
    QList< Phase2Data > phase2;
    QRegion region = damage;
    // Draw each opaque window top to bottom, subtracting the bounding rect of
    // each window from the clip region after it's been drawn.
    for( int i = windows.count() - 1;
         i >= 0;
         --i )
        {
        Toplevel* c = windows[ i ];
        assert( this->windows.contains( c ));
        Window& w = this->windows[ c ];
        if( !w.isVisible())
            continue;
        if( !w.isOpaque())
            {
            phase2.prepend( Phase2Data( &w, region ));
            continue;
            }
        w.paint( region, PAINT_OPAQUE );
        // window is opaque, clip windows below
        region -= w.shape().translated( w.x(), w.y());
        }
    // Fill any areas of the root window not covered by windows
    paintBackground( region );
    // Now walk the list bottom to top, drawing translucent windows.
    // That we draw bottom to top is important now since we're drawing translucent objects
    // and also are clipping only by opaque windows.
    foreach( Phase2Data d, phase2 )
        {
        Window& w = *d.window;
        w.paint( d.region, PAINT_TRANSLUCENT );
        }
    // Use the damage region as the clip region for the root window
    XserverRegion front_region = toXserverRegion( damage );
    XFixesSetPictureClipRegion( display(), front, 0, 0, front_region );
    XFixesDestroyRegion( display(), front_region );
    // copy composed buffer to the root window
    XFixesSetPictureClipRegion( display(), buffer, 0, 0, None );
    XRenderComposite( display(), PictOpSrc, buffer, None, front, 0, 0, 0, 0, 0, 0, displayWidth(), displayHeight());
    XFlush( display());
    }

void SceneXrender::paintBackground( QRegion region )
    {
    XserverRegion background_region = toXserverRegion( region );
    XFixesSetPictureClipRegion( display(), buffer, 0, 0, background_region );
    XFixesDestroyRegion( display(), background_region );
    XRenderColor col = { 0xffff, 0xffff, 0xffff, 0xffff };
    XRenderFillRectangle( display(), PictOpSrc, buffer, &col, 0, 0, displayWidth(), displayHeight());
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

Picture SceneXrender::Window::alphaMask()
    {
    if( isOpaque())
        return None;
    if( alpha != None && alpha_cached_opacity != toplevel->opacity())
        {
        if( alpha != None )
            XRenderFreePicture( display(), alpha );
        alpha = None;
        }
    if( alpha != None )
        return alpha;
    if( toplevel->opacity() == 1.0 )
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
    col.alpha = int( toplevel->opacity() * 0xffff );
    alpha_cached_opacity = toplevel->opacity();
    XRenderFillRectangle( display(), PictOpSrc, alpha, &col, 0, 0, 1, 1 );
    return alpha;
    }

void SceneXrender::Window::paint( QRegion region, int mask )
    {
    if( mask & ( PAINT_OPAQUE | PAINT_TRANSLUCENT ))
        {}
    else if( mask & PAINT_OPAQUE )
        {
        if( !isOpaque())
            return;
        }
    else if( mask & PAINT_TRANSLUCENT )
        {
        if( isOpaque())
            return;
        }
    XserverRegion clip_region = toXserverRegion( region );
    XFixesSetPictureClipRegion( display(), buffer, 0, 0, clip_region );
    XFixesDestroyRegion( display(), clip_region );
    Picture pic = picture();
    if( pic == None ) // The render format can be null for GL and/or Xv visuals
        return;
    if( isOpaque())
        {
        XRenderComposite( display(), PictOpSrc, pic, None, buffer, 0, 0, 0, 0,
            toplevel->x(), toplevel->y(), toplevel->width(), toplevel->height());
        }
    else
        {
        Picture alpha = alphaMask();
        XRenderComposite( display(), PictOpOver, pic, alpha, buffer, 0, 0, 0, 0,
            toplevel->x(), toplevel->y(), toplevel->width(), toplevel->height());
        }
    XFixesSetPictureClipRegion( display(), buffer, 0, 0, None );
    }

} // namespace
#endif
