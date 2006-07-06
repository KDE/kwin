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
        virtual void paint( XserverRegion damage, ToplevelList windows );
        virtual void windowGeometryShapeChanged( Toplevel* );
        virtual void windowOpacityChanged( Toplevel* );
        virtual void windowDeleted( Toplevel* );
        virtual void transformWindowDamage( Toplevel*, XserverRegion ) const;
        virtual void updateTransformation( Toplevel* );
    private:
        void createBuffer();
        void resetWindowData( Toplevel* c );
        Picture windowPicture( Toplevel* c );
        void saveWindowClipRegion( Toplevel* c, XserverRegion r );
        XserverRegion savedWindowClipRegion( Toplevel* c );
        bool isOpaque( Toplevel* c ) const;
        Picture windowAlphaMask( Toplevel* c );
        XserverRegion windowShape( Toplevel* c );
        static void setPictureMatrix( Picture pic, const Matrix& m );
        void cleanup( Toplevel* c );
        XRenderPictFormat* format;
        Picture front;
        Picture buffer;
        struct WindowData
            {
            WindowData();
            void free();
            bool simpleTransformation() const;
            Picture picture;
            XRenderPictFormat* format;
            XserverRegion saved_clip_region;
            Picture alpha;
            double alpha_cached_opacity;
            XserverRegion shape;
            Matrix matrix;
            EffectData effect;
            int phase;
            };
        QMap< Toplevel*, WindowData > window_data;
    };

} // namespace

#endif

#endif
