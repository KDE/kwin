/*****************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2006 Lubos Lunak <l.lunak@kde.org>

You can Freely distribute this program under the GNU General Public
License. See the file "COPYING" for the exact licensing terms.
******************************************************************/

// TODO MIT or some other licence, perhaps move to some lib

#ifndef KWIN_EFFECTS_H
#define KWIN_EFFECTS_H

#include "scene.h"

#include <qpair.h>

namespace KWinInternal
{

class Toplevel;
class Workspace;

class WindowPaintData
    {
    public:
        WindowPaintData();
        /**
         * Window opacity, in range 0 = transparent to 1 = fully opaque
         */
        double opacity;
        double xScale;
        double yScale;
        int xTranslate;
        int yTranslate;
        /**
         * Saturation of the window, in range [0; 1]
         * 1 means that the window is unchanged, 0 means that it's completely
         *  unsaturated (greyscale). 0.5 would make the colors less intense,
         *  but not completely grey
         **/
        float saturation;
        /**
         * Brightness of the window, in range [0; 1]
         * 1 means that the window is unchanged, 0 means that it's completely
         * black. 0.5 would make it 50% darker than usual
         **/
        float brightness;
    };

class ScreenPaintData
    {
    public:
        ScreenPaintData();
        double xScale;
        double yScale;
        int xTranslate;
        int yTranslate;
    };

class Effect
    {
    public:
        virtual ~Effect();
        virtual void prePaintScreen( int* mask, QRegion* region, int time );
        virtual void paintScreen( int mask, QRegion region, ScreenPaintData& data );
        virtual void postPaintScreen();
        virtual void prePaintWindow( Scene::Window* w, int* mask, QRegion* region, int time );
        virtual void paintWindow( Scene::Window* w, int mask, QRegion region, WindowPaintData& data );
        virtual void postPaintWindow( Scene::Window* w );
        // called when moved/resized or once after it's finished
        virtual void windowUserMovedResized( Toplevel* c, bool first, bool last );
        virtual void windowAdded( Toplevel* c );
        virtual void windowDeleted( Toplevel* c );
        virtual void windowActivated( Toplevel* c );
        virtual void windowInputMouseEvent( Window w, QEvent* e );
    };

class EffectsHandler
    {
    public:
        EffectsHandler( Workspace* ws );
        ~EffectsHandler();
        // for use by effects
        void prePaintScreen( int* mask, QRegion* region, int time );
        void paintScreen( int mask, QRegion region, ScreenPaintData& data );
        void postPaintScreen();
        void prePaintWindow( Scene::Window* w, int* mask, QRegion* region, int time );
        void paintWindow( Scene::Window* w, int mask, QRegion region, WindowPaintData& data );
        void postPaintWindow( Scene::Window* w );
        // Functions for handling input - e.g. when an Expose-like effect is shown, an input window
        // covering the whole screen is created and all mouse events will be intercepted by it.
        // The effect's windowInputMouseEvent() will get called with such events.
        Window createInputWindow( Effect* e, int x, int y, int w, int h, const QCursor& cursor );
        Window createInputWindow( Effect* e, const QRect& r, const QCursor& cursor );
        void destroyInputWindow( Window w );
        // functions that allow controlling windows/desktop
        void activateWindow( Client* c );
        // internal (used by kwin core or compositing code)
        void startPaint();
        void windowUserMovedResized( Toplevel* c, bool first, bool last );
        void windowAdded( Toplevel* c );
        void windowDeleted( Toplevel* c );
        void windowActivated( Toplevel* c );
        bool checkInputWindowEvent( XEvent* e );
        void checkInputWindowStacking();
    private:
        QVector< Effect* > effects;
        typedef QPair< Effect*, Window > InputWindowPair;
        QList< InputWindowPair > input_windows;
        int current_paint_window;
        int current_paint_screen;
    };

extern EffectsHandler* effects;

inline
WindowPaintData::WindowPaintData()
    : opacity( 1.0 )
    , xScale( 1 )
    , yScale( 1 )
    , xTranslate( 0 )
    , yTranslate( 0 )
    , saturation( 1 )
    , brightness( 1 )
    {
    }

inline
ScreenPaintData::ScreenPaintData()
    : xScale( 1 )
    , yScale( 1 )
    , xTranslate( 0 )
    , yTranslate( 0 )
    {
    }

} // namespace

#endif
