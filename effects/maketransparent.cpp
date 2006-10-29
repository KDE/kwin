/*****************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2006 Lubos Lunak <l.lunak@kde.org>

You can Freely distribute this program under the GNU General Public
License. See the file "COPYING" for the exact licensing terms.
******************************************************************/

#include "maketransparent.h"

#include <client.h>

namespace KWinInternal
{

void MakeTransparentEffect::prePaintWindow( Scene::Window* w, int* mask, QRegion* region, int time )
    {
    const Client* c = dynamic_cast< const Client* >( w->window());
    if(( c != NULL && ( c->isMove() || c->isResize())) || w->window()->isDialog())
        {
        *mask |= Scene::PAINT_WINDOW_TRANSLUCENT;
        *mask &= ~Scene::PAINT_WINDOW_OPAQUE;
        }
    effects->prePaintWindow( w, mask, region, time );
    }

void MakeTransparentEffect::paintWindow( Scene::Window* w, int mask, QRegion region, WindowPaintData& data )
    {
    const Client* c = dynamic_cast< const Client* >( w->window());
    if( w->window()->isDialog())
        data.opacity *= 0.8;
    if( c->isMove() || c->isResize())
        data.opacity *= 0.5;
    effects->paintWindow( w, mask, region, data );
    }

void MakeTransparentEffect::windowUserMovedResized( Toplevel* c, bool first, bool last )
    {
    if( first || last )
        c->addDamageFull();
    }

} // namespace
