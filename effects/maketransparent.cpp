/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2006 Lubos Lunak <l.lunak@kde.org>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*********************************************************************/

#include "maketransparent.h"

#include <kconfiggroup.h>

namespace KWin
{

KWIN_EFFECT( maketransparent, MakeTransparentEffect )

MakeTransparentEffect::MakeTransparentEffect()
    {
    KConfigGroup conf = effects->effectConfig("MakeTransparent");
    decoration = conf.readEntry( "Decoration", 0.7 );
    moveresize = conf.readEntry( "MoveResize", 0.8 );
    dialogs = conf.readEntry( "Dialogs", 1.0 );
    }

void MakeTransparentEffect::prePaintWindow( EffectWindow* w, WindowPrePaintData& data, int time )
    {
    if( decoration != 1.0 && w->hasDecoration())
        {
        data.mask |= PAINT_WINDOW_TRANSLUCENT;
        // don't clear PAINT_WINDOW_OPAQUE, contents are not affected
        data.clip &= w->contentsRect().translated( w->pos()); // decoration cannot clip
        }
    if(( moveresize != 1.0 && ( w->isUserMove() || w->isUserResize()))
        || ( dialogs != 1.0 && w->isDialog()))
        {
        data.setTranslucent();
        }
    effects->prePaintWindow( w, data, time );
    }

void MakeTransparentEffect::paintWindow( EffectWindow* w, int mask, QRegion region, WindowPaintData& data )
    {
    if( decoration != 1.0 && w->hasDecoration())
        data.decoration_opacity *= decoration;
    if( dialogs != 1.0 && w->isDialog())
        data.opacity *= dialogs;
    if( moveresize != 1.0 && ( w->isUserMove() || w->isUserResize()))
        data.opacity *= moveresize;
    effects->paintWindow( w, mask, region, data );
    }

void MakeTransparentEffect::windowUserMovedResized( EffectWindow* w, bool first, bool last )
    {
    if( moveresize != 1.0 && ( first || last ))
        w->addRepaintFull();
    }

} // namespace
