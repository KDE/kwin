/*****************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2006 Lubos Lunak <l.lunak@kde.org>

You can Freely distribute this program under the GNU General Public
License. See the file "COPYING" for the exact licensing terms.
******************************************************************/

#include "demo_shakymove.h"

namespace KWin
{

KWIN_EFFECT( demo_shakymove, ShakyMoveEffect )

ShakyMoveEffect::ShakyMoveEffect()
    {
    connect( &timer, SIGNAL( timeout()), SLOT( tick()));
    }

static const int shaky_diff[] = { 0, 1, 2, 3, 2, 1, 0, -1, -2, -3, -2, -1 };
static const int SHAKY_MAX = sizeof( shaky_diff ) / sizeof( shaky_diff[ 0 ] );

void ShakyMoveEffect::prePaintScreen( ScreenPrePaintData& data, int time )
    {
    if( !windows.isEmpty())
        data.mask |= PAINT_SCREEN_WITH_TRANSFORMED_WINDOWS;
    effects->prePaintScreen( data, time );
    }

void ShakyMoveEffect::prePaintWindow( EffectWindow* w, WindowPrePaintData& data, int time )
    {
    if( windows.contains( w ))
        data.setTransformed();
    effects->prePaintWindow( w, data, time );
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
        // just repaint whole screen, transformation is involved
        effects->addRepaintFull();
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
    for( QHash< const EffectWindow*, int >::Iterator it = windows.begin();
         it != windows.end();
         ++it )
        {
        if( *it == SHAKY_MAX - 1 )
            *it = 0;
        else
            ++(*it);
        // just repaint whole screen, transformation is involved
        effects->addRepaintFull();
        }
    }

} // namespace

#include "demo_shakymove.moc"
