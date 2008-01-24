/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

 Copyright (C) 2008 Martin Gräßlin <ubuntu@martin-graesslin.com

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

#ifndef KWIN_FLIPSWITCH_H
#define KWIN_FLIPSWITCH_H

#include <kwineffects.h>
#include <QTime>

namespace KWin
{

class FlipSwitchEffect
    : public Effect
    {
    public:
        FlipSwitchEffect();
        ~FlipSwitchEffect();

        virtual void prePaintScreen( ScreenPrePaintData& data, int time );
        virtual void paintScreen( int mask, QRegion region, ScreenPaintData& data );
        virtual void postPaintScreen();
        virtual void paintWindow( EffectWindow* w, int mask, QRegion region, WindowPaintData& data );
        virtual void tabBoxAdded( int mode );
        virtual void tabBoxClosed();
        virtual void tabBoxUpdated();
    private:
        void paintWindowFlip( EffectWindow* w, bool draw = true, float opacity = 0.8  );
        bool mActivated;
        bool mAnimation;
        int mFlipDuration;
        bool animateFlip;
        bool forward;
        QTime animationTime;
        int selectedWindow;
        bool start;
        bool stop;
        bool addFullRepaint;
        int rearrangeWindows;
        bool stopRequested;
        bool startRequested;
    };

} // namespace

#endif
