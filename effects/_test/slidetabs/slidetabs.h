/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2009 Jorge Mata <matamax123@gmail.com>

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

#ifndef KWIN_SLIDETABS_H
#define KWIN_SLIDETABS_H

#include <kwineffects.h>

namespace KWin
{

class SlideTabsEffect : public Effect
    {
    public:
        SlideTabsEffect();
        virtual void reconfigure( ReconfigureFlags );
        virtual void prePaintWindow( EffectWindow *w, WindowPrePaintData &data, int time );
        virtual void paintWindow( EffectWindow* w, int mask, QRegion region, WindowPaintData& data );
        virtual void postPaintWindow( EffectWindow* w );
        virtual void prePaintScreen( ScreenPrePaintData &data, int time );
        virtual void postPaintScreen();
        virtual void clientGroupItemSwitched( EffectWindow* from, EffectWindow* to );
        virtual void clientGroupItemAdded( EffectWindow* from, EffectWindow* to );

    private:
        QRect calculateNextMove();
        QPoint calculatePointTarget( const QPoint &a, const QPoint &b );
        WindowMotionManager motionManager;
        EffectWindow* inMove;
        EffectWindow* notMoving;
        TimeLine timeLine;
        QRect target, source;
        bool direction, wasD, grouping, switching;
        int totalTime, distance;
    };

}

#endif
