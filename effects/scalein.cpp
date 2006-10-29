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

void ScaleInEffect::prePaintWindow( Scene::Window* w, int* mask, QRegion* region, int time )
    {
    if( windows.contains( w->window()))
        {
        windows[ w->window() ] += time / 500.; // complete change in 500ms
        if( windows[ w->window() ] < 1 )
            *mask |= Scene::PAINT_WINDOW_TRANSFORMED;
        else
            windows.remove( w->window());
        }
    effects->prePaintWindow( w, mask, region, time );
    }

void ScaleInEffect::paintWindow( Scene::Window* w, int mask, QRegion region, WindowPaintData& data )
    {
    if( windows.contains( w->window()))
        {
        data.xScale *= windows[ w->window()];
        data.yScale *= windows[ w->window()];
        data.xTranslate += int( w->window()->width() / 2 * ( 1 - windows[ w->window()] ));
        data.yTranslate += int( w->window()->height() / 2 * ( 1 - windows[ w->window()] ));
        }
    effects->paintWindow( w, mask, region, data );
    }

void ScaleInEffect::postPaintWindow( Scene::Window* w )
    {
    if( windows.contains( w->window()))
        w->window()->addDamageFull(); // trigger next animation repaint
    effects->postPaintWindow( w );
    }

void ScaleInEffect::windowAdded( Toplevel* c )
    {
    Client* cc = dynamic_cast< Client* >( c );
    if( cc == NULL || cc->isOnCurrentDesktop())
        {
        windows[ c ] = 0;
        c->addDamageFull();
        }
    }

void ScaleInEffect::windowDeleted( Toplevel* c )
    {
    windows.remove( c );
    }

} // namespace
