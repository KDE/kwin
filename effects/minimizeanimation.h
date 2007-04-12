/*****************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2007 Rivo Laks <rivolaks@hot.ee>

You can Freely distribute this program under the GNU General Public
License. See the file "COPYING" for the exact licensing terms.
******************************************************************/

#ifndef KWIN_MINIMIZEANIMATION_H
#define KWIN_MINIMIZEANIMATION_H

// Include with base class for effects.
#include <kwineffects.h>


namespace KWin
{

/**
 * Animates minimize/unminimize
 **/
class MinimizeAnimationEffect
    : public Effect
    {
    public:
        MinimizeAnimationEffect();

        virtual void prePaintScreen( int* mask, QRegion* region, int time );
        virtual void prePaintWindow( EffectWindow* w, int* mask, QRegion* paint, QRegion* clip, int time );
        virtual void paintWindow( EffectWindow* w, int mask, QRegion region, WindowPaintData& data );
        virtual void postPaintScreen();

        virtual void windowMinimized( EffectWindow* c );
        virtual void windowUnminimized( EffectWindow* c );

    private:
        QHash< EffectWindow*, float > mAnimationProgress;
        int mActiveAnimations;
    };

} // namespace

#endif
