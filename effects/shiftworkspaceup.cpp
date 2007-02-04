/*****************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2006 Lubos Lunak <l.lunak@kde.org>

You can Freely distribute this program under the GNU General Public
License. See the file "COPYING" for the exact licensing terms.
******************************************************************/

#include "shiftworkspaceup.h"

#include <workspace.h>

namespace KWinInternal
{

ShiftWorkspaceUpEffect::ShiftWorkspaceUpEffect()
    : up( false )
    , diff( 0 )
    {
    connect( &timer, SIGNAL( timeout()), SLOT( tick()));
    timer.start( 2000 );
    }

void ShiftWorkspaceUpEffect::prePaintScreen( int* mask, QRegion* region, int time )
    {
    if( up && diff < 1000 )
        diff = qBound( 0, diff + time, 1000 ); // KDE3: note this differs from KCLAMP
    if( !up && diff > 0 )
        diff = qBound( 0, diff - time, 1000 );
    if( diff != 0 )
        *mask |= Scene::PAINT_SCREEN_TRANSFORMED;
    effects->prePaintScreen( mask, region, time );
    }

void ShiftWorkspaceUpEffect::paintScreen( int mask, QRegion region, ScreenPaintData& data )
    {
    if( diff != 0 )
        data.yTranslate -= diff / 100;
    effects->paintScreen( mask, region, data );
    }

void ShiftWorkspaceUpEffect::postPaintScreen()
    {
    if( up ? diff < 1000 : diff > 0 )
        workspace()->addDamageFull(); // trigger next animation repaint
    effects->postPaintScreen();
    }

void ShiftWorkspaceUpEffect::tick()
    {
    up = !up;
    workspace()->addDamageFull();
    }

} // namespace

#include "shiftworkspaceup.moc"
