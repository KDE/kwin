/*****************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2006 Lubos Lunak <l.lunak@kde.org>

You can Freely distribute this program under the GNU General Public
License. See the file "COPYING" for the exact licensing terms.
******************************************************************/

#ifndef KWIN_MAKETRANSPARENT_H
#define KWIN_MAKETRANSPARENT_H

#include <kwineffects.h>

namespace KWin
{

class MakeTransparentEffect
    : public Effect
    {
    public:
        virtual void windowUserMovedResized( EffectWindow* c, bool first, bool last );
        virtual void prePaintWindow( EffectWindow* w, int* mask, QRegion* paint, QRegion* clip, int time );
        virtual void paintWindow( EffectWindow* w, int mask, QRegion region, WindowPaintData& data );
    };

} // namespace

#endif
