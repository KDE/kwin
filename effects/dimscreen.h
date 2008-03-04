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

#ifndef KWIN_DIMSCREEN_H
#define KWIN_DIMSCREEN_H

#include <kwineffects.h>

#include <QTime>

#ifdef KWIN_HAVE_XRENDER_COMPOSITING
#include <X11/extensions/Xrender.h>
#endif

namespace KWin
{

class DimScreenEffect
    : public Effect
    {
    public:
        DimScreenEffect();
        ~DimScreenEffect();

        virtual void prePaintScreen( ScreenPrePaintData& data, int time );
        virtual void paintScreen( int mask, QRegion region, ScreenPaintData& data );
        virtual void postPaintScreen();
        virtual void paintWindow( EffectWindow *w, int mask, QRegion region, WindowPaintData &data );
        virtual void windowActivated( EffectWindow *w );

    private:
        bool mActivated;
        QTime animationTime;
        int animationDuration;
        bool animation;
        bool deactivate;

#ifdef KWIN_HAVE_XRENDER_COMPOSITING
        XRenderPictFormat* alphaFormat;
#endif
    };

} // namespace

#endif
