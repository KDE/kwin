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
        // repaints the given screen areas, windows provides the stacking order
        virtual void paint( QRegion damage, ToplevelList windows ) = 0;
        // shape/size of a window changed
        virtual void windowGeometryShapeChanged( Toplevel* );
        // opacity of a window changed
        virtual void windowOpacityChanged( Toplevel* );
        // a new window has been created
        virtual void windowAdded( Toplevel* );
        // a window has been destroyed
        virtual void windowDeleted( Toplevel* );
        // transforms the window areas to screen areas
        virtual void transformWindowDamage( Toplevel*, QRegion& ) const;
        // updates cached window information after an effect changes
        // transformation
        // TODO - remove?
        virtual void updateTransformation( Toplevel* );
    protected:
        Workspace* wspace;
    };

extern Scene* scene;

} // namespace

#endif
