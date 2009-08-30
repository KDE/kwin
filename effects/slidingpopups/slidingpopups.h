/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2009 Marco Martin notmart@gmail.com

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

#ifndef KWIN_SLIDEBACK_H
#define KWIN_SLIDEBACK_H

// Include with base class for effects.
#include <kwineffects.h>

namespace KWin
{

class SlidingPopupsEffect
    : public Effect
    {
    public:
        SlidingPopupsEffect();
        ~SlidingPopupsEffect();
        virtual void prePaintScreen( ScreenPrePaintData& data, int time );
        virtual void prePaintWindow( EffectWindow* w, WindowPrePaintData& data, int time );
        virtual void paintWindow( EffectWindow* w, int mask, QRegion region, WindowPaintData& data );
        virtual void postPaintWindow( EffectWindow* w );
        // TODO react also on virtual desktop changes
        virtual void windowAdded( EffectWindow* c );
        virtual void windowClosed( EffectWindow* c );
        virtual void windowDeleted( EffectWindow* c );
        virtual void propertyNotify( EffectWindow* w, long a );
    private:
        enum Position
            {
            West = 0,
            North = 1,
            East = 2,
            South = 3
            };
        struct Data
            {
            int start; //point in screen coordinates where the window starts
                       //to animate, from decides if this point is an x or an y
            Position from;
            };
        long mAtom;
        QHash< const EffectWindow*, TimeLine > mAppearingWindows;
        QHash< const EffectWindow*, TimeLine > mDisappearingWindows;
        QHash< const EffectWindow*, Data > mWindowsData;
	int mFadeInTime;
	int mFadeOutTime;
    };

} // namespace

#endif
