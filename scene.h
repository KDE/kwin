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
        virtual void paint( QRegion damage, ToplevelList windows ) = 0;
        virtual void windowGeometryShapeChanged( Toplevel* );
        virtual void windowOpacityChanged( Toplevel* );
        virtual void windowAdded( Toplevel* );
        virtual void windowDeleted( Toplevel* );
        virtual void transformWindowDamage( Toplevel*, QRegion& ) const;
        virtual void updateTransformation( Toplevel* );
    protected:
        Workspace* wspace;
    };

extern Scene* scene;

} // namespace

#endif
