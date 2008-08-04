/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

 Copyright (C) 2008 Martin Gräßlin <ubuntu@martin-graesslin.com>

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

// based on minimize animation by Rivo Laks <rivolaks@hot.ee>

#include "magiclamp.h"

namespace KWin
{

KWIN_EFFECT( magiclamp, MagicLampEffect )

MagicLampEffect::MagicLampEffect()
    {
    mActiveAnimations = 0;
    }


void MagicLampEffect::prePaintScreen( ScreenPrePaintData& data, int time )
    {
    mActiveAnimations = mTimeLineWindows.count();
    if( mActiveAnimations > 0 )
        // We need to mark the screen windows as transformed. Otherwise the
        //  whole screen won't be repainted, resulting in artefacts
        data.mask |= PAINT_SCREEN_WITH_TRANSFORMED_WINDOWS;

    effects->prePaintScreen(data, time);
    }

void MagicLampEffect::prePaintWindow( EffectWindow* w, WindowPrePaintData& data, int time )
    {
    if( mTimeLineWindows.contains( w ))
        {
        if( w->isMinimized() )
            {
            mTimeLineWindows[w].addTime(time);
            if( mTimeLineWindows[w].progress() >= 1.0f )
                mTimeLineWindows.remove( w );
            }
        else
            {
            mTimeLineWindows[w].removeTime(time);
            if( mTimeLineWindows[w].progress() <= 0.0f )
                mTimeLineWindows.remove( w );
            }

        // Schedule window for transformation if the animation is still in
        //  progress
        if( mTimeLineWindows.contains( w ))
            {
            // We'll transform this window
            data.setTransformed();
            data.quads = data.quads.makeGrid( 40 );
            w->enablePainting( EffectWindow::PAINT_DISABLED_BY_MINIMIZE );
            }
        }

    effects->prePaintWindow( w, data, time );
    }

void MagicLampEffect::paintWindow( EffectWindow* w, int mask, QRegion region, WindowPaintData& data )
    {
    if( mTimeLineWindows.contains( w ))
    {
        // 0 = not minimized, 1 = fully minimized
        float progress = mTimeLineWindows[w].value();

        QRect geo = w->geometry();
        QRect icon = w->iconGeometry();
        // If there's no icon geometry, minimize to the center of the screen
        if( !icon.isValid() )
            icon = QRect( displayWidth() / 2, displayHeight() / 2, 0, 0 );

        WindowQuadList newQuads;
        foreach( WindowQuad quad, data.quads )
            {
            // quadFactor defines how fast a quad is vertically moved: y coordinates near to window top are slowed down
            // it is used as quadFactor^3/windowHeight^3
            // quadFactor is the y position of the quad but is changed towards becomming the window height
            // by that the factor becomes 1 and has no influence any more
            float quadFactor = quad[0].y() + (geo.height() - quad[0].y())*progress;
            // how far has a quad to be moved? Distance between icon and window multiplied by the progress and by the quadFactor
            float yOffsetTop = (icon.y() + quad[0].y() - geo.y())*progress*
                ((quadFactor*quadFactor*quadFactor)/(geo.height()*geo.height()*geo.height()));
            quadFactor = quad[2].y() + (geo.height() - quad[2].y())*progress;
            float yOffsetBottom = (icon.y() + quad[2].y() - geo.y())*progress*
                ((quadFactor*quadFactor*quadFactor)/(geo.height()*geo.height()*geo.height()));
            // top and bottom progress is the factor which defines how far the x values have to be changed
            // factor is the current moved y value diveded by the distance between icon and window
            float topProgress = qMin( yOffsetTop/(icon.y()+icon.height() - geo.y()-(float)(quad[0].y()/geo.height()*geo.height())), 1.0f );
            float bottomProgress = qMin( yOffsetBottom/(icon.y()+icon.height() - geo.y()-(float)(quad[2].y()/geo.height()*geo.height())), 1.0f );

            // x values are moved towards the center of the icon
            quad[0].setX( (icon.x() + icon.width()*(quad[0].x()/geo.width()) - (quad[0].x() + geo.x()))*topProgress + quad[0].x() );
            quad[1].setX( (icon.x() + icon.width()*(quad[1].x()/geo.width()) - (quad[1].x() + geo.x()))*topProgress + quad[1].x() );
            quad[2].setX( (icon.x() + icon.width()*(quad[2].x()/geo.width()) - (quad[2].x() + geo.x()))*bottomProgress + quad[2].x() );
            quad[3].setX( (icon.x() + icon.width()*(quad[3].x()/geo.width()) - (quad[3].x() + geo.x()))*bottomProgress + quad[3].x() );

            quad[0].setY( quad[0].y() + yOffsetTop );
            quad[1].setY( quad[1].y() + yOffsetTop );
            quad[2].setY( quad[2].y() + yOffsetBottom );
            quad[3].setY( quad[3].y() + yOffsetBottom );
            newQuads.append( quad );
            }
        data.quads = newQuads;
    }

    // Call the next effect.
    effects->paintWindow( w, mask, region, data );
    }

void MagicLampEffect::postPaintScreen()
    {
    if( mActiveAnimations > 0 )
        // Repaint the workspace so that everything would be repainted next time
        effects->addRepaintFull();
    mActiveAnimations = mTimeLineWindows.count();

    // Call the next effect.
    effects->postPaintScreen();
    }

void MagicLampEffect::windowMinimized( EffectWindow* w )
    {
    mTimeLineWindows[w].setCurveShape(TimeLine::LinearCurve);
    mTimeLineWindows[w].setDuration( 250 );
    mTimeLineWindows[w].setProgress(0.0f);
    }

void MagicLampEffect::windowUnminimized( EffectWindow* w )
    {
    mTimeLineWindows[w].setCurveShape(TimeLine::LinearCurve);
    mTimeLineWindows[w].setDuration( 250 );
    mTimeLineWindows[w].setProgress(1.0f);
    }

} // namespace
