/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2006 Lubos Lunak <l.lunak@kde.org>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*********************************************************************/

#ifndef KWIN_SCENE_BASIC_H
#define KWIN_SCENE_BASIC_H

#include "scene.h"

namespace KWin
{

class SceneBasic
    : public Scene
    {
    public:
        SceneBasic( Workspace* ws );
        virtual ~SceneBasic();
        virtual bool initFailed() const;
        virtual CompositingType compositingType() const { return NoCompositing; }
        virtual void paint( QRegion damage, ToplevelList windows );
    protected:
        virtual void paintBackground( QRegion region );
        virtual void windowGeometryShapeChanged( Toplevel* );
        virtual void windowOpacityChanged( Toplevel* );
        virtual void windowAdded( Toplevel* );
        virtual void windowClosed( Toplevel*, Deleted* );
        virtual void windowDeleted( Deleted* );
    };

} // namespace

#endif
