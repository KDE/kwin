/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2006 Lubos Lunak <l.lunak@kde.org>

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

#ifndef KWIN_MAKETRANSPARENT_H
#define KWIN_MAKETRANSPARENT_H

#include <kwineffects.h>

namespace KWin
{

class MakeTransparentEffect
    : public Effect
    {
    public:
        MakeTransparentEffect();
        virtual void windowUserMovedResized( EffectWindow* c, bool first, bool last );
        virtual void prePaintWindow( EffectWindow* w, WindowPrePaintData& data, int time );
        virtual void paintWindow( EffectWindow* w, int mask, QRegion region, WindowPaintData& data );
        virtual void windowActivated( EffectWindow* w );
    private:
        bool isInactive( const EffectWindow *w ) const;

        double decoration;
        double moveresize;
        double dialogs;
        double inactive;
        EffectWindow* active;
    };

} // namespace

#endif
