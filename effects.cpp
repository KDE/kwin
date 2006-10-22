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

void Effect::prePaintScreen( int* mask, QRegion* region )
    {
    effects->nextPrePaintScreen( mask, region );
    }

void Effect::paintScreen( int mask, QRegion region, ScreenPaintData& data )
    {
    effects->nextPaintScreen( mask, region, data );
    }

void Effect::prePaintWindow( Scene::Window* w, int* mask, QRegion* region )
    {
    effects->nextPrePaintWindow( w, mask, region );
    }

void Effect::paintWindow( Scene::Window* w, int mask, QRegion region, WindowPaintData& data )
    {
    effects->nextPaintWindow( w, mask, region, data );
    }

void MakeHalfTransparent::prePaintWindow( Scene::Window* w, int* mask, QRegion* region )
    {
    const Client* c = dynamic_cast< const Client* >( w->window());
    if(( c != NULL && ( c->isMove() || c->isResize())) || w->window()->isDialog())
        {
        *mask |= Scene::PAINT_WINDOW_TRANSLUCENT;
        *mask &= ~Scene::PAINT_WINDOW_OPAQUE;
        }
    effects->nextPrePaintWindow( w, mask, region );
    }

void MakeHalfTransparent::paintWindow( Scene::Window* w, int mask, QRegion region, WindowPaintData& data )
    {
    const Client* c = dynamic_cast< const Client* >( w->window());
    if( w->window()->isDialog())
        data.opacity *= 0.8;
    if( c->isMove() || c->isResize())
        data.opacity *= 0.5;
    effects->nextPaintWindow( w, mask, region, data );
    }

void MakeHalfTransparent::windowUserMovedResized( Toplevel* c, bool first, bool last )
    {
    if( first || last )
        c->addDamage( c->rect());
    }

ShakyMove::ShakyMove()
    {
    connect( &timer, SIGNAL( timeout()), SLOT( tick()));
    }

static const int shaky_diff[] = { 0, 1, 2, 3, 2, 1, 0, -1, -2, -3, -2, -1 };
static const int SHAKY_MAX = sizeof( shaky_diff ) / sizeof( shaky_diff[ 0 ] );

void ShakyMove::prePaintScreen( int* mask, QRegion* region )
    {
    if( !windows.isEmpty())
        *mask |= Scene::PAINT_WINDOW_TRANSFORMED;
    effects->nextPrePaintScreen( mask, region );
    }

void ShakyMove::prePaintWindow( Scene::Window* w, int* mask, QRegion* region )
    {
    if( windows.contains( w->window()))
        *mask |= Scene::PAINT_WINDOW_TRANSFORMED;
    effects->nextPrePaintWindow( w, mask, region );
    }

void ShakyMove::paintWindow( Scene::Window* w, int mask, QRegion region, WindowPaintData& data )
    {
    if( windows.contains( w->window()))
        data.xTranslate += shaky_diff[ windows[ w->window() ]];
    effects->nextPaintWindow( w, mask, region, data );
    }

void ShakyMove::windowUserMovedResized( Toplevel* c, bool first, bool last )
    {
    if( first )
        {
        if( windows.isEmpty())
            timer.start( 50 );
        windows[ c ] = 0;
        }
    else if( last )
        {
        windows.remove( c );
        // just damage whole screen, transformation is involved
        c->workspace()->addDamage( 0, 0, displayWidth(), displayHeight());
        if( windows.isEmpty())
            timer.stop();
        }
    }

void ShakyMove::windowDeleted( Toplevel* c )
    {
    windows.remove( c );
    if( windows.isEmpty())
        timer.stop();
    }

void ShakyMove::tick()
    {
    for( QMap< const Toplevel*, int >::Iterator it = windows.begin();
         it != windows.end();
         ++it )
        {
        if( *it == SHAKY_MAX - 1 )
            *it = 0;
        else
            ++(*it);
        // just damage whole screen, transformation is involved
        it.key()->workspace()->addDamage( 0, 0, displayWidth(), displayHeight());
        }
    }

#if 0
void GrowMove::transformWindow( Toplevel* c, Matrix& matrix, EffectData& )
    {
    if( Client* c2 = dynamic_cast< Client* >( c ))
        if( c2->isMove())
            {
            Matrix m;
            m.m[ 0 ][ 0 ] = 1.2;
            m.m[ 1 ][ 1 ] = 1.4;
            matrix *= m;
            }
    }

void GrowMove::windowUserMovedResized( Toplevel* c, bool first, bool last )
    {
    if( first || last )
        {
        // TODO damage whole screen, transformation is involved
        c->workspace()->addDamage( c->geometry());
        }
    }
#endif

ShiftWorkspaceUp::ShiftWorkspaceUp( Workspace* ws )
    : up( false )
    , wspace( ws )
    {
    connect( &timer, SIGNAL( timeout()), SLOT( tick()));
    timer.start( 2000 );
    }

void ShiftWorkspaceUp::prePaintScreen( int* mask, QRegion* region )
    {
    if( up )
        *mask |= Scene::PAINT_SCREEN_TRANSFORMED;
    effects->nextPrePaintScreen( mask, region );
    }

void ShiftWorkspaceUp::paintScreen( int mask, QRegion region, ScreenPaintData& data )
    {
    if( up )
        data.yTranslate -= 10;
    effects->nextPaintScreen( mask, region, data );
    }

void ShiftWorkspaceUp::tick()
    {
    up = !up;
    wspace->addDamage( 0, 0, displayWidth(), displayHeight());
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
//    effects.append( new MakeHalfTransparent );
//    effects.append( new ShakyMove );
//    effects.append( new GrowMove );
//    effects.append( new ShiftWorkspaceUp( ws ));
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

void EffectsHandler::prePaintScreen( int* mask, QRegion* region, Effect* final )
    {
    assert( current_paint_screen == 0 );
    effects.append( final );
    nextPrePaintScreen( mask, region );
    effects.pop_back();
    }

void EffectsHandler::paintScreen( int mask, QRegion region, ScreenPaintData& data, Effect* final )
    {
    assert( current_paint_screen == 0 );
    effects.append( final );
    nextPaintScreen( mask, region, data );
    effects.pop_back();
    }

void EffectsHandler::prePaintWindow( Scene::Window* w, int* mask, QRegion* region, Effect* final )
    {
    assert( current_paint_window == 0 );
    effects.append( final );
    nextPrePaintWindow( w, mask, region );
    effects.pop_back();
    }

void EffectsHandler::paintWindow( Scene::Window* w, int mask, QRegion region, WindowPaintData& data, Effect* final )
    {
    assert( current_paint_window == 0 );
    effects.append( final );
    nextPaintWindow( w, mask, region, data );
    effects.pop_back();
    }

// the idea is that effects call this function again which calls the next one
void EffectsHandler::nextPrePaintScreen( int* mask, QRegion* region )
    {
    effects[ current_paint_screen++ ]->prePaintScreen( mask, region );
    --current_paint_screen;
    }

void EffectsHandler::nextPaintScreen( int mask, QRegion region, ScreenPaintData& data )
    {
    effects[ current_paint_screen++ ]->paintScreen( mask, region, data );
    --current_paint_screen;
    }

void EffectsHandler::nextPrePaintWindow( Scene::Window* w, int* mask, QRegion* region )
    {
    effects[ current_paint_window++ ]->prePaintWindow( w, mask, region );
    --current_paint_window;
    }

void EffectsHandler::nextPaintWindow( Scene::Window* w, int mask, QRegion region, WindowPaintData& data )
    {
    effects[ current_paint_window++ ]->paintWindow( w, mask, region, data );
    --current_paint_window;
    }

EffectsHandler* effects;

} // namespace

#include "effects.moc"
