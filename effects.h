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

#include <qmap.h>
#include <qpoint.h>
#include <qtimer.h>

#include "scene.h"

namespace KWinInternal
{

class Toplevel;
class Workspace;

class WindowPaintData
    {
    public:
        WindowPaintData();
        double opacity;
        double xScale;
        double yScale;
        int xTranslate;
        int yTranslate;
    };

class ScreenPaintData
    {
    public:
        ScreenPaintData();
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
        // internal (used by kwin core or compositing code)
        void startPaint();
        void windowUserMovedResized( Toplevel* c, bool first, bool last );
        void windowAdded( Toplevel* c );
        void windowDeleted( Toplevel* c );
    private:
        QVector< Effect* > effects;
        int current_paint_window;
        int current_paint_screen;
    };

extern EffectsHandler* effects;

class MakeHalfTransparent
    : public Effect
    {
    public:
        virtual void windowUserMovedResized( Toplevel* c, bool first, bool last );
        virtual void prePaintWindow( Scene::Window* w, int* mask, QRegion* region, int time );
        virtual void paintWindow( Scene::Window* w, int mask, QRegion region, WindowPaintData& data );
    };

class ShakyMove
    : public QObject, public Effect
    {
    Q_OBJECT
    public:
        ShakyMove();
        virtual void windowUserMovedResized( Toplevel* c, bool first, bool last );
        virtual void prePaintScreen( int* mask, QRegion* region, int time );
        virtual void prePaintWindow( Scene::Window* w, int* mask, QRegion* region, int time );
        virtual void paintWindow( Scene::Window* w, int mask, QRegion region, WindowPaintData& data );
        virtual void windowDeleted( Toplevel* c );
    private slots:
        void tick();
    private:
        QMap< const Toplevel*, int > windows;
        QTimer timer;
    };

#if 0
class GrowMove
    : public Effect
    {
    public:
        virtual void windowUserMovedResized( Toplevel* c, bool first, bool last );
        virtual void transformWindow( Toplevel* c, Matrix& m, EffectData& data );
    };
#endif

class ShiftWorkspaceUp
    : public QObject, public Effect
    {
    Q_OBJECT
    public:
        ShiftWorkspaceUp( Workspace* ws );
        virtual void prePaintScreen( int* mask, QRegion* region, int time );
        virtual void paintScreen( int mask, QRegion region, ScreenPaintData& data );
        virtual void postPaintScreen();
    private slots:
        void tick();
    private:
        QTimer timer;
        bool up;
        int diff;
        Workspace* wspace;
    };

class FadeIn
    : public Effect
    {
    public:
        virtual void prePaintWindow( Scene::Window* w, int* mask, QRegion* region, int time );
        virtual void paintWindow( Scene::Window* w, int mask, QRegion region, WindowPaintData& data );
        virtual void postPaintWindow( Scene::Window* w );
        // TODO react also on virtual desktop changes
        virtual void windowAdded( Toplevel* c );
        virtual void windowDeleted( Toplevel* c );
    private:
        QMap< const Toplevel*, double > windows;
    };

class ScaleIn
    : public Effect
    {
    public:
        virtual void prePaintWindow( Scene::Window* w, int* mask, QRegion* region, int time );
        virtual void paintWindow( Scene::Window* w, int mask, QRegion region, WindowPaintData& data );
        virtual void postPaintWindow( Scene::Window* w );
        // TODO react also on virtual desktop changes
        virtual void windowAdded( Toplevel* c );
        virtual void windowDeleted( Toplevel* c );
    private:
        QMap< const Toplevel*, double > windows;
    };

inline
WindowPaintData::WindowPaintData()
    : opacity( 1.0 )
    , xScale( 1 )
    , yScale( 1 )
    , xTranslate( 0 )
    , yTranslate( 0 )
    {
    }

inline
ScreenPaintData::ScreenPaintData()
    : xTranslate( 0 )
    , yTranslate( 0 )
    {
    }

} // namespace

#endif
