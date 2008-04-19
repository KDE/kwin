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

#include "minimizeanimation.h"

namespace KWin
{

KWIN_EFFECT( minimizeanimation, MinimizeAnimationEffect )

MinimizeAnimationEffect::MinimizeAnimationEffect()
    {
    mActiveAnimations = 0;
    }


void MinimizeAnimationEffect::prePaintScreen( ScreenPrePaintData& data, int time )
    {
    mActiveAnimations = mTimeLine.count();
    if( mActiveAnimations > 0 )
        // We need to mark the screen windows as transformed. Otherwise the
        //  whole screen won't be repainted, resulting in artefacts
        data.mask |= PAINT_SCREEN_WITH_TRANSFORMED_WINDOWS;

    effects->prePaintScreen(data, time);
    }

void MinimizeAnimationEffect::prePaintWindow( EffectWindow* w, WindowPrePaintData& data, int time )
    {
    if( mTimeLine.contains( w ))
        {
        if( w->isMinimized() )
            {
            mTimeLine[w].addTime(time);
            if( mTimeLine[w].progress() >= 1.0f )
                mTimeLine.remove( w );
            }
        else
            {
            mTimeLine[w].removeTime(time);
            if( mTimeLine[w].progress() <= 0.0f )
                mTimeLine.remove( w );
            }

        // Schedule window for transformation if the animation is still in
        //  progress
        if( mTimeLine.contains( w ))
            {
            // We'll transform this window
            data.setTransformed();
            w->enablePainting( EffectWindow::PAINT_DISABLED_BY_MINIMIZE );
            }
        }

    effects->prePaintWindow( w, data, time );
    }

void MinimizeAnimationEffect::paintWindow( EffectWindow* w, int mask, QRegion region, WindowPaintData& data )
    {
    if( mTimeLine.contains( w ))
    {
        // 0 = not minimized, 1 = fully minimized
        double progress = mTimeLine[w].value();

        QRect geo = w->geometry();
        QRect icon = w->iconGeometry();
        // If there's no icon geometry, minimize to the center of the screen
        if( !icon.isValid() )
            icon = QRect( displayWidth() / 2, displayHeight() / 2, 0, 0 );

        data.xScale *= interpolate(1.0, icon.width() / (double)geo.width(), progress);
        data.yScale *= interpolate(1.0, icon.height() / (double)geo.height(), progress);
        data.xTranslate = (int)interpolate(data.xTranslate, icon.x() - geo.x(), progress);
        data.yTranslate = (int)interpolate(data.yTranslate, icon.y() - geo.y(), progress);
    }

    // Call the next effect.
    effects->paintWindow( w, mask, region, data );
    }

void MinimizeAnimationEffect::postPaintScreen()
    {
    if( mActiveAnimations > 0 )
        // Repaint the workspace so that everything would be repainted next time
        effects->addRepaintFull();
    mActiveAnimations = mTimeLine.count();

    // Call the next effect.
    effects->postPaintScreen();
    }

void MinimizeAnimationEffect::windowMinimized( EffectWindow* w )
    {
    mTimeLine[w].setCurveShape(TimeLine::EaseInCurve);
    mTimeLine[w].setProgress(0.0f);
    }

void MinimizeAnimationEffect::windowUnminimized( EffectWindow* w )
    {
    mTimeLine[w].setCurveShape(TimeLine::EaseOutCurve);
    mTimeLine[w].setProgress(1.0f);
    }

} // namespace

