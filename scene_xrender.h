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

#include "scene.h"

#ifdef HAVE_XRENDER
#include <X11/extensions/Xrender.h>

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
    protected:
        virtual void paintBackground( QRegion region );
    private:
        void createBuffer();
        static XserverRegion toXserverRegion( QRegion region );
        XRenderPictFormat* format;
        Picture front;
        static Picture buffer;
        class Window;
        QMap< Toplevel*, Window > windows;
    };

class SceneXrender::Window
    : public Scene::Window
    {
    public:
        Window( Toplevel* c );
        virtual void free();
        virtual void performPaint( QRegion region, int mask );
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

} // namespace

#endif

#endif
