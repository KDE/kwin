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

#include "scalein.h"

namespace KWin
{

KWIN_EFFECT( scalein, ScaleInEffect )

void ScaleInEffect::prePaintScreen( ScreenPrePaintData& data, int time )
    {
    if( !windows.isEmpty())
        data.mask |= PAINT_SCREEN_WITH_TRANSFORMED_WINDOWS;
    effects->prePaintScreen( data, time );
    }

void ScaleInEffect::prePaintWindow( EffectWindow* w, WindowPrePaintData& data, int time )
    {
    if( windows.contains( w ))
        {
        windows[ w ] += time / 300.; // complete change in 300ms
        if( windows[ w ] < 1 )
            data.setTransformed();
        else
            windows.remove( w );
        }
    effects->prePaintWindow( w, data, time );
    }

void ScaleInEffect::paintWindow( EffectWindow* w, int mask, QRegion region, WindowPaintData& data )
    {
    if( windows.contains( w ) && isScaleWindow( w ) )
        {
        data.xScale *= windows[ w ];
        data.yScale *= windows[ w ];
        data.xTranslate += int( w->width() / 2 * ( 1 - windows[ w ] ));
        data.yTranslate += int( w->height() / 2 * ( 1 - windows[ w ] ));
        }
    effects->paintWindow( w, mask, region, data );
    }

bool ScaleInEffect::isScaleWindow( EffectWindow* w )
    {
    // TODO: isSpecialWindow is rather generic, maybe tell windowtypes separately?
    if ( w->isPopupMenu() || w->isSpecialWindow() )
        return false;
    return true;
    }

void ScaleInEffect::postPaintWindow( EffectWindow* w )
    {
    if( windows.contains( w ))
        w->addRepaintFull(); // trigger next animation repaint
    effects->postPaintWindow( w );
    }

void ScaleInEffect::windowAdded( EffectWindow* c )
    {
    if( c->isOnCurrentDesktop())
        {
        windows[ c ] = 0;
        c->addRepaintFull();
        }
    }

void ScaleInEffect::windowClosed( EffectWindow* c )
    {
    windows.remove( c );
    }

} // namespace
