/*****************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2007 Rivo Laks <rivolaks@hot.ee>

You can Freely distribute this program under the GNU General Public
License. See the file "COPYING" for the exact licensing terms.
******************************************************************/


#include "wavywindows.h"

#include <scene_opengl.h>
#include <workspace.h>
#include <client.h>

#include <math.h>


// Note that currently effects need to be manually enabled in the EffectsHandler
// class constructor (in effects.cpp).

namespace KWinInternal
{

WavyWindowsEffect::WavyWindowsEffect()
    {
    mTimeElapsed = 0.0f;
    }


void WavyWindowsEffect::prePaintScreen( int* mask, QRegion* region, int time )
    {
    // Adjust elapsed time
    mTimeElapsed += (time / 1000.0f);
    // We need to mark the screen as transformed. Otherwise the whole screen
    //  won't be repainted, resulting in artefacts
    *mask |= Scene::PAINT_SCREEN_TRANSFORMED;

    effects->prePaintScreen(mask, region, time);
    }

void WavyWindowsEffect::prePaintWindow( EffectWindow* w, int* mask, QRegion* region, int time )
    {
    // This window will be transformed by the effect
    *mask |= Scene::PAINT_WINDOW_TRANSFORMED;
    // Check if OpenGL compositing is used
    SceneOpenGL::Window* glwin = dynamic_cast< SceneOpenGL::Window *>( w->sceneWindow() );
    if(glwin)
        // Request the window to be divided into cells which are at most 30x30
        //  pixels big
        glwin->requestVertexGrid(30);

    effects->prePaintWindow( w, mask, region, time );
    }

void WavyWindowsEffect::paintWindow( EffectWindow* w, int mask, QRegion region, WindowPaintData& data )
    {
    // Make sure we have OpenGL compositing and the window is vidible and not a
    //  special window
    SceneOpenGL::Window* glwin = dynamic_cast< SceneOpenGL::Window* >( w->sceneWindow() );
    Client* c = qobject_cast< Client* >( w->window() );
    if( glwin && glwin->isVisible() && c && !c->isSpecialWindow() )
        {
        // We have OpenGL compositing and the window has been subdivided
        //  because of our request (in pre-paint pass)
        // Transform all the vertices to create wavy effect
        QVector< SceneOpenGL::Window::Vertex >& vertices = glwin->vertices();
        for(int i = 0; i < vertices.count(); i++)
            {
            vertices[i].pos[0] += sin(mTimeElapsed + vertices[i].texcoord[1] / 60 + 0.5f) * 10;
            vertices[i].pos[1] += sin(mTimeElapsed + vertices[i].texcoord[0] / 80) * 10;
            }
        // We have changed the vertices, so they will have to be reset before
        //  the next paint pass
        glwin->markVerticesDirty();
        }

    // Call the next effect.
    effects->paintWindow( w, mask, region, data );
    }

void WavyWindowsEffect::postPaintScreen()
    {
    // Damage the workspace so that everything would be repainted next time
    workspace()->addDamageFull();

    // Call the next effect.
    effects->postPaintScreen();
    }

} // namespace

