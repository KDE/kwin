/*****************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2006 Lubos Lunak <l.lunak@kde.org>

You can Freely distribute this program under the GNU General Public
License. See the file "COPYING" for the exact licensing terms.
******************************************************************/

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

// returns mask and possibly modified region
void Scene::paintScreen( int* mask, QRegion* region )
    {
    *mask = ( *region == QRegion( 0, 0, displayWidth(), displayHeight()))
        ? 0 : PAINT_SCREEN_REGION;
    updateTimeDiff();
    // preparation step
    effects->prePaintScreen( mask, region, time_diff );
    if( *mask & ( PAINT_SCREEN_TRANSFORMED | PAINT_WINDOW_TRANSFORMED ))
        *mask &= ~PAINT_SCREEN_REGION;
    // TODO call also prePaintWindow() for all windows
    ScreenPaintData data;
    effects->paintScreen( *mask, *region, data );
    }

void Scene::updateTimeDiff()
    {
    if( last_time.isNull())
        {
        // has been idle for some time, time_diff would be huge
        time_diff = 0;
        }
    else
        time_diff = last_time.elapsed();
    if( time_diff < 0 )
        time_diff = 0;
    last_time.start();;
    }

void Scene::idle()
    {
    last_time = QTime();
    }

// the function that'll be eventually called by paintScreen() above
void Scene::finalPaintScreen( int mask, QRegion region, ScreenPaintData& data )
    {
    if( mask & PAINT_SCREEN_REGION )
        paintSimpleScreen( mask, region );
    else
        paintGenericScreen( mask, data );
    }

// the generic painting code that should eventually handle even
// transformations
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

// the optimized case without any transformations at all
void Scene::paintSimpleScreen( int orig_mask, QRegion region )
    {
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
        if( mask & PAINT_WINDOW_TRANSLUCENT )
            phase2.prepend( Phase2Data( w, region, mask ));
        if( mask & PAINT_WINDOW_OPAQUE )
            {
            paintWindow( w, mask, region );
            if( ( mask & PAINT_WINDOW_TRANSLUCENT ) == 0 ) // window is not transparent, can clip windows below
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

void Scene::windowGeometryShapeChanged( Toplevel* )
    {
    }

void Scene::windowOpacityChanged( Toplevel* )
    {
    }

void Scene::windowAdded( Toplevel* )
    {
    }

void Scene::windowDeleted( Toplevel* )
    {
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
    shape_valid = false;
    }

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
