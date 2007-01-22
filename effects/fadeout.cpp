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

void FadeOutEffect::prePaintWindow( Scene::Window* w, int* mask, QRegion* region, int time )
    {
    if( windows.contains( w->window()))
        {
        windows[ w->window() ] -= time / 1000.; // complete change in 1000ms
        if( windows[ w->window() ] > 0 )
            {
            *mask |= Scene::PAINT_WINDOW_TRANSLUCENT;
            *mask &= ~( Scene::PAINT_WINDOW_OPAQUE | Scene::PAINT_WINDOW_DISABLED );
            }
        else
            {
            windows.remove( w->window());
            static_cast< Deleted* >( w->window())->unrefWindow();
            }
        }
    effects->prePaintWindow( w, mask, region, time );
    }

void FadeOutEffect::paintWindow( Scene::Window* w, int mask, QRegion region, WindowPaintData& data )
    {
    if( windows.contains( w->window()))
        {
        data.opacity *= windows[ w->window()];
        }
    effects->paintWindow( w, mask, region, data );
    }

void FadeOutEffect::postPaintWindow( Scene::Window* w )
    {
    if( windows.contains( w->window()))
        w->window()->addDamageFull(); // trigger next animation repaint
    effects->postPaintWindow( w );
    }

void FadeOutEffect::windowClosed( Toplevel* c, Deleted* d )
    {
    Client* cc = dynamic_cast< Client* >( c );
    if( cc == NULL || cc->isOnCurrentDesktop())
        {
        windows[ d ] = 1; // count down to 0
        d->addDamageFull();
        d->refWindow();
        }
    }

void FadeOutEffect::windowDeleted( Deleted* c )
    {
    windows.remove( c );
    }

} // namespace
