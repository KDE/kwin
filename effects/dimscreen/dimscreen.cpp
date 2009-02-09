/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

 Copyright (C) 2008, 2009 Martin Gräßlin <kde@martin-graesslin.com>

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
#include "dimscreen.h"

#include <kwinglutils.h>

namespace KWin
{

KWIN_EFFECT( dimscreen, DimScreenEffect )

DimScreenEffect::DimScreenEffect()
    : mActivated( false )
    , activateAnimation( false )
    , deactivateAnimation( false )
    {
    reconfigure( ReconfigureAll );
    }

DimScreenEffect::~DimScreenEffect()
    {
    }

void DimScreenEffect::reconfigure( ReconfigureFlags )
    {
    timeline.setDuration( animationTime( 250 ));
    }

void DimScreenEffect::prePaintScreen( ScreenPrePaintData& data, int time )
    {
    if( mActivated && activateAnimation && !effects->activeFullScreenEffect() )
        timeline.addTime( time );
    if( mActivated && deactivateAnimation )
        timeline.removeTime( time );
    if( mActivated && effects->activeFullScreenEffect() )
        timeline.removeTime( time );
    if( mActivated && !activateAnimation && !deactivateAnimation && !effects->activeFullScreenEffect() && timeline.value() != 1.0 )
        timeline.addTime( time );
    effects->prePaintScreen( data, time );
    }

void DimScreenEffect::postPaintScreen()
    {
    if( mActivated )
        {
        if( activateAnimation && timeline.value() == 1.0 )
            {
            activateAnimation = false;
            effects->addRepaintFull();
            }
        if( deactivateAnimation && timeline.value() == 0.0 )
            {
            deactivateAnimation = false;
            mActivated = false;
            effects->addRepaintFull();
            }
        // still animating
        if( timeline.value() > 0.0 && timeline.value() < 1.0 )
            effects->addRepaintFull();
        }
    effects->postPaintScreen();
    }

void DimScreenEffect::paintWindow( EffectWindow *w, int mask, QRegion region, WindowPaintData &data )
    {
    if( mActivated && ( w != window ) &&
        !( w->isComboBox() || w->isDropdownMenu() || w->isTooltip() ) )
        {
        data.brightness *= (1.0 - 0.33 * timeline.value() );
        data.saturation *= (1.0 - 0.33 * timeline.value() );
        }
    effects->paintWindow( w, mask, region, data );
    }

void DimScreenEffect::windowActivated( EffectWindow *w )
    {
    if( !w ) return;
    QStringList check;
    check << "kdesu kdesu";
    check << "kdesudo kdesudo";
    check << "polkit-kde-manager polkit-kde-manager";
    if( check.contains( w->windowClass() ) )
        {
        mActivated = true;
        activateAnimation = true;
        deactivateAnimation = false;
        window = w;
        effects->addRepaintFull();
        }
    else
        {
        if( mActivated)
            {
            activateAnimation = false;
            deactivateAnimation = true;
            effects->addRepaintFull();
            }
        }
    }
} // namespace
