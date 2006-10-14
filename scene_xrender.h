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
        void paintGenericScreen( ToplevelList windows );
        void paintSimpleScreen( QRegion damage, ToplevelList windows );
        void paintBackground( XserverRegion region );
        enum
            {
            PAINT_OPAQUE = 1 << 0,
            PAINT_TRANSLUCENT = 1 << 1
            };
        XRenderPictFormat* format;
        Picture front;
        static Picture buffer;
        class Window;
        static XserverRegion infiniteRegion;
        QMap< Toplevel*, Window > windows;
        struct Phase2Data
            {
            Phase2Data( Window* w, XserverRegion r ) : window( w ), region( r ) {}
            Window* window;
            XserverRegion region;
            };
    };

class SceneXrender::Window
    {
    public:
        Window( Toplevel* c );
        void free(); // is often copied by value, use manually instead of dtor
        void paint( XserverRegion region, int mask );
        bool isOpaque() const;
        void geometryShapeChanged();
        void opacityChanged();
        bool isVisible() const;
        XserverRegion shape();
        void discardPicture();
        void discardShape();
        void discardAlpha();
        Window() {} // QMap sucks even in Qt4
    private:
        Picture picture();
        Picture alphaMask();
        Toplevel* toplevel;
        Picture _picture;
        XRenderPictFormat* format;
        Picture alpha;
        double alpha_cached_opacity;
        XserverRegion _shape;
    };

} // namespace

#endif

#endif
