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

MakeTransparentEffect::MakeTransparentEffect()
    {
    // TODO options
    decoration = 0.7;
    moveresize = 0.5;
    dialogs = 1.0;
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
