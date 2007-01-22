/*****************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2006 Lubos Lunak <l.lunak@kde.org>

You can Freely distribute this program under the GNU General Public
License. See the file "COPYING" for the exact licensing terms.
******************************************************************/

#include "shakymove.h"

#include <workspace.h>

namespace KWinInternal
{

ShakyMoveEffect::ShakyMoveEffect()
    {
    connect( &timer, SIGNAL( timeout()), SLOT( tick()));
    }

static const int shaky_diff[] = { 0, 1, 2, 3, 2, 1, 0, -1, -2, -3, -2, -1 };
static const int SHAKY_MAX = sizeof( shaky_diff ) / sizeof( shaky_diff[ 0 ] );

void ShakyMoveEffect::prePaintScreen( int* mask, QRegion* region, int time )
    {
    if( !windows.isEmpty())
        *mask |= Scene::PAINT_WINDOW_TRANSFORMED;
    effects->prePaintScreen( mask, region, time );
    }

void ShakyMoveEffect::prePaintWindow( EffectWindow* w, int* mask, QRegion* region, int time )
    {
    if( windows.contains( w ))
        *mask |= Scene::PAINT_WINDOW_TRANSFORMED;
    effects->prePaintWindow( w, mask, region, time );
    }

void ShakyMoveEffect::paintWindow( EffectWindow* w, int mask, QRegion region, WindowPaintData& data )
    {
    if( windows.contains( w ))
        data.xTranslate += shaky_diff[ windows[ w ]];
    effects->paintWindow( w, mask, region, data );
    }

void ShakyMoveEffect::windowUserMovedResized( EffectWindow* c, bool first, bool last )
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
        c->window()->workspace()->addDamageFull();
        if( windows.isEmpty())
            timer.stop();
        }
    }

void ShakyMoveEffect::windowClosed( EffectWindow* c )
    {
    windows.remove( c );
    if( windows.isEmpty())
        timer.stop();
    }

// TODO use time provided with prePaintWindow() instead
void ShakyMoveEffect::tick()
    {
    for( QMap< const EffectWindow*, int >::Iterator it = windows.begin();
         it != windows.end();
         ++it )
        {
        if( *it == SHAKY_MAX - 1 )
            *it = 0;
        else
            ++(*it);
        // just damage whole screen, transformation is involved
        it.key()->window()->workspace()->addDamageFull();
        }
    }

} // namespace

#include "shakymove.moc"
