/*****************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2006 Lubos Lunak <l.lunak@kde.org>

You can Freely distribute this program under the GNU General Public
License. See the file "COPYING" for the exact licensing terms.
******************************************************************/

#ifndef KWIN_SCENE_H
#define KWIN_SCENE_H

#include <qdatetime.h>

#include "toplevel.h"
#include "utils.h"
#include "kwineffects.h"

namespace KWin
{

class Workspace;
class Deleted;
class EffectWindowImpl;
class EffectHandlerImpl;

// The base class for compositing backends.
class Scene
    {
    public:
        Scene( Workspace* ws );
        virtual ~Scene() = 0;
        class Window;

        virtual CompositingType compositingType() const = 0;
        // Repaints the given screen areas, windows provides the stacking order.
        // The entry point for the main part of the painting pass.
        virtual void paint( QRegion damage, ToplevelList windows ) = 0;
        
        // Notification function - KWin core informs about changes.
        // Used to mainly discard cached data.
        
        // shape/size of a window changed
        virtual void windowGeometryShapeChanged( Toplevel* ) = 0;
        // opacity of a window changed
        virtual void windowOpacityChanged( Toplevel* ) = 0;
        // a new window has been created
        virtual void windowAdded( Toplevel* ) = 0;
        // a window has been closed
        virtual void windowClosed( Toplevel*, Deleted* ) = 0;
        // a window has been destroyed
        virtual void windowDeleted( Deleted* ) = 0;
        // Flags controlling how painting is done.
        enum
            {
            // Window (or at least part of it) will be painted opaque.
            PAINT_WINDOW_OPAQUE         = 1 << 0,
            // Window (or at least part of it) will be painted translucent.
            PAINT_WINDOW_TRANSLUCENT    = 1 << 1,
            // Window will be painted with transformed geometry.
            PAINT_WINDOW_TRANSFORMED    = 1 << 2,
            // Paint only a region of the screen (can be optimized, cannot
            // be used together with TRANSFORMED flags).
            PAINT_SCREEN_REGION         = 1 << 3,
            // Whole screen will be painted with transformed geometry.
            PAINT_SCREEN_TRANSFORMED    = 1 << 4,
            // At least one window will be painted with transformed geometry.
            PAINT_SCREEN_WITH_TRANSFORMED_WINDOWS = 1 << 5,
            // Clear whole background as the very first step, without optimizing it
            PAINT_SCREEN_BACKGROUND_FIRST = 1 << 6
            };
        // types of filtering available
        enum ImageFilterType { ImageFilterFast, ImageFilterGood };
        // there's nothing to paint (adjust time_diff later)
        void idle();
        bool waitSyncAvailable() { return has_waitSync; }
    protected:
        // shared implementation, starts painting the screen
        void paintScreen( int* mask, QRegion* region );
        friend class EffectsHandlerImpl;
        // called after all effects had their paintScreen() called
        void finalPaintScreen( int mask, QRegion region, ScreenPaintData& data );
        // shared implementation of painting the screen in the generic
        // (unoptimized) way
        virtual void paintGenericScreen( int mask, ScreenPaintData data );
        // shared implementation of painting the screen in an optimized way
        virtual void paintSimpleScreen( int mask, QRegion region );
        // paint the background (not the desktop background - the whole background)
        virtual void paintBackground( QRegion region ) = 0;
        // called after all effects had their paintWindow() called
        void finalPaintWindow( EffectWindowImpl* w, int mask, QRegion region, WindowPaintData& data );
        // shared implementation, starts painting the window
        virtual void paintWindow( Window* w, int mask, QRegion region );
        // called after all effects had their drawWindow() called
        void finalDrawWindow( EffectWindowImpl* w, int mask, QRegion region, WindowPaintData& data );
        // infinite region, i.e. everything
        static QRegion infiniteRegion();
        // compute time since the last repaint
        void updateTimeDiff();
        // saved data for 2nd pass of optimized screen painting
        struct Phase2Data
            {
            Phase2Data( Window* w, QRegion r, int m ) : window( w ), region( r ), mask( m ) {}
            Window* window;
            QRegion region;
            int mask;
            };
        // windows in their stacking order
        QVector< Window* > stacking_order;
        // The region which actually has been painted by paintScreen() and should be
        // copied from the buffer to the screen. I.e. the region returned from Scene::paintScreen().
        // Since prePaintWindow() can extend areas to paint, these changes would have to propagate
        // up all the way from paintSimpleScreen() up to paintScreen(), so save them here rather
        // than propagate them up in arguments.
        QRegion painted_region;
        // time since last repaint
        int time_diff;
        QTime last_time;
        Workspace* wspace;
        bool has_waitSync;
    };

// The base class for windows representations in composite backends
class Scene::Window
    {
    public:
        Window( Toplevel* c );
        virtual ~Window();
        // perform the actual painting of the window
        virtual void performPaint( int mask, QRegion region, WindowPaintData data ) = 0;
        virtual void prepareForPainting()  {}
        int x() const;
        int y() const;
        int width() const;
        int height() const;
        QRect geometry() const;
        QPoint pos() const;
        QSize size() const;
        QRect rect() const;
        // access to the internal window class
        // TODO eventually get rid of this
        Toplevel* window();
        // should the window be painted
        bool isPaintingEnabled() const;
        void resetPaintingEnabled();
        // Flags explaining why painting should be disabled
        enum
            {
            // Window will not be painted
            PAINT_DISABLED              = 1 << 0,
            // Window will not be painted because it is deleted
            PAINT_DISABLED_BY_DELETE    = 1 << 1,
            // Window will not be painted because of which desktop it's on
            PAINT_DISABLED_BY_DESKTOP   = 1 << 2,
            // Window will not be painted because it is minimized
            PAINT_DISABLED_BY_MINIMIZE  = 1 << 3
            };
        void enablePainting( int reason );
        void disablePainting( int reason );
        // is the window visible at all
        bool isVisible() const;
        // is the window fully opaque
        bool isOpaque() const;
        // shape of the window
        QRegion shape() const;
        void discardShape();
        void updateToplevel( Toplevel* c );
    protected:
        Toplevel* toplevel;
        ImageFilterType filter;
    private:
        int disable_painting;
        mutable QRegion shape_region;
        mutable bool shape_valid;
        Q_DISABLE_COPY(Window)
    };

extern Scene* scene;

inline
QRegion Scene::infiniteRegion()
    { // INT_MIN / 2 because width/height is used (INT_MIN+INT_MAX==-1)
    return QRegion( INT_MIN / 2, INT_MIN / 2, INT_MAX, INT_MAX );
    }

inline
int Scene::Window::x() const
    {
    return toplevel->x();
    }
    
inline
int Scene::Window::y() const
    {
    return toplevel->y();
    }

inline
int Scene::Window::width() const
    {
    return toplevel->width();
    }
    
inline
int Scene::Window::height() const
    {
    return toplevel->height();
    }

inline
QRect Scene::Window::geometry() const
    {
    return toplevel->geometry();
    }

inline
QSize Scene::Window::size() const
    {
    return toplevel->size();
    }
    
inline
QPoint Scene::Window::pos() const
    {
    return toplevel->pos();
    }
    
inline
QRect Scene::Window::rect() const
    {
    return toplevel->rect();
    }

inline
Toplevel* Scene::Window::window()
    {
    return toplevel;
    }

inline
void Scene::Window::updateToplevel( Toplevel* c )
    {
    toplevel = c;
    }

} // namespace

#endif
