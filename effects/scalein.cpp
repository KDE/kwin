/*****************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2006 Lubos Lunak <l.lunak@kde.org>

You can Freely distribute this program under the GNU General Public
License. See the file "COPYING" for the exact licensing terms.
******************************************************************/

#include "scalein.h"

#include <client.h>

namespace KWinInternal
{

void ScaleInEffect::prePaintScreen( int* mask, QRegion* region, int time )
    {
    if( !windows.isEmpty())
        *mask |= Scene::PAINT_SCREEN_WITH_TRANSFORMED_WINDOWS;
    effects->prePaintScreen( mask, region, time );
    }

void ScaleInEffect::prePaintWindow( EffectWindow* w, int* mask, QRegion* region, int time )
    {
    if( windows.contains( w ))
        {
        windows[ w ] += time / 500.; // complete change in 500ms
        if( windows[ w ] < 1 )
            *mask |= Scene::PAINT_WINDOW_TRANSFORMED;
        else
            windows.remove( w );
        }
    effects->prePaintWindow( w, mask, region, time );
    }

void ScaleInEffect::paintWindow( EffectWindow* w, int mask, QRegion region, WindowPaintData& data )
    {
    if( windows.contains( w ))
        {
        data.xScale *= windows[ w ];
        data.yScale *= windows[ w ];
        data.xTranslate += int( w->window()->width() / 2 * ( 1 - windows[ w ] ));
        data.yTranslate += int( w->window()->height() / 2 * ( 1 - windows[ w ] ));
        }
    effects->paintWindow( w, mask, region, data );
    }

void ScaleInEffect::postPaintWindow( EffectWindow* w )
    {
    if( windows.contains( w ))
        w->window()->addRepaintFull(); // trigger next animation repaint
    effects->postPaintWindow( w );
    }

void ScaleInEffect::windowAdded( EffectWindow* c )
    {
    Client* cc = dynamic_cast< Client* >( c->window());
    if( cc == NULL || cc->isOnCurrentDesktop())
        {
        windows[ c ] = 0;
        c->window()->addRepaintFull();
        }
    }

void ScaleInEffect::windowClosed( EffectWindow* c )
    {
    windows.remove( c );
    }

} // namespace
