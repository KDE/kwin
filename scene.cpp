/*****************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2006 Lubos Lunak <l.lunak@kde.org>

You can Freely distribute this program under the GNU General Public
License. See the file "COPYING" for the exact licensing terms.
******************************************************************/

#include "scene_basic.h"
#include "client.h"

#include <X11/extensions/shape.h>

#include "effects.h"

namespace KWinInternal
{

//****************************************
// Scene::WrapperEffect
//****************************************

Scene::WrapperEffect::~WrapperEffect()
    {
    }

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

void Scene::WrapperEffect::prePaintScreen( int*, QRegion* )
    {
    // nothing, no changes
    }

// the function that'll be eventually called by paintScreen() above
void Scene::WrapperEffect::paintScreen( int mask, QRegion region, ScreenPaintData& data )
    {
    if( mask & PAINT_SCREEN_REGION )
        scene->paintSimpleScreen( region );
    else
        scene->paintGenericScreen();
    }
    
// the generic painting code that should eventually handle even
// transformations
void Scene::paintGenericScreen()
    {
    paintBackground( infiniteRegion());
    foreach( Window* w, stacking_order ) // bottom to top
        {
        if( !w->isVisible())
            continue;
        paintWindow( w, PAINT_WINDOW_OPAQUE | PAINT_WINDOW_TRANSLUCENT, infiniteRegion());
        }
    }

// the optimized case without any transformations at all
void Scene::paintSimpleScreen( QRegion region )
    {
    QList< Phase2Data > phase2;
    // TODO repaint only damaged areas (means also don't do glXSwapBuffers and similar)
    region = QRegion( 0, 0, displayWidth(), displayHeight());
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
        if( !w->isOpaque())
            {
            phase2.prepend( Phase2Data( w, region ));
            continue;
            }
        paintWindow( w, PAINT_WINDOW_OPAQUE, region );
        // window is opaque, clip windows below
        region -= w->shape().translated( w->x(), w->y());
        }
    // Fill any areas of the root window not covered by windows
    paintBackground( region );
    // Now walk the list bottom to top, drawing translucent windows.
    // That we draw bottom to top is important now since we're drawing translucent objects
    // and also are clipping only by opaque windows.
    foreach( Phase2Data d, phase2 )
        {
        Window* w = d.window;
        paintWindow( w, PAINT_WINDOW_TRANSLUCENT, d.region );
        }
    }

void Scene::WrapperEffect::prePaintWindow( Scene::Window* , int*, QRegion* )
    {
    // nothing, no changes
    }

void Scene::paintWindow( Window* w, int mask, QRegion region )
    {
    WindowPaintData data;
//        data.opacity = w->opacity();
    WrapperEffect wrapper;
    effects->paintWindow( w, mask, region, data, &wrapper );
    }

// the function that'll be eventually called by paintWindow() above
void Scene::WrapperEffect::paintWindow( Scene::Window* w, int mask, QRegion region, WindowPaintData& data )
    {
    w->performPaint( region, mask );
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
