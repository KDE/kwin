/*****************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2007 Philip Falkner <philip.falkner@gmail.com>

You can Freely distribute this program under the GNU General Public
License. See the file "COPYING" for the exact licensing terms.
******************************************************************/

#ifndef KWIN_FADE_H
#define KWIN_FADE_H

#include <effects.h>

namespace KWinInternal
{

class FadeEffect
    : public Effect
    {
    public:
        FadeEffect();
        virtual void prePaintWindow( EffectWindow* w, int* mask, QRegion* region, int time );
        virtual void paintWindow( EffectWindow* w, int mask, QRegion region, WindowPaintData& data );
        virtual void postPaintWindow( EffectWindow* w );
        // TODO react also on virtual desktop changes
        virtual void windowOpacityChanged( EffectWindow* c, double old_opacity );
        virtual void windowAdded( EffectWindow* c );
        virtual void windowClosed( EffectWindow* c );
        virtual void windowDeleted( EffectWindow* c );
    private:
        int fade_in_speed, fade_out_speed; // TODO make these configurable
        class WindowInfo;
        QHash< const EffectWindow*, WindowInfo > windows;
    };

class FadeEffect::WindowInfo
    {
    public:
        WindowInfo()
            : current( 0 )
            , target( 0 )
            , step_mult( 0 )
            , deleted( false )
            {};
        bool isFading() const;
        double current;
        double target;
        double step_mult;
        bool deleted;
    };

inline bool FadeEffect::WindowInfo::isFading() const
    {
    return current != target;
    }

} // namespace

#endif
