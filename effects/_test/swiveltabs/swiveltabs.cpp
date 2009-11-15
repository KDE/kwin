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

#include "swiveltabs.h"
#include <cmath>

namespace KWin
{

KWIN_EFFECT( swiveltabs, SwivelTabsEffect )
KWIN_EFFECT_SUPPORTED( swiveltabs, SwivelTabsEffect::supported() )

SwivelTabsEffect::SwivelTabsEffect()
    {
    isActive = false;
    PI = 2.0 * acos( 0.0 );
    reconfigure( ReconfigureAll );
    }

bool SwivelTabsEffect::supported()
    {
    return effects->compositingType() == OpenGLCompositing;
    }

void SwivelTabsEffect::reconfigure( ReconfigureFlags )
    {
    KConfigGroup conf = EffectsHandler::effectConfig("SwivelTabs");
    vertical = conf.readEntry("SwivelVertical", true );
    horizontal = conf.readEntry("SwivelHorizontal", true );
    totalTime = conf.readEntry("SwivelDuration", 500 );
    }

void SwivelTabsEffect::prePaintScreen( ScreenPrePaintData& data, int time )
    {
    if( isActive )
        data.mask |= Effect::PAINT_SCREEN_WITH_TRANSFORMED_WINDOWS;
    effects->prePaintScreen( data, time );
    }

void SwivelTabsEffect::prePaintWindow( EffectWindow *w, WindowPrePaintData &data, int time )
    {
    if( isActive && ( w == windows.show || w == windows.hide ) )
        {
        data.quads = data.quads.makeGrid( 40 );
        data.setTransformed();
        w->enablePainting( EffectWindow::PAINT_DISABLED );
        windows.time.addTime( time );
        }
    effects->prePaintWindow( w, data, time );
    }

void SwivelTabsEffect::paintWindow( EffectWindow* w, int mask, QRegion region, WindowPaintData& data )
    {
    if( isActive && ( w == windows.show || w == windows.hide ) )
        {
        for( int i = 0; i < data.quads.count(); ++i )
            transformQuad( data.quads[i] );
        }
    effects->paintWindow( w, mask, region, data );
    }

void SwivelTabsEffect::postPaintWindow( EffectWindow* w )
    {
    if( isActive && ( w == windows.show || w == windows.hide ) )
        w->addRepaintFull();
    effects->postPaintWindow( w );
    }

void SwivelTabsEffect::transformQuad( WindowQuad &quad )
    {
    double F = windows.time.progress();
    int width = quad[1].x()-quad[0].x();
    int height = quad[3].y() - quad[0].y();
    int cx = quad[0].x() + ( width/2 );
    int cy = quad[0].y() + ( height/2 );
    if( lastF < 0.5 && F > 0.5 )
        {
        effects->setElevatedWindow( windows.hide, false );
        effects->setElevatedWindow( windows.show, true );
        }
    lastF = F;
    if( F >= 1.0 )
        {
        isActive = false;
        effects->setElevatedWindow( windows.hide, false );
        effects->setElevatedWindow( windows.show, false );
        }
    if( F < 0.5 )
        {
        if( horizontal )
            {
            quad[0].setX( quad[0].x() + ( width * F ) );
            quad[3].setX( quad[3].x() + ( width * F ) );
            quad[1].setX( quad[1].x() - ( width * F ) );
            quad[2].setX( quad[2].x() - ( width * F ) );
            }
        if( vertical )
            {
            quad[0].setY( quad[0].y() + ( height * F ) );
            quad[3].setY( quad[3].y() - ( height * F ) );
            quad[1].setY( quad[1].y() + ( height * F ) );
            quad[2].setY( quad[2].y() - ( height * F ) );
            }
        }
    else
        {
        F -= 0.5;
        if( horizontal )
            {
            quad[0].setX( cx - ( width * F ) );
            quad[3].setX( cx - ( width * F ) );
            quad[1].setX( cx + ( width * F ) );
            quad[2].setX( cx + ( width * F ) );
            }
        if( vertical )
            {
            quad[0].setY( cy - ( height * F ) );
            quad[3].setY( cy + ( height * F ) );
            quad[1].setY( cy - ( height * F ) );
            quad[2].setY( cy + ( height * F ) );
            }
        }
    }

void SwivelTabsEffect::clientGroupItemSwitched( EffectWindow* from, EffectWindow* to )
    {
    if( isActive || from->isMinimized() )
        return;
    windows.show = to;
    windows.hide = from;
    windows.time.setCurveShape( TimeLine::LinearCurve );
    windows.time.setDuration( animationTime( totalTime ) );
    windows.time.setProgress( 0.0f );
    lastF = 0.0;
    isActive = true;
    effects->setElevatedWindow( to, false );
    effects->setElevatedWindow( from, true );
    }

}
