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

void FadeInEffect::prePaintWindow( Scene::Window* w, int* mask, QRegion* region, int time )
    {
    if( windows.contains( w->window()))
        {
        windows[ w->window() ] += time / 1000.; // complete change in 1000ms
        if( windows[ w->window() ] < 1 )
            {
            *mask |= Scene::PAINT_WINDOW_TRANSLUCENT;
            *mask &= ~Scene::PAINT_WINDOW_OPAQUE;
            }
        else
            windows.remove( w->window());
        }
    effects->prePaintWindow( w, mask, region, time );
    }

void FadeInEffect::paintWindow( Scene::Window* w, int mask, QRegion region, WindowPaintData& data )
    {
    if( windows.contains( w->window()))
        {
        data.opacity *= windows[ w->window()];
        }
    effects->paintWindow( w, mask, region, data );
    }

void FadeInEffect::postPaintWindow( Scene::Window* w )
    {
    if( windows.contains( w->window()))
        w->window()->addDamageFull(); // trigger next animation repaint
    effects->postPaintWindow( w );
    }

void FadeInEffect::windowAdded( Toplevel* c )
    {
    Client* cc = dynamic_cast< Client* >( c );
    if( cc == NULL || cc->isOnCurrentDesktop())
        {
        windows[ c ] = 0;
        c->addDamageFull();
        }
    }

void FadeInEffect::windowDeleted( Toplevel* c )
    {
    windows.remove( c );
    }

} // namespace
