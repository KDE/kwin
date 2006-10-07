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
        void resetWindowData( Toplevel* c );
        static void setPictureMatrix( Picture pic, const Matrix& m );
        XRenderPictFormat* format;
        Picture front;
        Picture buffer;
        class WindowData
            {
            public:
                WindowData( Toplevel* c, XRenderPictFormat* f );
                void free(); // is often copied by value, use manually instead of dtor
                void cleanup(); // removes data needed only during painting pass
                Picture picture();
                bool simpleTransformation() const;
                void saveClipRegion( XserverRegion r );
                XserverRegion savedClipRegion();
                bool isOpaque() const;
                Picture alphaMask();
                XserverRegion shape();
                void geometryShapeChanged();
                void opacityChanged();
                Matrix matrix;
                EffectData effect;
                int phase;
                WindowData() {} // QMap sucks even in Qt4
            private:
                Toplevel* window;
                Picture _picture;
                XRenderPictFormat* format;
                XserverRegion saved_clip_region;
                Picture alpha;
                double alpha_cached_opacity;
                XserverRegion _shape;
            };
        QMap< Toplevel*, WindowData > window_data;
    };

} // namespace

#endif

#endif
