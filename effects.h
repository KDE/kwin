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
        virtual void prePaintScreen( int* mask, QRegion* region );
        virtual void paintScreen( int mask, QRegion region, ScreenPaintData& data );
        virtual void prePaintWindow( Scene::Window* w, int* mask, QRegion* region );
        virtual void paintWindow( Scene::Window* w, int mask, QRegion region, WindowPaintData& data );
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
        void nextPrePaintScreen( int* mask, QRegion* region );
        void nextPaintScreen( int mask, QRegion region, ScreenPaintData& data );
        void nextPrePaintWindow( Scene::Window* w, int* mask, QRegion* region );
        void nextPaintWindow( Scene::Window* w, int mask, QRegion region, WindowPaintData& data );
        // internal (used by kwin core or compositing code)
        void prePaintScreen( int* mask, QRegion* region, Effect* final );
        void paintScreen( int mask, QRegion region, ScreenPaintData& data, Effect* final );
        void prePaintWindow( Scene::Window* w, int* mask, QRegion* region, Effect* final );
        void paintWindow( Scene::Window* w, int mask, QRegion region, WindowPaintData& data, Effect* final );
        void windowUserMovedResized( Toplevel* c, bool first, bool last );
        void windowAdded( Toplevel* c );
        void windowDeleted( Toplevel* c );
    private:
        QVector< Effect* > effects;
        int current_paint_window;
        int current_paint_screen;
    };

extern EffectsHandler* effects;

#if 0
class MakeHalfTransparent
    : public Effect
    {
    public:
        virtual void windowUserMovedResized( Toplevel* c, bool first, bool last );
        virtual void transformWindow( Toplevel* c, Matrix& m, EffectData& data );
    };

class ShakyMove
    : public QObject, public Effect
    {
    Q_OBJECT
    public:
        ShakyMove();
        virtual void windowUserMovedResized( Toplevel* c, bool first, bool last );
        virtual void transformWindow( Toplevel* c, Matrix& m, EffectData& data );
        virtual void windowDeleted( Toplevel* c );
    private slots:
        void tick();
    private:
        QMap< Toplevel*, int > windows;
        QTimer timer;
    };

class GrowMove
    : public Effect
    {
    public:
        virtual void windowUserMovedResized( Toplevel* c, bool first, bool last );
        virtual void transformWindow( Toplevel* c, Matrix& m, EffectData& data );
    };

class ShiftWorkspaceUp
    : public QObject, public Effect
    {
    Q_OBJECT
    public:
        ShiftWorkspaceUp( Workspace* ws );
        virtual void transformWorkspace( Matrix& m, EffectData& data );
    private slots:
        void tick();
    private:
        QTimer timer;
        bool up;
        Workspace* wspace;
    };
#endif

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
