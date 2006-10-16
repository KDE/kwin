/*****************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2006 Lubos Lunak <l.lunak@kde.org>

You can Freely distribute this program under the GNU General Public
License. See the file "COPYING" for the exact licensing terms.
******************************************************************/

#ifndef KWIN_SCENE_XRENDER_H
#define KWIN_SCENE_XRENDER_H

#include "config.h"

#ifdef HAVE_XRENDER
#include <X11/extensions/Xrender.h>

#include "scene.h"
#include "effects.h"

namespace KWinInternal
{

class Matrix;

class SceneXrender
    : public Scene
    {
    public:
        SceneXrender( Workspace* ws );
        virtual ~SceneXrender();
        virtual void paint( QRegion damage, ToplevelList windows );
        virtual void windowGeometryShapeChanged( Toplevel* );
        virtual void windowOpacityChanged( Toplevel* );
        virtual void windowAdded( Toplevel* );
        virtual void windowDeleted( Toplevel* );
    private:
        void createBuffer();
        void paintGenericScreen();
        void paintSimpleScreen( QRegion region );
        void paintBackground( QRegion region );
        static XserverRegion toXserverRegion( QRegion region );
        XRenderPictFormat* format;
        Picture front;
        static Picture buffer;
        class Window;
        QMap< Toplevel*, Window > windows;
        QVector< Window* > stacking_order;
        typedef Scene::Phase2Data< Window > Phase2Data;
        class WrapperEffect;
    };

class SceneXrender::Window
    : public Scene::Window
    {
    public:
        Window( Toplevel* c );
        void free(); // is often copied by value, use manually instead of dtor
        void paint( QRegion region, int mask );
        void discardPicture();
        void discardAlpha();
        Window() {} // QMap sucks even in Qt4
    private:
        Picture picture();
        Picture alphaMask();
        Picture _picture;
        XRenderPictFormat* format;
        Picture alpha;
        double alpha_cached_opacity;
    };

// a special effect that is last in the order that'll actually call the painting functions
class SceneXrender::WrapperEffect
    : public Effect
    {
    public:
        virtual void paintWindow( Scene::Window* w, int mask, QRegion region, WindowPaintData& data );
        virtual void paintScreen( int mask, QRegion region, ScreenPaintData& data );
    };

} // namespace

#endif

#endif
