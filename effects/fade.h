/*****************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2007 Philip Falkner <philip.falkner@gmail.com>

You can Freely distribute this program under the GNU General Public
License. See the file "COPYING" for the exact licensing terms.
******************************************************************/

#ifndef KWIN_FADE_H
#define KWIN_FADE_H

#include <kwineffects.h>

namespace KWin
{

class FadeEffect
    : public Effect
    {
    public:
        FadeEffect();
        virtual void prePaintScreen( int* mask, QRegion* region, int time );
        virtual void prePaintWindow( EffectWindow* w, int* mask, QRegion* paint, QRegion* clip, int time );
        virtual void paintWindow( EffectWindow* w, int mask, QRegion region, WindowPaintData& data );

        // TODO react also on virtual desktop changes
        virtual void windowOpacityChanged( EffectWindow* c, double old_opacity );
        virtual void windowAdded( EffectWindow* c );
        virtual void windowClosed( EffectWindow* c );
        virtual void windowDeleted( EffectWindow* c );

        bool isFadeWindow( EffectWindow* w );
    private:
        class WindowInfo;
        QHash< const EffectWindow*, WindowInfo > windows;
        double fadeInStep, fadeOutStep;
        int fadeInTime, fadeOutTime;
        bool fadeWindows;
    };

class FadeEffect::WindowInfo
    {
    public:
        WindowInfo()
            : fadeInStep( 0.0 )
            , fadeOutStep( 0.0 )
            , opacity( 1.0 )
            , saturation( 1.0 )
            , brightness( 1.0 )
            , deleted( false )
            {}
        double fadeInStep, fadeOutStep;
        double opacity;
        float saturation, brightness;
        bool deleted;
    };

} // namespace

#endif
