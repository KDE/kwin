/*****************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2006 Lubos Lunak <l.lunak@kde.org>

You can Freely distribute this program under the GNU General Public
License. See the file "COPYING" for the exact licensing terms.
******************************************************************/

#ifndef KWIN_DESKTOPCHANGESLIDE_H
#define KWIN_DESKTOPCHANGESLIDE_H

#include <kwineffects.h>

namespace KWin
{

class DesktopChangeSlideEffect
    : public Effect
    {
    public:
        DesktopChangeSlideEffect();
        virtual void prePaintScreen( int* mask, QRegion* region, int time );
        virtual void paintScreen( int mask, QRegion region, ScreenPaintData& data );
        virtual void postPaintScreen();
        virtual void prePaintWindow( EffectWindow* w, int* mask, QRegion* paint, QRegion* clip, int time );
        virtual void paintWindow( EffectWindow* w, int mask, QRegion region, WindowPaintData& data );
        virtual void desktopChanged( int old );
    private:
        QPoint desktopPos( int desktop );
        QPoint startPos;
        int progress;
        int painting_desktop;
        bool painting_sticky;
        QPoint painting_diff;
    };

} // namespace

#endif
