/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

 Copyright (C) 2009 Martin Gräßlin <kde@martin-graesslin.com>

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

#ifndef KWIN_CUBESLIDE_H
#define KWIN_CUBESLIDE_H

#include <kwineffects.h>
#include <kwinglutils.h>
#include <QQueue>
#include <QSet>

namespace KWin
{
class CubeSlideEffect
    : public Effect
    {
    public:
        CubeSlideEffect();
        ~CubeSlideEffect();
        virtual void reconfigure( ReconfigureFlags );
        virtual void prePaintScreen( ScreenPrePaintData& data, int time );
        virtual void paintScreen( int mask, QRegion region, ScreenPaintData& data );
        virtual void postPaintScreen();
        virtual void prePaintWindow( EffectWindow* w, WindowPrePaintData& data, int time );
        virtual void paintWindow( EffectWindow* w, int mask, QRegion region, WindowPaintData& data );
        virtual void desktopChanged( int old );

        static bool supported();
    private:
        enum RotationDirection
            {
            Left,
            Right,
            Upwards,
            Downwards
            };
        void paintSlideCube( int mask, QRegion region, ScreenPaintData& data );
        bool cube_painting;
        int front_desktop;
        int painting_desktop;
        int other_desktop;
        bool firstDesktop;
        TimeLine timeLine;
        QQueue<RotationDirection> slideRotations;
        QSet<EffectWindow*> panels;
        QSet<EffectWindow*> stickyWindows;
        bool dontSlidePanels;
        bool dontSlideStickyWindows;
    };
}

#endif
