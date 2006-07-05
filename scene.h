/*****************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2006 Lubos Lunak <l.lunak@kde.org>

You can Freely distribute this program under the GNU General Public
License. See the file "COPYING" for the exact licensing terms.
******************************************************************/

#ifndef KWIN_SCENE_H
#define KWIN_SCENE_H

#include "utils.h"

namespace KWinInternal
{

class Workspace;

class Scene
    {
    public:
        Scene( Workspace* ws );
        virtual ~Scene();
        void setWindows( const ToplevelList& list );
        virtual void paint() = 0;
    protected:
        Workspace* wspace;
        ToplevelList windows;
    };

extern Scene* scene;

} // namespace

#endif
