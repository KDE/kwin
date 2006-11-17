/*****************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2006 Lubos Lunak <l.lunak@kde.org>

You can Freely distribute this program under the GNU General Public
License. See the file "COPYING" for the exact licensing terms.
******************************************************************/

/*
 (NOTE: The compositing code is work in progress. As such this design
 documentation may get outdated in some areas.)

 The base class for compositing, implementing shared functionality
 between the OpenGL and XRender backends.
 
 Design:
 
 When compositing is turned on, XComposite extension is used to redirect
 drawing of windows to pixmaps and XDamage extension is used to get informed
 about damage (changes) to window contents. This code is mostly in composite.cpp .
 
 Workspace::performCompositing() starts one painting pass. Painting is done
 by painting the screen, which in turn paints every window. Painting can be affected
 using effects, which are chained. E.g. painting a screen means that actually
 paintScreen() of the first effect is called, which possibly does modifications
 and calls next effect's paintScreen() and so on, until Scene::finalPaintScreen()
 is called.
 
 There are 3 phases of every paint (not necessarily done together):
 The pre-paint phase, the paint phase and the post-paint phase.
 
 The pre-paint phase is used to find out about how the painting will be actually
 done (i.e. what the effects will do). For example when only a part of the screen
 needs to be updated and no effect will do any transformation it is possible to use
 an optimized paint function. How the painting will be done is controlled
 by the mask argument, see PAINT_WINDOW_* and PAINT_SCREEN_* flags in scene.h .
 For example an effect that decides to paint a normal windows as translucent
 will need to modify the mask in its prePaintWindow() to include
 the PAINT_WINDOW_TRANSLUCENT flag. The paintWindow() function will then get
 the mask with this flag turned on and will also paint using transparency.
 
 The paint pass does the actual painting, based on the information collected
 using the pre-paint pass. After running through the effects' paintScreen()
 either paintGenericScreen() or optimized paintSimpleScreen() are called.
 Those call paintWindow() on windows (not necessarily all), possibly using
 clipping to optimize performance and calling paintWindow() first with only
 PAINT_WINDOW_OPAQUE to paint the opaque parts and then later
 with PAINT_WINDOW_TRANSLUCENT to paint the transparent parts. Function
 paintWindow() again goes through effects' paintWindow() until
 finalPaintWindow() is called, which calls the window's performPaint() to
 do the actual painting.
 
 The post-paint can be used for cleanups and is also used for scheduling
 repaints during the next painting pass for animations. Effects wanting to
 repaint certain parts can manually damage them during post-paint and repaint
 of these parts will be done during the next paint pass.
 
 
 Various notes:
 
 - When the screen or a window are transformed (*_TRANSFORMED flag), clipping
   and similar optimizations are not done (too complicated), so in such cases
   infiniteRegion() should be always used. Make sure not to make any transformations
   of such regions.
 
*/

#include "scene.h"

#include <X11/extensions/shape.h>

#include "client.h"
#include "effects.h"

namespace KWinInternal
{

//****************************************
// Scene
//****************************************

Scene* scene;

Scene::Scene( Workspace* ws )
    : wspace( ws )
    {
    }
    
Scene::~Scene()
    {
    }

void Scene::initPaint()
    {
    effects->startPaint();
    // do the rest of prepaint pass together with paint pass
    }

// returns mask and possibly modified region
void Scene::paintScreen( int* mask, QRegion* region )
    {
    *mask = ( *region == QRegion( 0, 0, displayWidth(), displayHeight()))
        ? 0 : PAINT_SCREEN_REGION;
    updateTimeDiff();
    // preparation step
    effects->prePaintScreen( mask, region, time_diff );
    if( *mask & ( PAINT_SCREEN_TRANSFORMED | PAINT_WINDOW_TRANSFORMED ))
        { // optimized painting is not possible with transformations
        *mask &= ~PAINT_SCREEN_REGION;
        *region = infiniteRegion();
        }
    else if(( *mask & PAINT_SCREEN_REGION ) == 0 )
        { // force region to be full
        *region = QRegion( 0, 0, displayWidth(), displayHeight());
        }
    ScreenPaintData data;
    effects->paintScreen( *mask, *region, data );
    }

// Compute time since the last painting pass.
void Scene::updateTimeDiff()
    {
    if( last_time.isNull())
        {
        // Painting has been idle (optimized out) for some time,
        // which means time_diff would be huge and would break animations.
        // Simply set it to zero.
        time_diff = 0;
        }
    else
        time_diff = last_time.elapsed();
    if( time_diff < 0 ) // check time rollback
        time_diff = 0;
    last_time.start();;
    }

// Painting pass is optimized away.
void Scene::idle()
    {
    // Don't break time since last paint for the next pass.
    last_time = QTime();
    }

// the function that'll be eventually called by paintScreen() above
void Scene::finalPaintScreen( int mask, QRegion region, ScreenPaintData& data )
    {
    if( mask & ( PAINT_SCREEN_TRANSFORMED | PAINT_WINDOW_TRANSFORMED ))
        paintGenericScreen( mask, data );
    else
        paintSimpleScreen( mask, region );
    }

// The generic painting code that can handle even transformations.
// It simply paints bottom-to-top.
void Scene::paintGenericScreen( int orig_mask, ScreenPaintData )
    {
    paintBackground( infiniteRegion());
    foreach( Window* w, stacking_order ) // bottom to top
        {
        if( !w->isVisible())
            continue;
        int mask = orig_mask | ( w->isOpaque() ? PAINT_WINDOW_OPAQUE : PAINT_WINDOW_TRANSLUCENT );
        QRegion damage = infiniteRegion();
        // preparation step
        effects->prePaintWindow( w, &mask, &damage, time_diff );
        paintWindow( w, mask, damage );
        }
    }

// The optimized case without any transformations at all.
// It can paint only the requested region and can use clipping
// to reduce painting and improve performance.
void Scene::paintSimpleScreen( int orig_mask, QRegion region )
    {
    // TODO PAINT_WINDOW_* flags don't belong here, that's why it's in the assert,
    // perhaps the two enums should be separated
    assert(( orig_mask & ( PAINT_WINDOW_TRANSFORMED | PAINT_SCREEN_TRANSFORMED
        | PAINT_WINDOW_TRANSLUCENT | PAINT_WINDOW_OPAQUE )) == 0 );
    QList< Phase2Data > phase2;
    // Draw each opaque window top to bottom, subtracting the bounding rect of
    // each window from the clip region after it's been drawn.
    for( int i = stacking_order.count() - 1; // top to bottom
         i >= 0;
         --i )
        {
        Window* w = stacking_order[ i ];
        if( !w->isVisible())
            continue;
        if( region.isEmpty()) // completely clipped
            continue;
        int mask = orig_mask | ( w->isOpaque() ? PAINT_WINDOW_OPAQUE : PAINT_WINDOW_TRANSLUCENT );
        QRegion damage = region;
        // preparation step
        effects->prePaintWindow( w, &mask, &damage, time_diff );
        // If the window is transparent, the transparent part will be done
        // in the 2nd pass.
        if( mask & PAINT_WINDOW_TRANSLUCENT )
            phase2.prepend( Phase2Data( w, region, mask ));
        if( mask & PAINT_WINDOW_OPAQUE )
            {
            paintWindow( w, mask, region );
            // If the window is not transparent at all, it can clip windows below.
            if( ( mask & PAINT_WINDOW_TRANSLUCENT ) == 0 )
                region -= w->shape().translated( w->x(), w->y());
            }
        }
    // Fill any areas of the root window not covered by windows
    paintBackground( region );
    // Now walk the list bottom to top, drawing translucent windows.
    // That we draw bottom to top is important now since we're drawing translucent objects
    // and also are clipping only by opaque windows.
    foreach( Phase2Data d, phase2 )
        {
        Window* w = d.window;
        paintWindow( w, d.mask, d.region );
        }
    }

void Scene::paintWindow( Window* w, int mask, QRegion region )
    {
    WindowPaintData data;
    data.opacity = w->window()->opacity();
    effects->paintWindow( w, mask, region, data );
    }

// the function that'll be eventually called by paintWindow() above
void Scene::finalPaintWindow( Scene::Window* w, int mask, QRegion region, WindowPaintData& data )
    {
    w->performPaint( mask, region, data );
    }

void Scene::postPaint()
    {
    effects->postPaintScreen();
    foreach( Window* w, stacking_order )
        effects->postPaintWindow( w );
    // do cleanup
    stacking_order.clear();
    }

//****************************************
// Scene::Window
//****************************************

Scene::Window::Window( Toplevel * c )
    : toplevel( c )
    , shape_valid( false )
    {
    }

Scene::Window::~Window()
    {
    }

void Scene::Window::free()
    {
    }

void Scene::Window::discardShape()
    {
    // it is created on-demand and cached, simply
    // reset the flag
    shape_valid = false;
    }

// Find out the shape of the window using the XShape extension
// or if not shape is set then simply it's the window geometry.
QRegion Scene::Window::shape() const
    {
    if( !shape_valid )
        {
        Client* c = dynamic_cast< Client* >( toplevel );
        if( toplevel->shape() || ( c != NULL && !c->mask().isEmpty()))
            {
            int count, order;
            XRectangle* rects = XShapeGetRectangles( display(), toplevel->handle(),
                ShapeBounding, &count, &order );
            if(rects)
                {
                shape_region = QRegion();
                for( int i = 0;
                     i < count;
                     ++i )
                    shape_region += QRegion( rects[ i ].x, rects[ i ].y,
                        rects[ i ].width, rects[ i ].height );
                XFree(rects);
                }
            else
                shape_region = QRegion( 0, 0, width(), height());
            }
        else
            shape_region = QRegion( 0, 0, width(), height());
        shape_valid = true;
        }
    return shape_region;
    }

bool Scene::Window::isVisible() const
    {
    return true; // TODO there may be transformations, so always true for now
    // TODO mapping state?
    return !toplevel->geometry()
        .intersect( QRect( 0, 0, displayWidth(), displayHeight()))
        .isEmpty();
    }

bool Scene::Window::isOpaque() const
    {
    return toplevel->opacity() == 1.0 && !toplevel->hasAlpha();
    }

} // namespace
