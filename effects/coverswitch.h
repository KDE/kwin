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

#ifndef KWIN_COVERSWITCH_H
#define KWIN_COVERSWITCH_H

#include <kwineffects.h>
#include <QTime>

namespace KWin
{

class CoverSwitchEffect
    : public Effect
    {
    public:
        CoverSwitchEffect();
        ~CoverSwitchEffect();

        virtual void prePaintScreen( ScreenPrePaintData& data, int time );
        virtual void paintScreen( int mask, QRegion region, ScreenPaintData& data );
        virtual void postPaintScreen();
        virtual void paintWindow( EffectWindow* w, int mask, QRegion region, WindowPaintData& data );
        virtual void tabBoxAdded( int mode );
        virtual void tabBoxClosed();
        virtual void tabBoxUpdated();

    private:
        void paintScene( EffectWindow* frontWindow, QList< EffectWindow* >* leftWindows, QList< EffectWindow* >* rightWindows,
            float timeFactor, bool reflectedWindows = false );
        void paintWindowCover( EffectWindow* w, QRect windowRect, bool reflectedWindow, float opacity = 1.0 );
        void paintFrontWindow( EffectWindow* frontWindow, float timeFactor, int width, int leftWindows, int rightWindows, bool reflectedWindow  );
        void paintWindows( QList< EffectWindow* >* windows, float timeFactor, bool left, bool reflectedWindows, EffectWindow* additionalWindow = NULL );
        bool mActivated;
        float angle;
        bool animateSwitch;
        bool animateStart;
        bool animateStop;
        bool animation;
        bool start;
        bool stop;
        bool forward;
        bool reflection;
        QTime animationTime;
        int animationDuration;
        int selectedWindow;
        int rearrangeWindows;
        bool stopRequested;
        bool startRequested;
    };

} // namespace

#endif
