/*****************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2006 Lubos Lunak <l.lunak@kde.org>

You can Freely distribute this program under the GNU General Public
License. See the file "COPYING" for the exact licensing terms.
******************************************************************/

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
    if( windows.contains( w ))
        {
        data.xScale *= windows[ w ];
        data.yScale *= windows[ w ];
        data.xTranslate += int( w->width() / 2 * ( 1 - windows[ w ] ));
        data.yTranslate += int( w->height() / 2 * ( 1 - windows[ w ] ));
        }
    effects->paintWindow( w, mask, region, data );
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
