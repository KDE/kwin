/*****************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2006 Lubos Lunak <l.lunak@kde.org>

You can Freely distribute this program under the GNU General Public
License. See the file "COPYING" for the exact licensing terms.
******************************************************************/

#ifndef KWIN_SCENE_BASIC_H
#define KWIN_SCENE_BASIC_H

#include "scene.h"

namespace KWinInternal
{

class SceneBasic
    : public Scene
    {
    public:
        SceneBasic( Workspace* ws );
        virtual ~SceneBasic();
        virtual void paint( QRegion damage, ToplevelList windows );
    protected:
        virtual void paintBackground( QRegion region );
        virtual void windowGeometryShapeChanged( Toplevel* );
        virtual void windowOpacityChanged( Toplevel* );
        virtual void windowAdded( Toplevel* );
        virtual void windowDeleted( Toplevel* );
    };

} // namespace

#endif
