/*****************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2007 Lubos Lunak <l.lunak@kde.org>

You can Freely distribute this program under the GNU General Public
License. See the file "COPYING" for the exact licensing terms.
******************************************************************/

#ifndef KWIN_SHADOW_H
#define KWIN_SHADOW_H

#include <effects.h>

namespace KWin
{

class ShadowEffect
    : public Effect
    {
    public:
        ShadowEffect();
        virtual void prePaintWindow( EffectWindow* w, int* mask, QRegion* paint, QRegion* clip, int time );
        virtual void paintWindow( EffectWindow* w, int mask, QRegion region, WindowPaintData& data );
        virtual void postPaintWindow( EffectWindow* w );
        virtual QRect transformWindowDamage( EffectWindow* w, const QRect& r );
    private:
        void drawShadow( EffectWindow* w, int mask, QRegion region, WindowPaintData& data );
        int shadowXOffset, shadowYOffset;
    };

} // namespace

#endif
