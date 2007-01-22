/*****************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2006 Lubos Lunak <l.lunak@kde.org>

You can Freely distribute this program under the GNU General Public
License. See the file "COPYING" for the exact licensing terms.
******************************************************************/

#include "fadeout.h"

#include <client.h>
#include <deleted.h>

namespace KWinInternal
{

void FadeOutEffect::prePaintWindow( EffectWindow* w, int* mask, QRegion* region, int time )
    {
    if( windows.contains( w ))
        {
        windows[ w  ] -= time / 1000.; // complete change in 1000ms
        if( windows[ w  ] > 0 )
            {
            *mask |= Scene::PAINT_WINDOW_TRANSLUCENT;
            *mask &= ~( Scene::PAINT_WINDOW_OPAQUE | Scene::PAINT_WINDOW_DISABLED );
            }
        else
            {
            windows.remove( w );
            static_cast< Deleted* >( w->window())->unrefWindow();
            }
        }
    effects->prePaintWindow( w, mask, region, time );
    }

void FadeOutEffect::paintWindow( EffectWindow* w, int mask, QRegion region, WindowPaintData& data )
    {
    if( windows.contains( w ))
        {
        data.opacity *= windows[ w ];
        }
    effects->paintWindow( w, mask, region, data );
    }

void FadeOutEffect::postPaintWindow( EffectWindow* w )
    {
    if( windows.contains( w ))
        w->window()->addDamageFull(); // trigger next animation repaint
    effects->postPaintWindow( w );
    }

void FadeOutEffect::windowClosed( EffectWindow* c )
    {
    Client* cc = dynamic_cast< Client* >( c->window());
    if( cc == NULL || cc->isOnCurrentDesktop())
        {
        windows[ c ] = 1; // count down to 0
        c->window()->addDamageFull();
        static_cast< Deleted* >( c->window())->refWindow();
        }
    }

void FadeOutEffect::windowDeleted( EffectWindow* c )
    {
    windows.remove( c );
    }

} // namespace
