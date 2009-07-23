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

#include "slidingpopups.h"

#include <kdebug.h>

namespace KWin
{

KWIN_EFFECT( slidingpopups, SlidingPopupsEffect )

SlidingPopupsEffect::SlidingPopupsEffect()
    {
    mAtom = XInternAtom( display(), "_KDE_SLIDE", False );
    effects->registerPropertyType( mAtom, true );
    // TODO hackish way to announce support, make better after 4.0
    unsigned char dummy = 0;
    XChangeProperty( display(), rootWindow(), mAtom, mAtom, 8, PropModeReplace, &dummy, 1 );
    }

SlidingPopupsEffect::~SlidingPopupsEffect()
    {
    XDeleteProperty( display(), rootWindow(), mAtom );
    effects->registerPropertyType( mAtom, false );
    }

void SlidingPopupsEffect::prePaintScreen( ScreenPrePaintData& data, int time )
    {
    if( !mAppearingWindows.isEmpty() || !mDisappearingWindows.isEmpty() )
        data.mask |= PAINT_SCREEN_WITH_TRANSFORMED_WINDOWS;
    effects->prePaintScreen( data, time );
    }

void SlidingPopupsEffect::prePaintWindow( EffectWindow* w, WindowPrePaintData& data, int time )
    {
    if( mAppearingWindows.contains( w ) )
        {
        mAppearingWindows[ w ].addTime( time );
        if( mAppearingWindows[ w ].value() < 1 )
            data.setTransformed();
        else
            mAppearingWindows.remove( w );
        }
    else if( mDisappearingWindows.contains( w ) )
        {
        mDisappearingWindows[ w ].addTime( time );
        if( mDisappearingWindows[ w ].value() < 1 )
            data.setTransformed();
        else
            {
            mDisappearingWindows.remove( w );
            w->unrefWindow();
            }
        }
    effects->prePaintWindow( w, data, time );
    }

void SlidingPopupsEffect::paintWindow( EffectWindow* w, int mask, QRegion region, WindowPaintData& data )
    {
    bool animating = false;
    bool appearing = false;
    QRegion clippedRegion = region;

    if( mAppearingWindows.contains( w ) )
        {
        appearing = true;
        animating = true;
        }
    else if( mDisappearingWindows.contains( w ) )
        {
        appearing = false;
        animating = true;
        }


    if( animating )
        {
        const qreal progress = appearing?(1 - mAppearingWindows[ w ].value()):mDisappearingWindows[ w ].value();
        const int start = mWindowsData[ w ].start;

        switch (mWindowsData[ w ].from)
            {
            case West:
                data.xTranslate +=  (start - w->width()) * progress;
                clippedRegion = clippedRegion.subtracted(QRegion(start - w->width(), w->y(), w->width(), w->height()));
                break;
            case North:
                data.yTranslate +=  (start - w->height()) * progress;
                clippedRegion = clippedRegion.subtracted(QRegion(w->x(), start - w->height(), w->width(), w->height()));
                break;
            case East:
                data.xTranslate +=  (start - w->x()) * progress;
                clippedRegion = clippedRegion.subtracted(QRegion(w->x()+w->width(), w->y(), w->width(), w->height()));
                break;
            case South:
            default:
                data.yTranslate +=  (start - w->y()) * progress;
                clippedRegion = clippedRegion.subtracted(QRegion(w->x(), start, w->width(), w->height()));
            }
        }

    effects->paintWindow( w, mask, clippedRegion, data );
    }

void SlidingPopupsEffect::postPaintWindow( EffectWindow* w )
    {
    if( mAppearingWindows.contains( w ) || mDisappearingWindows.contains( w ) )
        w->addRepaintFull(); // trigger next animation repaint
    effects->postPaintWindow( w );
    }

void SlidingPopupsEffect::windowAdded( EffectWindow* w )
    {
    propertyNotify( w, mAtom );
    if( w->isOnCurrentDesktop() && mWindowsData.contains( w ) )
        {
        mAppearingWindows[ w ].setDuration( animationTime( 250 ));
        mAppearingWindows[ w ].setProgress( 0.0 );
        mAppearingWindows[ w ].setCurveShape( TimeLine::EaseOutCurve );

        w->addRepaintFull();
        }
    }

void SlidingPopupsEffect::windowClosed( EffectWindow* w )
    {
    propertyNotify( w, mAtom );
    if( w->isOnCurrentDesktop() && !w->isMinimized() && mWindowsData.contains( w ) )
        {
        w->refWindow();
        mAppearingWindows.remove( w );
        mDisappearingWindows[ w ].setDuration( animationTime( 250 ));
        mDisappearingWindows[ w ].setProgress( 0.0 );
        mDisappearingWindows[ w ].setCurveShape( TimeLine::EaseOutCurve );

        w->addRepaintFull();
        }
    }

void SlidingPopupsEffect::windowDeleted( EffectWindow* w )
    {
    mAppearingWindows.remove( w );
    mDisappearingWindows.remove( w );
    mWindowsData.remove( w );
    }

void SlidingPopupsEffect::propertyNotify( EffectWindow* w, long a )
    {
    if( a != mAtom )
        return;

    QByteArray data = w->readProperty( mAtom, mAtom, 32 );
    if( data.length() < 1 )
        return;
    long* d = reinterpret_cast< long* >( data.data());
    Data animData;
    animData.start = d[ 0 ];
    animData.from = (Position)d[ 1 ];
    mWindowsData[ w ] = animData;
    }
} // namespace
