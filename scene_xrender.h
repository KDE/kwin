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

namespace KWinInternal
{

class SceneXrender
    : public Scene
    {
    public:
        SceneXrender( Workspace* ws );
        virtual ~SceneXrender();
        virtual void paint( XserverRegion damage );
    private:
        void createBuffer();
        XRenderPictFormat* format;
        Picture front;
        Picture buffer;
    };

} // namespace

#endif

#endif
