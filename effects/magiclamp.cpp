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
            icon = QRect( displayWidth() / 2, displayHeight(), 0, 0 );

        QRect area = effects->clientArea( PlacementArea, w );
        IconPosition position = Bottom;
        // top
        if( icon.y() + icon.height() <= area.y() )
            {
            position = Top;
            }
        // bottom
        if( icon.y() >= area.y()+area.height() )
            {
            position = Bottom;
            }
        // left
        if( icon.x() + icon.width() <= area.x() )
            {
            position = Left;
            }
        // right
        if( icon.x() >= area.x()+ area.width() )
            {
            position = Right;
            }

        WindowQuadList newQuads;
        foreach( WindowQuad quad, data.quads )
            {
            if( position == Top || position == Bottom )
                {
                // quadFactor defines how fast a quad is vertically moved: y coordinates near to window top are slowed down
                // it is used as quadFactor^3/windowHeight^3
                // quadFactor is the y position of the quad but is changed towards becomming the window height
                // by that the factor becomes 1 and has no influence any more
                float quadFactor;
                // how far has a quad to be moved? Distance between icon and window multiplied by the progress and by the quadFactor
                float yOffsetTop;
                float yOffsetBottom;
                // top and bottom progress is the factor which defines how far the x values have to be changed
                // factor is the current moved y value diveded by the distance between icon and window
                float topProgress;
                float bottomProgress;
                if( position == Bottom )
                    {
                    quadFactor = quad[0].y() + (geo.height() - quad[0].y())*progress;
                    yOffsetTop = (icon.y() + quad[0].y() - geo.y())*progress*
                        ((quadFactor*quadFactor*quadFactor)/(geo.height()*geo.height()*geo.height()));
                    quadFactor = quad[2].y() + (geo.height() - quad[2].y())*progress;
                    yOffsetBottom = (icon.y() + quad[2].y() - geo.y())*progress*
                        ((quadFactor*quadFactor*quadFactor)/(geo.height()*geo.height()*geo.height()));
                    topProgress = qMin( yOffsetTop/(icon.y()+icon.height() - geo.y()-(float)(quad[0].y()/geo.height()*geo.height())), 1.0f );
                    bottomProgress = qMin( yOffsetBottom/(icon.y()+icon.height() - geo.y()-(float)(quad[2].y()/geo.height()*geo.height())), 1.0f );
                    }
                else
                    {
                    quadFactor = geo.height() - quad[0].y() + (quad[0].y())*progress;
                    yOffsetTop = (geo.y() - icon.height() + geo.height() + quad[0].y() - icon.y())*progress*
                        ((quadFactor*quadFactor*quadFactor)/(geo.height()*geo.height()*geo.height()));
                    quadFactor = geo.height() - quad[2].y() + (quad[2].y())*progress;
                    yOffsetBottom = (geo.y() - icon.height() + geo.height() + quad[2].y() - icon.y())*progress*
                        ((quadFactor*quadFactor*quadFactor)/(geo.height()*geo.height()*geo.height()));
                    topProgress = qMin( yOffsetTop/(geo.y() - icon.height() + geo.height() - icon.y() - 
                        (float)((geo.height()-quad[0].y())/geo.height()*geo.height())), 1.0f );
                    bottomProgress = qMin( yOffsetBottom/(geo.y() - icon.height() + geo.height() - icon.y() - 
                        (float)((geo.height()-quad[2].y())/geo.height()*geo.height())), 1.0f );
                    }
                if( position == Top )
                    {
                    yOffsetTop *= -1;
                    yOffsetBottom *= -1;
                    }
                if( topProgress < 0 )
                    topProgress *= -1;
                if( bottomProgress < 0 )
                    bottomProgress *= -1;

                // x values are moved towards the center of the icon
                quad[0].setX( (icon.x() + icon.width()*(quad[0].x()/geo.width()) - (quad[0].x() + geo.x()))*topProgress + quad[0].x() );
                quad[1].setX( (icon.x() + icon.width()*(quad[1].x()/geo.width()) - (quad[1].x() + geo.x()))*topProgress + quad[1].x() );
                quad[2].setX( (icon.x() + icon.width()*(quad[2].x()/geo.width()) - (quad[2].x() + geo.x()))*bottomProgress + quad[2].x() );
                quad[3].setX( (icon.x() + icon.width()*(quad[3].x()/geo.width()) - (quad[3].x() + geo.x()))*bottomProgress + quad[3].x() );

                quad[0].setY( quad[0].y() + yOffsetTop );
                quad[1].setY( quad[1].y() + yOffsetTop );
                quad[2].setY( quad[2].y() + yOffsetBottom );
                quad[3].setY( quad[3].y() + yOffsetBottom );
                }
            else
                {
                float quadFactor;
                float xOffsetLeft;
                float xOffsetRight;
                float leftProgress;
                float rightProgress;
                if( position == Right )
                    {
                    quadFactor = quad[0].x() + (geo.width() - quad[0].x())*progress;
                    xOffsetLeft = (icon.x() + quad[0].x() - geo.x())*progress*
                        ((quadFactor*quadFactor*quadFactor)/(geo.width()*geo.width()*geo.width()));
                    quadFactor = quad[1].x() + (geo.width() - quad[1].x())*progress;
                    xOffsetRight = (icon.x() + quad[1].x() - geo.x())*progress*
                        ((quadFactor*quadFactor*quadFactor)/(geo.width()*geo.width()*geo.width()));
                    leftProgress = qMin( xOffsetLeft/(icon.x()+icon.width() - geo.x()-(float)(quad[0].x()/geo.width()*geo.width())), 1.0f );
                    rightProgress = qMin( xOffsetRight/(icon.x()+icon.width() - geo.x()-(float)(quad[1].x()/geo.width()*geo.width())), 1.0f );
                    }
                else
                    {
                    quadFactor = geo.width() - quad[0].x() + (quad[0].x())*progress;
                    xOffsetLeft = (geo.x() - icon.width() + geo.width() + quad[0].x() - icon.x())*progress*
                        ((quadFactor*quadFactor*quadFactor)/(geo.width()*geo.width()*geo.width()));
                    quadFactor = geo.width() - quad[1].x() + (quad[1].x())*progress;
                    xOffsetRight = (geo.x() - icon.width() + geo.width() + quad[1].x() - icon.x())*progress*
                        ((quadFactor*quadFactor*quadFactor)/(geo.width()*geo.width()*geo.width()));
                    leftProgress = qMin( xOffsetLeft/(geo.x() - icon.width() + geo.width() - icon.x() - 
                        (float)((geo.width()-quad[0].x())/geo.width()*geo.width())), 1.0f );
                    rightProgress = qMin( xOffsetRight/(geo.x() - icon.width() + geo.width() - icon.x() - 
                        (float)((geo.width()-quad[1].x())/geo.width()*geo.width())), 1.0f );
                    }
                if( position == Left )
                    {
                    xOffsetLeft *= -1;
                    xOffsetRight *= -1;
                    }
                if( leftProgress < 0 )
                    leftProgress *= -1;
                if( rightProgress < 0 )
                    rightProgress *= -1;

                quad[0].setY( (icon.y() + icon.height()*(quad[0].y()/geo.height()) - (quad[0].y() + geo.y()))*leftProgress + quad[0].y() );
                quad[1].setY( (icon.y() + icon.height()*(quad[1].y()/geo.height()) - (quad[1].y() + geo.y()))*rightProgress + quad[1].y() );
                quad[2].setY( (icon.y() + icon.height()*(quad[2].y()/geo.height()) - (quad[2].y() + geo.y()))*rightProgress + quad[2].y() );
                quad[3].setY( (icon.y() + icon.height()*(quad[3].y()/geo.height()) - (quad[3].y() + geo.y()))*leftProgress + quad[3].y() );

                quad[0].setX( quad[0].x() + xOffsetLeft );
                quad[1].setX( quad[1].x() + xOffsetRight );
                quad[2].setX( quad[2].x() + xOffsetRight );
                quad[3].setX( quad[3].x() + xOffsetLeft );
                }
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
    mTimeLineWindows[w].setDuration( animationTime( 250 ));
    mTimeLineWindows[w].setProgress(0.0f);
    }

void MagicLampEffect::windowUnminimized( EffectWindow* w )
    {
    mTimeLineWindows[w].setCurveShape(TimeLine::LinearCurve);
    mTimeLineWindows[w].setDuration( animationTime( 250 ));
    mTimeLineWindows[w].setProgress(1.0f);
    }

} // namespace
