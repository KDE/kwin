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

void Effect::paintWindow( Scene::Window* w, int mask, QRegion region, WindowPaintData& data )
    {
    effects->nextPaintWindow( w, mask, region, data );
    }

void Effect::paintScreen( int mask, QRegion region, ScreenPaintData& data )
    {
    effects->nextPaintScreen( mask, region, data );
    }

#if 0
void MakeHalfTransparent::transformWindow( Toplevel* c, Matrix&, EffectData& data )
    {
    if( c->isDialog())
        data.opacity *= 0.8;
    if( Client* c2 = dynamic_cast< Client* >( c ))
        if( c2->isMove() || c2->isResize())
            data.opacity *= 0.5;
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

void ShakyMove::transformWindow( Toplevel* c, Matrix& matrix, EffectData& )
    {
    if( windows.contains( c ))
        {
        Matrix m;
        m.m[ 0 ][ 3 ] = shaky_diff[ windows[ c ]];
        matrix *= m;
        }
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
        // TODO just damage whole screen, transformation is involved
        c->workspace()->addDamage( c->geometry().adjusted( -3, 7, 0, 0 ));
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
    for( QMap< Toplevel*, int >::Iterator it = windows.begin();
         it != windows.end();
         ++it )
        {
        if( *it == SHAKY_MAX - 1 )
            *it = 0;
        else
            ++(*it);
        // TODO damage whole screen, transformation is involved
        it.key()->workspace()->addDamage( it.key()->geometry().adjusted( -1, 2, 0, 0 ));
        }
    }

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

ShiftWorkspaceUp::ShiftWorkspaceUp( Workspace* ws )
    : up( false )
    , wspace( ws )
    {
    connect( &timer, SIGNAL( timeout()), SLOT( tick()));
    timer.start( 2000 );
    }

void ShiftWorkspaceUp::transformWorkspace( Matrix& matrix, EffectData& )
    {
    if( !up )
        return;
    Matrix m;
    m.m[ 1 ][ 3 ] = -10;
    matrix *= m;
    }

void ShiftWorkspaceUp::tick()
    {
    up = !up;
    wspace->addDamage( 0, 0, displayWidth(), displayHeight());
    }

static MakeHalfTransparent* mht;
static ShakyMove* sm;
static GrowMove* gm;
static ShiftWorkspaceUp* swu;
#endif

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

void EffectsHandler::paintWindow( Scene::Window* w, int mask, QRegion region, WindowPaintData& data, Effect* final )
    {
    assert( current_paint_window == 0 );
    effects.append( final );
    nextPaintWindow( w, mask, region, data );
    effects.pop_back();
    }

void EffectsHandler::paintScreen( int mask, QRegion region, ScreenPaintData& data, Effect* final )
    {
    assert( current_paint_screen == 0 );
    effects.append( final );
    nextPaintScreen( mask, region, data );
    effects.pop_back();
    }

void EffectsHandler::nextPaintWindow( Scene::Window* w, int mask, QRegion region, WindowPaintData& data )
    {
    // the idea is that effects call this function again which calls the next one
    effects[ current_paint_window++ ]->paintWindow( w, mask, region, data );
    --current_paint_window;
    }

void EffectsHandler::nextPaintScreen( int mask, QRegion region, ScreenPaintData& data )
    {
    effects[ current_paint_screen++ ]->paintScreen( mask, region, data );
    --current_paint_screen;
    }

EffectsHandler* effects;

} // namespace

#include "effects.moc"
