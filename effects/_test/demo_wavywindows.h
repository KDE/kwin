/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2007 Rivo Laks <rivolaks@hot.ee>

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

#ifndef DEMO_WAVYWINDOWS_H
#define DEMO_WAVYWINDOWS_H

// Include with base class for effects.
#include <kwineffects.h>

namespace KWin
{

/**
 * Demo effect which applies waves to all windows
 **/
class WavyWindowsEffect
    : public Effect
    {
    public:
        WavyWindowsEffect();

        virtual void prePaintScreen( ScreenPrePaintData& data, int time );
        virtual void prePaintWindow( EffectWindow* w, WindowPrePaintData& data, int time );
        virtual void paintWindow( EffectWindow* w, int mask, QRegion region, WindowPaintData& data );
        virtual void postPaintScreen();

    private:
        double mTimeElapsed;
    };

} // namespace

#endif // DEMO_WAVYWINDOWS_H
