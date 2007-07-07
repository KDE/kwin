/*****************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2006 Lubos Lunak <l.lunak@kde.org>

You can Freely distribute this program under the GNU General Public
License. See the file "COPYING" for the exact licensing terms.
******************************************************************/

#include "maketransparent.h"

namespace KWin
{

KWIN_EFFECT( maketransparent, MakeTransparentEffect )

void MakeTransparentEffect::prePaintWindow( EffectWindow* w, WindowPrePaintData& data, int time )
    {
    if(( w->isUserMove() || w->isUserResize()) || w->isDialog())
        {
        data.mask |= PAINT_WINDOW_TRANSLUCENT;
        data.mask &= ~PAINT_WINDOW_OPAQUE;
        }
    effects->prePaintWindow( w, data, time );
    }

void MakeTransparentEffect::paintWindow( EffectWindow* w, int mask, QRegion region, WindowPaintData& data )
    {
    if( w->isDialog())
        data.opacity *= 0.8;
    if( w->isUserMove() || w->isUserResize())
        data.opacity *= 0.5;
    effects->paintWindow( w, mask, region, data );
    }

void MakeTransparentEffect::windowUserMovedResized( EffectWindow* w, bool first, bool last )
    {
    if( first || last )
        w->addRepaintFull();
    }

} // namespace
