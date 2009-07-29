/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2006 Lubos Lunak <l.lunak@kde.org>
Copyright (C) 2009 Fredrik Höglund <fredrik@kde.org>

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

/*
 This is the XRender-based compositing code. The primary compositing
 backend is the OpenGL-based one, which should be more powerful
 and also possibly better documented. This backend is mostly for cases
 when the OpenGL backend cannot be used for some reason (insufficient
 performance, no usable OpenGL support at all, etc.)
 The plan is to keep it around as long as needed/possible, but if it
 proves to be too much hassle it will be dropped in the future.
 
 Docs:
 
 XRender (the protocol, but the function calls map to it):
 http://gitweb.freedesktop.org/?p=xorg/proto/renderproto.git;a=blob_plain;hb=HEAD;f=renderproto.txt
 
 XFixes (again, the protocol):
 http://gitweb.freedesktop.org/?p=xorg/proto/fixesproto.git;a=blob_plain;hb=HEAD;f=fixesproto.txt

*/

#include "scene_xrender.h"

#ifdef KWIN_HAVE_XRENDER_COMPOSITING

#include "toplevel.h"
#include "client.h"
#include "deleted.h"
#include "effects.h"
#include "kwinxrenderutils.h"

#include <X11/extensions/Xcomposite.h>

#include <kxerrorhandler.h>

namespace KWin
{

//****************************************
// SceneXrender
//****************************************

// kDebug() support for the XserverRegion type
struct RegionDebug
   {   
   RegionDebug( XserverRegion r ) : rr( r ) {}   
   XserverRegion rr;   
   };   

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

Picture SceneXrender::buffer = None;
ScreenPaintData SceneXrender::screen_paint;

SceneXrender::SceneXrender( Workspace* ws )
    : Scene( ws )
    , front( None )
    , init_ok( false )
    {
    if( !Extensions::renderAvailable())
        {
        kError( 1212 ) << "No XRender extension available";
        return;
        }
    if( !Extensions::fixesRegionAvailable())
        {
        kError( 1212 ) << "No XFixes v3+ extension available";
        return;
        }
    KXErrorHandler xerr;
    if( wspace->createOverlay())
        {
        wspace->setupOverlay( None );
        XWindowAttributes attrs;
        XGetWindowAttributes( display(), wspace->overlayWindow(), &attrs );
        format = XRenderFindVisualFormat( display(), attrs.visual );
        if( format == NULL )
            {
            kError( 1212 ) << "Failed to find XRender format for overlay window";
            return;
            }
        front = XRenderCreatePicture( display(), wspace->overlayWindow(), format, 0, NULL );
        }
    else
        {
        // create XRender picture for the root window
        format = XRenderFindVisualFormat( display(), DefaultVisual( display(), DefaultScreen( display())));
        if( format == NULL )
            {
            kError( 1212 ) << "Failed to find XRender format for root window";
            return; // error
            }
        XRenderPictureAttributes pa;
        pa.subwindow_mode = IncludeInferiors;
        front = XRenderCreatePicture( display(), rootWindow(), format, CPSubwindowMode, &pa );
        }
    createBuffer();
    if( xerr.error( true ))
        {
        kError( 1212 ) << "XRender compositing setup failed";
        return;
        }
    if( !initting ) // see comment for opengl version
        {
        if( !selfCheck())
            return;
        selfCheckDone = true;
        }
    init_ok = true;
    }

SceneXrender::~SceneXrender()
    {
    if( !init_ok )
        {
        // TODO this probably needs to clean up whatever has been created until the failure
        wspace->destroyOverlay();
        return;
        }
    XRenderFreePicture( display(), front );
    XRenderFreePicture( display(), buffer );
    buffer = None;
    wspace->destroyOverlay();
    foreach( Window* w, windows )
        delete w;
    }

bool SceneXrender::initFailed() const
    {
    return !init_ok;
    }

// Create the compositing buffer. The root window is not double-buffered,
// so it is done manually using this buffer,
void SceneXrender::createBuffer()
    {
    Pixmap pixmap = XCreatePixmap( display(), rootWindow(), displayWidth(), displayHeight(), DefaultDepth( display(), DefaultScreen( display())));
    buffer = XRenderCreatePicture( display(), pixmap, format, 0, 0 );
    XFreePixmap( display(), pixmap ); // The picture owns the pixmap now
    }

// Just like SceneOpenGL::selfCheck()
bool SceneXrender::selfCheck()
    {
    QRegion reg = selfCheckRegion();
    if( wspace->overlayWindow())
        { // avoid covering the whole screen too soon
        wspace->setOverlayShape( reg );
        wspace->showOverlay();
        }
    selfCheckSetup();
    flushBuffer( PAINT_SCREEN_REGION, reg );
    bool ok = selfCheckFinish();
    if( wspace->overlayWindow())
        wspace->hideOverlay();
    return ok;
    }

void SceneXrender::selfCheckSetup()
    {
    KXErrorHandler err;
    QImage img( selfCheckWidth(), selfCheckHeight(), QImage::Format_RGB32 );
    img.setPixel( 0, 0, QColor( Qt::red ).rgb());
    img.setPixel( 1, 0, QColor( Qt::green ).rgb());
    img.setPixel( 2, 0, QColor( Qt::blue ).rgb());
    img.setPixel( 0, 1, QColor( Qt::white ).rgb());
    img.setPixel( 1, 1, QColor( Qt::black ).rgb());
    img.setPixel( 2, 1, QColor( Qt::white ).rgb());
    QPixmap pix = QPixmap::fromImage( img );
    foreach( const QPoint& p, selfCheckPoints())
        {
        XSetWindowAttributes wa;
        wa.override_redirect = True;
        ::Window window = XCreateWindow( display(), rootWindow(), 0, 0, selfCheckWidth(), selfCheckHeight(),
            0, QX11Info::appDepth(), CopyFromParent, CopyFromParent, CWOverrideRedirect, &wa );
        XSetWindowBackgroundPixmap( display(), window, pix.handle());
        XClearWindow( display(), window );
        XMapWindow( display(), window );
        // move the window one down to where the result will be rendered too, just in case
        // the render would fail completely and eventual check would try to read this window's contents
        XMoveWindow( display(), window, p.x() + 1, p.y());
        XCompositeRedirectWindow( display(), window, CompositeRedirectAutomatic );
        Pixmap wpix = XCompositeNameWindowPixmap( display(), window );
        XWindowAttributes attrs;
        XGetWindowAttributes( display(), window, &attrs );
        XRenderPictFormat* format = XRenderFindVisualFormat( display(), attrs.visual );
        Picture pic = XRenderCreatePicture( display(), wpix, format, 0, 0 );
        QRect rect( p.x(), p.y(), selfCheckWidth(), selfCheckHeight());
        XRenderComposite( display(), PictOpSrc, pic, None, buffer, 0, 0, 0, 0,
            rect.x(), rect.y(), rect.width(), rect.height());
        XRenderFreePicture( display(), pic );
        XFreePixmap( display(), wpix );
        XDestroyWindow( display(), window );
        }
    err.error( true ); // just sync and discard
    }

bool SceneXrender::selfCheckFinish()
    {
    KXErrorHandler err;
    bool ok = true;
    foreach( const QPoint& p, selfCheckPoints())
        {
        QPixmap pix = QPixmap::grabWindow( rootWindow(), p.x(), p.y(), selfCheckWidth(), selfCheckHeight());
        QImage img = pix.toImage();
//        kDebug(1212) << "P:" << QColor( img.pixel( 0, 0 )).name();
//        kDebug(1212) << "P:" << QColor( img.pixel( 1, 0 )).name();
//        kDebug(1212) << "P:" << QColor( img.pixel( 2, 0 )).name();
//        kDebug(1212) << "P:" << QColor( img.pixel( 0, 1 )).name();
//        kDebug(1212) << "P:" << QColor( img.pixel( 1, 1 )).name();
//        kDebug(1212) << "P:" << QColor( img.pixel( 2, 1 )).name();
        if( img.pixel( 0, 0 ) != QColor( Qt::red ).rgb()
            || img.pixel( 1, 0 ) != QColor( Qt::green ).rgb()
            || img.pixel( 2, 0 ) != QColor( Qt::blue ).rgb()
            || img.pixel( 0, 1 ) != QColor( Qt::white ).rgb()
            || img.pixel( 1, 1 ) != QColor( Qt::black ).rgb()
            || img.pixel( 2, 1 ) != QColor( Qt::white ).rgb())
            {
            kError( 1212 ) << "Compositing self-check failed, disabling compositing.";
            ok = false;
            break;
            }
        }
    if( err.error( true ))
        ok = false;
    if( ok )
        kDebug( 1212 ) << "Compositing self-check passed.";
    if( !ok && options->disableCompositingChecks )
        {
        kWarning( 1212 ) << "Compositing checks disabled, proceeding regardless of self-check failure.";
        return true;
        }
    return ok;
    }


// the entry point for painting
void SceneXrender::paint( QRegion damage, ToplevelList toplevels )
    {
    foreach( Toplevel* c, toplevels )
        {
        assert( windows.contains( c ));
        stacking_order.append( windows[ c ] );
        }
    int mask = 0;
    paintScreen( &mask, &damage );
    if( wspace->overlayWindow()) // show the window only after the first pass, since
        wspace->showOverlay();   // that pass may take long
    if( !selfCheckDone )
        {
        selfCheckSetup();
        damage |= selfCheckRegion();
        }
    flushBuffer( mask, damage );
    if( !selfCheckDone )
        {
        if( !selfCheckFinish())
            QTimer::singleShot( 0, Workspace::self(), SLOT( finishCompositing()));
        selfCheckDone = true;
        }
    // do cleanup
    stacking_order.clear();
    }

void SceneXrender::flushBuffer( int mask, QRegion damage )
    {
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
    if( true ) // as long as paintTransformedScreen() doesn't work properly
        Scene::paintGenericScreen( mask, data );
    else
        paintTransformedScreen( mask );
    }

/*
 TODO currently broken
 Try to do optimized painting even with transformations. Since only scaling
 and translation are supported by the painting code, clipping can be done
 manually to avoid having to paint everything in every pass. Whole screen
 still need to be painted but e.g. obscured windows don't. So this below
 is basically paintSimpleScreen() with extra code to compute clipping correctly.
 
 This code assumes that the only transformations possible with XRender are those
 provided by Window/ScreenPaintData, In the (very unlikely?) case more is needed
 then paintGenericScreen() needs to be used.
*/
void SceneXrender::paintTransformedScreen( int orig_mask )
    {
    QRegion region( 0, 0, displayWidth(), displayHeight());
    QList< Phase2Data > phase2;
    QRegion allclips;
    // Draw each opaque window top to bottom, subtracting the bounding rect of
    // each window from the clip region after it's been drawn.
    for( int i = stacking_order.count() - 1; // top to bottom
         i >= 0;
         --i )
        {
        Window* w = static_cast< Window* >( stacking_order[ i ] );
        WindowPrePaintData data;
        data.mask = orig_mask | ( w->isOpaque() ? PAINT_WINDOW_OPAQUE : PAINT_WINDOW_TRANSLUCENT );
        w->resetPaintingEnabled();
        data.paint = region;
        // TODO this is wrong, transformedShape() should be used here, but is not known yet
        data.clip = w->isOpaque() ? region : QRegion();
        data.quads = w->buildQuads();
        // preparation step
        effects->prePaintWindow( effectWindow( w ), data, time_diff );
#ifndef NDEBUG
        foreach( const WindowQuad &q, data.quads )
            if( q.isTransformed())
                kFatal( 1212 ) << "Pre-paint calls are not allowed to transform quads!" ;
#endif
        if( !w->isPaintingEnabled())
            continue;
        data.paint -= allclips; // make sure to avoid already clipped areas
        if( data.paint.isEmpty()) // completely clipped
            continue;
        if( data.paint != region ) // prepaint added area to draw
            {
            region |= data.paint; // make sure other windows in that area get painted too
            painted_region |= data.paint; // make sure it makes it to the screen
            }
        // If the window is transparent, the transparent part will be done
        // in the 2nd pass.
        if( data.mask & PAINT_WINDOW_TRANSLUCENT )
            phase2.prepend( Phase2Data( w, data.paint, data.clip, data.mask, data.quads ));
        if( data.mask & PAINT_WINDOW_OPAQUE )
            {
            w->setTransformedShape( QRegion());
            paintWindow( w, data.mask, data.paint, data.quads );
            // The window can clip by its opaque parts the windows below.
            region -= w->transformedShape();
            }
        // translucency or window transformed require window pixmap
        w->suspendUnredirect( data.mask & ( PAINT_WINDOW_TRANSLUCENT | PAINT_WINDOW_TRANSFORMED )); 
        }
    if( !( orig_mask & PAINT_SCREEN_BACKGROUND_FIRST ))
        paintBackground( region ); // Fill any areas of the root window not covered by windows
    // Now walk the list bottom to top, drawing translucent windows.
    // That we draw bottom to top is important now since we're drawing translucent objects
    // and also are clipping only by opaque windows.
    QRegion add_paint;
    foreach( const Phase2Data &d, phase2 )
        {
        Scene::Window* w = d.window;
        paintWindow( w, d.mask, d.region | add_paint, d.quads );
        // It is necessary to also add paint regions of windows below, because their
        // pre-paint's might have extended the paint area, so those areas need to be painted too.
        add_paint |= d.region;
        }
    }

// fill the screen background
void SceneXrender::paintBackground( QRegion region )
    {
    PaintClipper pc( region );
    for( PaintClipper::Iterator iterator;
         !iterator.isDone();
         iterator.next())
        {
        XRenderColor col = { 0, 0, 0, 0xffff }; // black
        XRenderFillRectangle( display(), PictOpSrc, buffer, &col, 0, 0, displayWidth(), displayHeight());
        }
    }

void SceneXrender::windowGeometryShapeChanged( Toplevel* c )
    {
    if( !windows.contains( c )) // this is ok, shape is not valid by default
        return;
    Window* w = windows[ c ];
    w->discardPicture();
    w->discardShape();
    w->discardAlpha();
    }
    
void SceneXrender::windowOpacityChanged( Toplevel* c )
    {
    if( !windows.contains( c )) // this is ok, alpha is created on demand
        return;
    Window* w = windows[ c ];
    w->discardAlpha();
    }

void SceneXrender::windowClosed( Toplevel* c, Deleted* deleted )
    {
    assert( windows.contains( c ));
    if( deleted != NULL )
        { // replace c with deleted
        Window* w = windows.take( c );
        w->updateToplevel( deleted );
        windows[ deleted ] = w;
        }
    else
        {
        delete windows.take( c );
        c->effectWindow()->setSceneWindow( NULL );
        }
    }

void SceneXrender::windowDeleted( Deleted* c )
    {
    assert( windows.contains( c ));
    delete windows.take( c );
    c->effectWindow()->setSceneWindow( NULL );
    }

void SceneXrender::windowAdded( Toplevel* c )
    {
    assert( !windows.contains( c ));
    windows[ c ] = new Window( c );
    c->effectWindow()->setSceneWindow( windows[ c ]);
    }

//****************************************
// SceneXrender::Window
//****************************************

QPixmap *SceneXrender::Window::temp_pixmap = 0;

SceneXrender::Window::Window( Toplevel* c )
    : Scene::Window( c )
    , _picture( None )
    , format( XRenderFindVisualFormat( display(), c->visual()))
    , alpha( None )
    , alpha_cached_opacity(0.0)
    {
    }

SceneXrender::Window::~Window()
    {
    discardPicture();
    discardAlpha();
    discardShape();
    }

// Create XRender picture for the pixmap with the window contents.
Picture SceneXrender::Window::picture()
    {
    if( !toplevel->damage().isEmpty() && _picture != None )
        {
        XRenderFreePicture( display(), _picture );
        _picture = None;
        }
    if( _picture == None && format != NULL )
        {
        // Get the pixmap with the window contents.
        Pixmap pix = toplevel->windowPixmap();
        if( pix == None )
            return None;
        _picture = XRenderCreatePicture( display(), pix, format, 0, 0 );
        toplevel->resetDamage( toplevel->rect());
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

// Create XRender picture for the alpha mask.
Picture SceneXrender::Window::alphaMask( double opacity )
    {
    if( isOpaque() && qFuzzyCompare( opacity, 1.0 ) )
        return None;

    bool created = false;
    if( alpha == None )
        {
        // Create a 1x1 8bpp pixmap containing the given opacity in the alpha channel.
        Pixmap pixmap = XCreatePixmap( display(), rootWindow(), 1, 1, 8 );
        XRenderPictFormat* format = XRenderFindStandardFormat( display(), PictStandardA8 );
        XRenderPictureAttributes pa;
        pa.repeat = True;
        alpha = XRenderCreatePicture( display(), pixmap, format, CPRepeat, &pa );
        XFreePixmap( display(), pixmap );
        created = true;
        }
    if( created || !qFuzzyCompare( alpha_cached_opacity + 1.0, opacity + 1.0 ) )
        {
        XRenderColor col;
        col.alpha = int( opacity * 0xffff );
        XRenderFillRectangle( display(), PictOpSrc, alpha, &col, 0, 0, 1, 1 );
        alpha_cached_opacity = opacity;
        }
    return alpha;
    }

// Maps window coordinates to screen coordinates
QRect SceneXrender::Window::mapToScreen( int mask, const WindowPaintData &data, const QRect &rect ) const
    {
    QRect r = rect;

    if( mask & PAINT_WINDOW_TRANSFORMED )
        {
        // Apply the window transformation
        r.moveTo( r.x() * data.xScale + data.xTranslate,
                  r.y() * data.yScale + data.yTranslate );
        r.setWidth( r.width() * data.xScale );
        r.setHeight( r.height() * data.yScale );
        }

    // Move the rectangle to the screen position
    r.translate( x(), y() );

    if( mask & PAINT_SCREEN_TRANSFORMED )
        {
        // Apply the screen transformation
        r.moveTo( r.x() * screen_paint.xScale + screen_paint.xTranslate,
                  r.y() * screen_paint.yScale + screen_paint.yTranslate );
        r.setWidth( r.width() * screen_paint.xScale );
        r.setHeight( r.height() * screen_paint.yScale );
        }

    return r;
    }

// Maps window coordinates to screen coordinates
QPoint SceneXrender::Window::mapToScreen( int mask, const WindowPaintData &data, const QPoint &point ) const
    {
    QPoint pt = point;

    if( mask & PAINT_WINDOW_TRANSFORMED )
        {
        // Apply the window transformation
        pt.rx() = pt.x() * data.xScale + data.xTranslate;
        pt.ry() = pt.y() * data.yScale + data.yTranslate;
        }

    // Move the point to the screen position
    pt += QPoint(x(), y());

    if( mask & PAINT_SCREEN_TRANSFORMED )
        {
        // Apply the screen transformation
        pt.rx() = pt.x() * screen_paint.xScale + screen_paint.xTranslate;
        pt.ry() = pt.y() * screen_paint.yScale + screen_paint.yTranslate;
        }

    return pt;
    }

void SceneXrender::Window::prepareTempPixmap(const QPixmap *left, const QPixmap *top,
                                             const QPixmap *right, const QPixmap *bottom)
    {
    const QRect r = static_cast<Client*>( toplevel )->decorationRect();

    if( !temp_pixmap )
        temp_pixmap = new QPixmap( r.width(), r.height() );
    else if( temp_pixmap->width() < r.width() || temp_pixmap->height() < r.height() )
        *temp_pixmap = QPixmap( r.width(), r.height() );

    temp_pixmap->fill( Qt::transparent );

    Display *dpy = display();
    XRenderComposite( dpy, PictOpSrc, top->x11PictureHandle(), None, temp_pixmap->x11PictureHandle(),
                      0, 0, 0, 0, 0, 0, top->width(), top->height() );
    XRenderComposite( dpy, PictOpSrc, left->x11PictureHandle(), None, temp_pixmap->x11PictureHandle(),
                      0, 0, 0, 0, 0, top->height(), left->width(), left->height() );
    XRenderComposite( dpy, PictOpSrc, right->x11PictureHandle(), None, temp_pixmap->x11PictureHandle(),
                      0, 0, 0, 0, r.width() - right->width(), top->height(), right->width(), right->height() );
    XRenderComposite( dpy, PictOpSrc, bottom->x11PictureHandle(), None, temp_pixmap->x11PictureHandle(),
                      0, 0, 0, 0, 0, r.height() - bottom->height(), bottom->width(), bottom->height() );
    }

// paint the window
void SceneXrender::Window::performPaint( int mask, QRegion region, WindowPaintData data )
    {
    setTransformedShape( QRegion()); // maybe nothing will be painted
    // check if there is something to paint
    bool opaque = isOpaque() && qFuzzyCompare( data.opacity, 1.0 );
    /* HACK: It seems this causes painting glitches, disable temporarily
    if(( mask & PAINT_WINDOW_OPAQUE ) ^ ( mask & PAINT_WINDOW_TRANSLUCENT ))
        { // We are only painting either opaque OR translucent windows, not both
        if( mask & PAINT_WINDOW_OPAQUE && !opaque )
            return; // Only painting opaque and window is translucent
        if( mask & PAINT_WINDOW_TRANSLUCENT && opaque )
            return; // Only painting translucent and window is opaque
        }*/
    Picture pic = picture(); // get XRender picture
    if( pic == None ) // The render format can be null for GL and/or Xv visuals
        return;
    // set picture filter
    if( options->xrenderSmoothScale ) // only when forced, it's slow
        {
        if( mask & PAINT_WINDOW_TRANSFORMED )
            filter = ImageFilterGood;
        else if( mask & PAINT_SCREEN_TRANSFORMED )
            filter = ImageFilterGood;
        else
            filter = ImageFilterFast;
        }
    else
        filter = ImageFilterFast;
    // do required transformations
    const QRect wr = mapToScreen(mask, data, QRect(0, 0, width(), height()));
    const QRect cr = QRect(toplevel->clientPos(), toplevel->clientSize()); // Client rect (in the window)
    const QRect dr = mapToScreen(mask, data, cr); // Destination rect
    double xscale = 1;
    double yscale = 1;
    bool scaled = false;

    Client *client = dynamic_cast<Client*>( toplevel );
    Deleted *deleted = dynamic_cast<Deleted*>( toplevel );
    if ( client && Workspace::self()->decorationHasAlpha() )
        transformed_shape = QRegion( client->decorationRect() );
    else if( deleted && Workspace::self()->decorationHasAlpha() )
        transformed_shape = QRegion( deleted->decorationRect() );
    else
        transformed_shape = shape();

    XTransform xform = {{
        { XDoubleToFixed( 1 ), XDoubleToFixed( 0 ), XDoubleToFixed( 0 ) },
        { XDoubleToFixed( 0 ), XDoubleToFixed( 1),  XDoubleToFixed( 0 ) },
        { XDoubleToFixed( 0 ), XDoubleToFixed( 0 ), XDoubleToFixed( 1 ) }
    }};
 
    XTransform identity = {{
        { XDoubleToFixed( 1 ), XDoubleToFixed( 0 ), XDoubleToFixed( 0 ) },
        { XDoubleToFixed( 0 ), XDoubleToFixed( 1),  XDoubleToFixed( 0 ) },
        { XDoubleToFixed( 0 ), XDoubleToFixed( 0 ), XDoubleToFixed( 1 ) }
    }};
 
    if( mask & PAINT_WINDOW_TRANSFORMED )
        {
        xscale = data.xScale;
        yscale = data.yScale;
        }
    if( mask & PAINT_SCREEN_TRANSFORMED )
        {
        xscale *= screen_paint.xScale;
        yscale *= screen_paint.yScale;
        }
    if( !qFuzzyCompare( xscale, 1.0 ) || !qFuzzyCompare( yscale, 1.0 ) )
        {
        scaled = true;
        xform.matrix[0][0] = XDoubleToFixed(1.0 / xscale);
        xform.matrix[1][1] = XDoubleToFixed(1.0 / yscale);

        XRenderSetPictureTransform( display(), pic, &xform );
        if( filter == ImageFilterGood )
            XRenderSetPictureFilter( display(), pic, const_cast< char* >( "good" ), NULL, 0 );

        // transform the shape for clipping in paintTransformedScreen()
        QVector< QRect > rects = transformed_shape.rects();
        for( int i = 0;
             i < rects.count();
             ++i )
            {
            QRect& r = rects[ i ];
            r = QRect( int( r.x() * xscale ), int( r.y() * yscale ),
                int( r.width() * xscale ), int( r.height() * xscale ));
            }
        transformed_shape.setRects( rects.constData(), rects.count());
        }

    transformed_shape.translate( mapToScreen( mask, data, QPoint(0, 0) ) );
    PaintClipper pcreg( region ); // clip by the region to paint
    PaintClipper pc( transformed_shape ); // clip by window's shape
    for( PaintClipper::Iterator iterator;
         !iterator.isDone();
         iterator.next())
        {
        if ( !(mask & PAINT_DECORATION_ONLY) )
            {
            // Paint the window contents
            if( opaque )
                {
                XRenderComposite( display(), PictOpSrc, pic, None, buffer, cr.x() * xscale, cr.y() * yscale,
                                  0, 0, dr.x(), dr.y(), dr.width(), dr.height());
                }
            else
                {
                Picture alpha = alphaMask( data.opacity );
                XRenderComposite( display(), PictOpOver, pic, alpha, buffer, cr.x() * xscale, cr.y() * yscale,
                                  0, 0, dr.x(), dr.y(), dr.width(), dr.height());
                transformed_shape = QRegion();
                }
            }
        if( client || deleted )
            {
            bool noBorder = true;
            const QPixmap *left;
            const QPixmap *top;
            const QPixmap *right;
            const QPixmap *bottom;
            QRect tr, lr, rr, br;
            QRect decorationRect;
            if( client && !client->noBorder() )
                {
                noBorder = client->noBorder();
                client->ensureDecorationPixmapsPainted();

                left   = client->leftDecoPixmap();
                top    = client->topDecoPixmap();
                right  = client->rightDecoPixmap();
                bottom = client->bottomDecoPixmap();
                client->layoutDecorationRects( lr, tr, rr, br, Client::WindowRelative );
                decorationRect = client->decorationRect();
                }
            if( deleted && !deleted->noBorder() )
                {
                noBorder = deleted->noBorder();
                left   = deleted->leftDecoPixmap();
                top    = deleted->topDecoPixmap();
                right  = deleted->rightDecoPixmap();
                bottom = deleted->bottomDecoPixmap();
                deleted->layoutDecorationRects( lr, tr, rr, br );
                decorationRect = deleted->decorationRect();
                }
            if( !noBorder )
                {
                // Paint the decoration
                Picture alpha = alphaMask( data.opacity * data.decoration_opacity );
                Display *dpy = display();

                if( !scaled )
                    {
                    tr = mapToScreen( mask, data, tr );
                    lr = mapToScreen( mask, data, lr );
                    rr = mapToScreen( mask, data, rr );
                    br = mapToScreen( mask, data, br );

                    XRenderComposite( dpy, PictOpOver, top->x11PictureHandle(), alpha, buffer,
                                      0, 0, 0, 0, tr.x(), tr.y(), tr.width(), tr.height());
                    XRenderComposite( dpy, PictOpOver, left->x11PictureHandle(), alpha, buffer,
                                      0, 0, 0, 0, lr.x(), lr.y(), lr.width(), lr.height());
                    XRenderComposite( dpy, PictOpOver, right->x11PictureHandle(), alpha, buffer,
                                      0, 0, 0, 0, rr.x(), rr.y(), rr.width(), rr.height());
                    XRenderComposite( dpy, PictOpOver, bottom->x11PictureHandle(), alpha, buffer,
                                      0, 0, 0, 0, br.x(), br.y(), br.width(), br.height());
                    }
                else
                    {
                    const QRect r = mapToScreen( mask, data, decorationRect );
                    prepareTempPixmap( left, top, right, bottom );
                    XRenderSetPictureTransform( dpy, temp_pixmap->x11PictureHandle(), &xform );
                    XRenderComposite( dpy, PictOpOver, temp_pixmap->x11PictureHandle(), alpha, buffer,
                                      0, 0, 0, 0, r.x(), r.y(), r.width(), r.height() );
                    XRenderSetPictureTransform( dpy, temp_pixmap->x11PictureHandle(), &identity );
                    }
                }
            }
        if( data.brightness < 1.0 )
            {
            // fake brightness change by overlaying black
            XRenderColor col = { 0, 0, 0, 0xffff * ( 1 - data.brightness ) * data.opacity };
            XRenderFillRectangle( display(), PictOpOver, buffer, &col, wr.x(), wr.y(), wr.width(), wr.height() );
            }
        }
    if( scaled )
        {
        XRenderSetPictureTransform( display(), pic, &identity );
        if( filter == ImageFilterGood )
            XRenderSetPictureFilter( display(), pic, const_cast< char* >( "fast" ), NULL, 0 );
        }
    }

} // namespace
#endif
