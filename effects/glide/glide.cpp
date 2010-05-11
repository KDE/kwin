/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2007 Philip Falkner <philip.falkner@gmail.com>
Copyright (C) 2009 Martin Gräßlin <kde@martin-graesslin.com>
Copyright (C) 2010 Alexandre Pereira <pereira.alex@gmail.com>

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

#include "glide.h"

#include <kconfiggroup.h>

// Effect is based on fade effect by Philip Falkner

namespace KWin
{

KWIN_EFFECT( glide, GlideEffect )
KWIN_EFFECT_SUPPORTED( glide, GlideEffect::supported() )

GlideEffect::GlideEffect()
    : windowCount( 0 )
    {
    reconfigure( ReconfigureAll );
    }

bool GlideEffect::supported()
    {
    return effects->compositingType() == OpenGLCompositing;
    }

void GlideEffect::reconfigure( ReconfigureFlags )
    {
    KConfigGroup conf = effects->effectConfig( "Glide" );
    duration = animationTime( conf, "AnimationTime", 350 );
    effect = (EffectStyle) conf.readEntry( "GlideEffect", 0 );
    angle = conf.readEntry( "GlideAngle", -90 );
    }

void GlideEffect::prePaintScreen( ScreenPrePaintData& data, int time )
    {
    if( windowCount > 0 )
        data.mask |= PAINT_SCREEN_WITH_TRANSFORMED_WINDOWS;
    effects->prePaintScreen( data, time );
    }

void GlideEffect::prePaintWindow( EffectWindow* w, WindowPrePaintData& data, int time )
    {
    if( windows.contains( w ) && ( windows[ w ].added || windows[ w ].closed ) )
        {
        if( windows[ w ].added )
            windows[ w ].timeLine->addTime( time );
        if( windows[ w ].closed )
            {
            windows[ w ].timeLine->removeTime( time );
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

void GlideEffect::paintWindow( EffectWindow* w, int mask, QRegion region, WindowPaintData& data )
    {
    if( windows.contains( w ) )
        {
        RotationData rot;
        rot.axis = RotationData::XAxis;
        rot.angle = angle * ( 1 - windows[ w ].timeLine->value() );
        data.rotation = &rot;
        data.opacity *= windows[ w ].timeLine->value();
        if( effect == GlideInOut )
            {
            if( windows[ w ].added )
                glideIn( w, data );
            if( windows[ w ].closed )
                glideOut( w, data );
            }
         if( effect == GlideOutIn )
            {
            if( windows[ w ].added )
                glideOut( w, data );
            if( windows[ w ].closed )
                glideIn( w, data );
            }
        if( effect == GlideIn )
                glideIn( w, data );
        if( effect == GlideOut )
                glideOut( w, data );
        effects->paintWindow( w, PAINT_WINDOW_TRANSFORMED, region, data );
        }
    else
        effects->paintWindow( w, mask, region, data );
    }

void GlideEffect::glideIn(EffectWindow* w, WindowPaintData& data )
    {
    data.xScale *= windows[ w ].timeLine->value();
    data.yScale *= windows[ w ].timeLine->value();
    data.zScale *= windows[ w ].timeLine->value();
    data.xTranslate += int( w->width() / 2 * ( 1 - windows[ w ].timeLine->value() ) );
    data.yTranslate += int( w->height() / 2 * ( 1 - windows[ w ].timeLine->value() ) );
    }

void GlideEffect::glideOut(EffectWindow* w, WindowPaintData& data )
    {
    data.xScale *= ( 2 - windows[ w ].timeLine->value() );
    data.yScale *= ( 2 - windows[ w ].timeLine->value() );
    data.zScale *= ( 2 - windows[ w ].timeLine->value() );
    data.xTranslate -= int( w->width() / 2 * ( 1 - windows[ w ].timeLine->value() ) );
    data.yTranslate -= int( w->height() / 2 * ( 1 - windows[ w ].timeLine->value() ) );
    }

void GlideEffect::postPaintWindow( EffectWindow* w )
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

void GlideEffect::windowAdded( EffectWindow* w )
    {
    if( !isGlideWindow( w ) )
        return;

    w->setData( WindowAddedGrabRole, QVariant::fromValue( static_cast<void*>( this )));
    windows[ w ] = WindowInfo();
    windows[ w ].added = true;
    windows[ w ].closed = false;
    windows[ w ].deleted = false;
    windows[ w ].timeLine->setDuration( duration );
    windows[ w ].timeLine->setCurveShape( TimeLine::EaseOutCurve );
    windowCount++;
    w->addRepaintFull();
    }

void GlideEffect::windowClosed( EffectWindow* w )
    {
    if( !windows.contains( w ) )
        return;
    w->setData( WindowClosedGrabRole, QVariant::fromValue( static_cast<void*>( this )));
    windows[ w ].added = false;
    windows[ w ].closed = true;
    windows[ w ].deleted = true;
    windows[ w ].timeLine->setDuration( duration );
    windows[ w ].timeLine->setCurveShape( TimeLine::EaseInCurve );
    windowCount++;
    w->refWindow();
    w->addRepaintFull();
    }

void GlideEffect::windowDeleted( EffectWindow* w )
    {
    //delete windows[ w ].timeLine;
    windows[ w ].timeLine = NULL;
    windows.remove( w );
    }

bool GlideEffect::isGlideWindow( EffectWindow* w )
    {
    const void* e = w->data( WindowAddedGrabRole ).value<void*>();
    // TODO: isSpecialWindow is rather generic, maybe tell windowtypes separately?
    if ( w->isPopupMenu() || w->isSpecialWindow() || w->isUtility() || ( e && e != this ))
        return false;
    return true;
    }
} // namespace
