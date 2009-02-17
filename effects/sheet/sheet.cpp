/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2007 Philip Falkner <philip.falkner@gmail.com>
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

#include "sheet.h"

#include <kconfiggroup.h>

// Effect is based on fade effect by Philip Falkner

namespace KWin
{

KWIN_EFFECT( sheet, SheetEffect )
KWIN_EFFECT_SUPPORTED( sheet, SheetEffect::supported() )

SheetEffect::SheetEffect()
    : windowCount( 0 )
    {
    reconfigure( ReconfigureAll );
    }

bool SheetEffect::supported()
    {
    return effects->compositingType() == OpenGLCompositing;
    }

void SheetEffect::reconfigure( ReconfigureFlags )
    {
    KConfigGroup conf = effects->effectConfig( "Sheet" );
    duration = animationTime( conf, "AnimationTime", 500 );
    }

void SheetEffect::prePaintScreen( ScreenPrePaintData& data, int time )
    {
    if( windowCount > 0 )
        {
        data.mask |= PAINT_SCREEN_WITH_TRANSFORMED_WINDOWS;
        screenTime = time;
        }
    effects->prePaintScreen( data, time );
    }

void SheetEffect::prePaintWindow( EffectWindow* w, WindowPrePaintData& data, int time )
    {
    if( windows.contains( w ) && ( windows[ w ].added || windows[ w ].closed ) )
        {
        if( windows[ w ].added )
            windows[ w ].timeLine->addTime( screenTime );
        if( windows[ w ].closed )
            {
            windows[ w ].timeLine->removeTime( screenTime );
            if( windows[ w ].deleted )
                {
                w->enablePainting( EffectWindow::PAINT_DISABLED_BY_DELETE );
                }
            }
        }
    effects->prePaintWindow( w, data, time );
    if( windows.contains( w ) && !w->isPaintingEnabled() && !effects->activeFullScreenEffect() )
        { // if the window isn't to be painted, then let's make sure
          // to track its progress
        if( windows[ w ].added || windows[ w ].closed )
            { // but only if the total change is less than the
              // maximum possible change
            w->addRepaintFull();
            }
        }
    }

void SheetEffect::paintWindow( EffectWindow* w, int mask, QRegion region, WindowPaintData& data )
    {
    if( windows.contains( w ) && ( windows[ w ].added || windows[ w ].closed ) )
        {
        RotationData rot;
        rot.axis = RotationData::XAxis;
        rot.angle = 60.0 * ( 1.0 - windows[ w ].timeLine->value() );
        data.rotation = &rot;
        data.yScale *= windows[ w ].timeLine->value();
        data.zScale *= windows[ w ].timeLine->value();
        data.yTranslate -= (w->y()-windows[ w ].parentY) * ( 1.0 - windows[ w ].timeLine->value() );
        effects->paintWindow( w, PAINT_WINDOW_TRANSFORMED, region, data );
        }
    else
        effects->paintWindow( w, mask, region, data );
    }

void SheetEffect::postPaintWindow( EffectWindow* w )
    {
    if( windows.contains( w ) )
        {
        if( windows[ w ].added && windows[ w ].timeLine->value() == 1.0 )
            {
            windows[ w ].added = false;
            windowCount--;
            effects->addRepaintFull();
            }
        if( windows[ w ].closed && windows[ w ].timeLine->value() == 0.0 )
            {
            windows[ w ].closed = false;
            if( windows[ w ].deleted )
                {
                windows.remove( w );
                w->unrefWindow();
                }
            windowCount--;
            effects->addRepaintFull();
            }
        if( windows[ w ].added || windows[ w ].closed )
            w->addRepaintFull();
        }
    effects->postPaintWindow( w );
    }

void SheetEffect::windowAdded( EffectWindow* w )
    {
    if( !isSheetWindow( w ) )
        return;
    windows[ w ] = WindowInfo();
    windows[ w ].added = true;
    windows[ w ].closed = false;
    windows[ w ].timeLine->setDuration( duration );
    // find parent
    foreach( EffectWindow* window, effects->stackingOrder() )
        {
        if( window->findModal() == w )
            {
            windows[ w ].parentY = window->y();
            break;
            }
        }
    windowCount++;
    w->addRepaintFull();
    }

void SheetEffect::windowClosed( EffectWindow* w )
    {
    if( !windows.contains( w ) )
        return;
    windows[ w ].added = false;
    windows[ w ].closed = true;
    windows[ w ].deleted = true;
    bool found = false;
    // find parent
    foreach( EffectWindow* window, effects->stackingOrder() )
        {
        if( window->findModal() == w )
            {
            windows[ w ].parentY = window->y();
            found = true;
            break;
            }
        }
    if( !found )
        windows[ w ].parentY = 0;
    windowCount++;
    w->refWindow();
    w->addRepaintFull();
    }

void SheetEffect::windowDeleted( EffectWindow* w )
    {
    windows.remove( w );
    }

bool SheetEffect::isSheetWindow( EffectWindow* w )
    {
    return ( w->isModal() );
    }

} // namespace
