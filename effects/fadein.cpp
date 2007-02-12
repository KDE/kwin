/*****************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2006 Lubos Lunak <l.lunak@kde.org>

You can Freely distribute this program under the GNU General Public
License. See the file "COPYING" for the exact licensing terms.
******************************************************************/

#include "fadein.h"

#include <client.h>

namespace KWinInternal
{

void FadeInEffect::prePaintWindow( EffectWindow* w, int* mask, QRegion* region, int time )
    {
    if( windows.contains( w ))
        {
        windows[ w ] += time / 1000.; // complete change in 1000ms
        if( windows[ w ] < 1 )
            {
            *mask |= Scene::PAINT_WINDOW_TRANSLUCENT;
            *mask &= ~Scene::PAINT_WINDOW_OPAQUE;
            }
        else
            windows.remove( w );
        }
    effects->prePaintWindow( w, mask, region, time );
    }

void FadeInEffect::paintWindow( EffectWindow* w, int mask, QRegion region, WindowPaintData& data )
    {
    if( windows.contains( w ))
        {
        data.opacity *= windows[ w ];
        }
    effects->paintWindow( w, mask, region, data );
    }

void FadeInEffect::postPaintWindow( EffectWindow* w )
    {
    if( windows.contains( w ))
        w->window()->addRepaintFull(); // trigger next animation repaint
    effects->postPaintWindow( w );
    }

void FadeInEffect::windowAdded( EffectWindow* c )
    {
    Client* cc = dynamic_cast< Client* >( c->window());
    if( cc == NULL || cc->isOnCurrentDesktop())
        {
        windows[ c ] = 0;
        c->window()->addRepaintFull();
        }
    }

void FadeInEffect::windowClosed( EffectWindow* c )
    {
    windows.remove( c );
    }

} // namespace
