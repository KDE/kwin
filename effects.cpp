/*****************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2006 Lubos Lunak <l.lunak@kde.org>

You can Freely distribute this program under the GNU General Public
License. See the file "COPYING" for the exact licensing terms.
******************************************************************/

#include "effects.h"

#include "toplevel.h"
#include "client.h"
#include "scene.h"

#include "effects/dialogparent.h"
#include "effects/fadein.h"
#include "effects/howto.h"
#include "effects/maketransparent.h"
#include "effects/scalein.h"
#include "effects/shakymove.h"
#include "effects/shiftworkspaceup.h"
#include "effects/showfps.h"

namespace KWinInternal
{

//****************************************
// Effect
//****************************************

Effect::~Effect()
    {
    }

void Effect::windowUserMovedResized( Toplevel* , bool, bool )
    {
    }

void Effect::windowAdded( Toplevel* )
    {
    }

void Effect::windowDeleted( Toplevel* )
    {
    }

void Effect::windowActivated( Toplevel* )
    {
    }

void Effect::prePaintScreen( int* mask, QRegion* region, int time )
    {
    effects->prePaintScreen( mask, region, time );
    }

void Effect::paintScreen( int mask, QRegion region, ScreenPaintData& data )
    {
    effects->paintScreen( mask, region, data );
    }
    
void Effect::postPaintScreen()
    {
    effects->postPaintScreen();
    }

void Effect::prePaintWindow( Scene::Window* w, int* mask, QRegion* region, int time )
    {
    effects->prePaintWindow( w, mask, region, time );
    }

void Effect::paintWindow( Scene::Window* w, int mask, QRegion region, WindowPaintData& data )
    {
    effects->paintWindow( w, mask, region, data );
    }

void Effect::postPaintWindow( Scene::Window* w )
    {
    effects->postPaintWindow( w );
    }

//****************************************
// EffectsHandler
//****************************************

EffectsHandler::EffectsHandler( Workspace* ws )
    : current_paint_window( 0 )
    , current_paint_screen( 0 )
    {
    if( !compositing())
        return;
    effects.append( new ShowFpsEffect( ws ));
//    effects.append( new HowtoEffect );
//    effects.append( new MakeTransparentEffect );
//    effects.append( new ShakyMoveEffect );
//    effects.append( new ShiftWorkspaceUpEffect( ws ));
//    effects.append( new FadeInEffect );
//    effects.append( new ScaleInEffect );
//    effects.append( new DialogParentEffect );
    }

EffectsHandler::~EffectsHandler()
    {
    foreach( Effect* e, effects )
        delete e;
    }

void EffectsHandler::windowUserMovedResized( Toplevel* c, bool first, bool last )
    {
    foreach( Effect* e, effects )
        e->windowUserMovedResized( c, first, last );
    }

void EffectsHandler::windowAdded( Toplevel* c )
    {
    foreach( Effect* e, effects )
        e->windowAdded( c );
    }

void EffectsHandler::windowDeleted( Toplevel* c )
    {
    foreach( Effect* e, effects )
        e->windowDeleted( c );
    }

void EffectsHandler::windowActivated( Toplevel* c )
    {
    foreach( Effect* e, effects )
        e->windowActivated( c );
    }

// start another painting pass
void EffectsHandler::startPaint()
    {
    assert( current_paint_screen == 0 );
    assert( current_paint_window == 0 );
    }

// the idea is that effects call this function again which calls the next one
void EffectsHandler::prePaintScreen( int* mask, QRegion* region, int time )
    {
    if( current_paint_screen < effects.size())
        {
        effects[ current_paint_screen++ ]->prePaintScreen( mask, region, time );
        --current_paint_screen;
        }
    // no special final code
    }

void EffectsHandler::paintScreen( int mask, QRegion region, ScreenPaintData& data )
    {
    if( current_paint_screen < effects.size())
        {
        effects[ current_paint_screen++ ]->paintScreen( mask, region, data );
        --current_paint_screen;
        }
    else
        scene->finalPaintScreen( mask, region, data );
    }

void EffectsHandler::postPaintScreen()
    {
    if( current_paint_screen < effects.size())
        {
        effects[ current_paint_screen++ ]->postPaintScreen();
        --current_paint_screen;
        }
    // no special final code
    }

void EffectsHandler::prePaintWindow( Scene::Window* w, int* mask, QRegion* region, int time )
    {
    if( current_paint_window < effects.size())
        {
        effects[ current_paint_window++ ]->prePaintWindow( w, mask, region, time );
        --current_paint_window;
        }
    // no special final code
    }

void EffectsHandler::paintWindow( Scene::Window* w, int mask, QRegion region, WindowPaintData& data )
    {
    if( current_paint_window < effects.size())
        {
        effects[ current_paint_window++ ]->paintWindow( w, mask, region, data );
        --current_paint_window;
        }
    else
        scene->finalPaintWindow( w, mask, region, data );
    }

void EffectsHandler::postPaintWindow( Scene::Window* w )
    {
    if( current_paint_window < effects.size())
        {
        effects[ current_paint_window++ ]->postPaintWindow( w );
        --current_paint_window;
        }
    // no special final code
    }

EffectsHandler* effects;

} // namespace
