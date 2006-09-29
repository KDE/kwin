/*****************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2006 Lubos Lunak <l.lunak@kde.org>

You can Freely distribute this program under the GNU General Public
License. See the file "COPYING" for the exact licensing terms.
******************************************************************/

#include "scene_basic.h"

namespace KWinInternal
{

//****************************************
// Scene
//****************************************

Scene::Scene( Workspace* ws )
    : wspace( ws )
    {
    }
    
Scene::~Scene()
    {
    }
    
void Scene::windowGeometryShapeChanged( Toplevel* )
    {
    }

void Scene::windowOpacityChanged( Toplevel* )
    {
    }

void Scene::windowAdded( Toplevel* )
    {
    }

void Scene::windowDeleted( Toplevel* )
    {
    }

void Scene::transformWindowDamage( Toplevel*, XserverRegion ) const
    {
    }

void Scene::updateTransformation( Toplevel* )
    {
    }

Scene* scene;

} // namespace
