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
class EffectWindow;

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
        virtual void prePaintWindow( EffectWindow* w, int* mask, QRegion* region, int time );
        virtual void paintWindow( EffectWindow* w, int mask, QRegion region, WindowPaintData& data );
        virtual void postPaintWindow( EffectWindow* w );
        // called when moved/resized or once after it's finished
        virtual void windowUserMovedResized( EffectWindow* c, bool first, bool last );
        virtual void windowAdded( EffectWindow* c );
        virtual void windowClosed( EffectWindow* c );
        virtual void windowDeleted( EffectWindow* c );
        virtual void windowActivated( EffectWindow* c );
        virtual void windowMinimized( EffectWindow* c );
        virtual void windowUnminimized( EffectWindow* c );
        virtual void windowInputMouseEvent( Window w, QEvent* e );
        virtual void desktopChanged( int old );

        // Interpolates between x and y
        static float interpolate(float x, float y, float a)
            {
            return x * (1 - a) + y * a;
            }

    protected:
        Workspace* workspace() const;
    };

class EffectFactory
    {
    public:
        // only here to avoid warnings
        virtual ~EffectFactory();

        virtual Effect* create() const = 0;
    };

template <class EFFECT>
class GenericEffectFactory : public EffectFactory
    {
        virtual Effect* create() const
            {
            return new EFFECT();
            }
    };


class EffectsHandler
    {
    friend class Effect;
    public:
        EffectsHandler();
        ~EffectsHandler();
        // for use by effects
        void prePaintScreen( int* mask, QRegion* region, int time );
        void paintScreen( int mask, QRegion region, ScreenPaintData& data );
        void postPaintScreen();
        void prePaintWindow( EffectWindow* w, int* mask, QRegion* region, int time );
        void paintWindow( EffectWindow* w, int mask, QRegion region, WindowPaintData& data );
        void postPaintWindow( EffectWindow* w );
        // Functions for handling input - e.g. when an Expose-like effect is shown, an input window
        // covering the whole screen is created and all mouse events will be intercepted by it.
        // The effect's windowInputMouseEvent() will get called with such events.
        Window createInputWindow( Effect* e, int x, int y, int w, int h, const QCursor& cursor );
        Window createInputWindow( Effect* e, const QRect& r, const QCursor& cursor );
        void destroyInputWindow( Window w );
        // functions that allow controlling windows/desktop
        void activateWindow( EffectWindow* c );
        // 
        int currentDesktop() const;
        // internal (used by kwin core or compositing code)
        void startPaint();
        void windowUserMovedResized( EffectWindow* c, bool first, bool last );
        void windowAdded( EffectWindow* c );
        void windowClosed( EffectWindow* c );
        void windowDeleted( EffectWindow* c );
        void windowActivated( EffectWindow* c );
        void windowMinimized( EffectWindow* c );
        void windowUnminimized( EffectWindow* c );
        bool checkInputWindowEvent( XEvent* e );
        void checkInputWindowStacking();
        void desktopChanged( int old );
        void registerEffect( const QString& name, EffectFactory* factory );
        void loadEffect( const QString& name );
        void unloadEffect( const QString& name );

    private:
        typedef QPair< QString, Effect* > EffectPair;
        QVector< EffectPair > loaded_effects;
        typedef QPair< Effect*, Window > InputWindowPair;
        QList< InputWindowPair > input_windows;
        QMap< QString, EffectFactory* > effect_factories;
        int current_paint_window;
        int current_paint_screen;
    };

// This class is a representation of a window used by/for Effect classes.
// The purpose is to hide internal data and also to serve as a single
// representation for the case when Client/Unmanaged becomes Deleted.
class EffectWindow
    {
    public:
        EffectWindow();
        const Toplevel* window() const;
        Toplevel* window();
        void enablePainting( int reason );
        void disablePainting( int reason );
        bool isOnDesktop( int d ) const;
        bool isOnCurrentDesktop() const;
        bool isOnAllDesktops() const;
        int desktop() const; // prefer isOnXXX()

        void setWindow( Toplevel* w ); // internal
        void setSceneWindow( Scene::Window* w ); // internal
        Scene::Window* sceneWindow(); // internal
    private:
        Toplevel* toplevel;
        Scene::Window* sw; // This one is used only during paint pass.
    };

EffectWindow* effectWindow( Toplevel* w );
EffectWindow* effectWindow( Scene::Window* w );

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

inline
const Toplevel* EffectWindow::window() const
    {
    return toplevel;
    }

inline
Toplevel* EffectWindow::window()
    {
    return toplevel;
    }

inline
void EffectWindow::setWindow( Toplevel* w )
    {
    toplevel = w;
    }

inline
void EffectWindow::setSceneWindow( Scene::Window* w )
    {
    sw = w;
    }

inline
Scene::Window* EffectWindow::sceneWindow()
    {
    return sw;
    }

inline
EffectWindow* effectWindow( Toplevel* w )
    {
    EffectWindow* ret = w->effectWindow();
    ret->setSceneWindow( NULL ); // just in case
    return ret;
    }

inline
EffectWindow* effectWindow( Scene::Window* w )
    {
    EffectWindow* ret = w->window()->effectWindow();
    ret->setSceneWindow( w );
    return ret;
    }

inline
Workspace* Effect::workspace() const
    {
    return Workspace::self();
    }

} // namespace

#endif
