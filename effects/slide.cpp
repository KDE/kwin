/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2007 Lubos Lunak <l.lunak@kde.org>
Copyright (C) 2008 Lucas Murray <admin@undefinedfire.com>

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

#include "slide.h"

#include <math.h>

namespace KWin
{

KWIN_EFFECT( slide, SlideEffect )

SlideEffect::SlideEffect()
    : slide( false )
    {
    mTimeLine.setCurveShape(TimeLine::EaseInOutCurve);
    reconfigure( ReconfigureAll );
    }

void SlideEffect::reconfigure( ReconfigureFlags )
    {
    mTimeLine.setDuration( animationTime( 250 ));
    }

void SlideEffect::prePaintScreen( ScreenPrePaintData& data, int time )
    {
    if( slide )
        {
        mTimeLine.addTime(time);

        // PAINT_SCREEN_BACKGROUND_FIRST is needed because screen will be actually painted more than once,
        // so with normal screen painting second screen paint would erase parts of the first paint
        if( mTimeLine.value() != 1 )
            data.mask |= PAINT_SCREEN_TRANSFORMED | PAINT_SCREEN_BACKGROUND_FIRST;
        else
            {
            slide = false;
            mTimeLine.setProgress(0);
            }
        }
    effects->prePaintScreen( data, time );
    }

void SlideEffect::prePaintWindow( EffectWindow* w, WindowPrePaintData& data, int time )
    {
    if( slide )
        {
        if( w->isOnAllDesktops())
            {
            if( slide_painting_sticky )
                data.setTransformed();
            else
                w->disablePainting( EffectWindow::PAINT_DISABLED_BY_DESKTOP );
            }
        else if( w->isOnDesktop( painting_desktop ))
            w->enablePainting( EffectWindow::PAINT_DISABLED_BY_DESKTOP );
        else
            w->disablePainting( EffectWindow::PAINT_DISABLED_BY_DESKTOP );
        }
    effects->prePaintWindow( w, data, time );
    }

void SlideEffect::paintScreen( int mask, QRegion region, ScreenPaintData& data )
    {
    if( mTimeLine.value() == 0 )
        {
        effects->paintScreen( mask, region, data );
        return;
        }
    
    /*
     Transformations are done by remembering starting position of the change and the progress
     of it, the destination is computed from the current desktop. Positions of desktops
     are done using their topleft corner.
    */
    QPoint destPos = desktopRect( effects->currentDesktop(), false ).topLeft();
    QPoint diffPos = destPos - slide_start_pos;
    int w = 0;
    int h = 0;
    if( effects->optionRollOverDesktops())
        {
        int x, y;
        Qt::Orientation orientation;
        effects->calcDesktopLayout( &x, &y, &orientation );
        w = x * displayWidth();
        h = y * displayHeight();
        // wrap around if shorter
        if( diffPos.x() > 0 && diffPos.x() > w / 2 )
            diffPos.setX( diffPos.x() - w );
        if( diffPos.x() < 0 && abs( diffPos.x()) > w / 2 )
            diffPos.setX( diffPos.x() + w );
        if( diffPos.y() > 0 && diffPos.y() > h / 2 )
            diffPos.setY( diffPos.y() - h );
        if( diffPos.y() < 0 && abs( diffPos.y()) > h / 2 )
            diffPos.setY( diffPos.y() + h );
        }
    QPoint currentPos = slide_start_pos + mTimeLine.value() * diffPos;
    QSize displaySize( displayWidth(), displayHeight());
    QRegion currentRegion = QRect( currentPos, displaySize );
    if( effects->optionRollOverDesktops())
        {
        currentRegion |= ( currentRegion & QRect( -w, 0, w, h )).translated( w, 0 );
        currentRegion |= ( currentRegion & QRect( 0, -h, w, h )).translated( 0, h );
        currentRegion |= ( currentRegion & QRect( w, 0, w, h )).translated( -w, 0 );
        currentRegion |= ( currentRegion & QRect( 0, h, w, h )).translated( 0, -h );
        }
    bool do_sticky = true;
    for( int desktop = 1;
         desktop <= effects->numberOfDesktops();
         ++desktop )
        {
        QRect rect = desktopRect( desktop, false );
        if( currentRegion.contains( rect )) // part of the desktop needs painting
            {
            painting_desktop = desktop;
            slide_painting_sticky = do_sticky;
            slide_painting_diff = rect.topLeft() - currentPos;
            if( effects->optionRollOverDesktops())
                {
                if( slide_painting_diff.x() > displayWidth())
                    slide_painting_diff.setX( slide_painting_diff.x() - w );
                if( slide_painting_diff.x() < -displayWidth())
                    slide_painting_diff.setX( slide_painting_diff.x() + w );
                if( slide_painting_diff.y() > displayHeight())
                    slide_painting_diff.setY( slide_painting_diff.y() - h );
                if( slide_painting_diff.y() < -displayHeight())
                    slide_painting_diff.setY( slide_painting_diff.y() + h );
                }
            do_sticky = false; // paint on-all-desktop windows only once
            ScreenPaintData d = data;
            d.xTranslate += slide_painting_diff.x();
            d.yTranslate += slide_painting_diff.y();
            // TODO mask parts that are not visible?
            effects->paintScreen( mask, region, d );
            }
        }
    }

void SlideEffect::paintWindow( EffectWindow* w, int mask, QRegion region, WindowPaintData& data )
    {
    if( slide )
        { // don't move windows on all desktops (compensate screen transformation)
        if( w->isOnAllDesktops()) // TODO also fix 'Workspace::movingClient'
            {
            data.xTranslate -= slide_painting_diff.x();
            data.yTranslate -= slide_painting_diff.y();
            }
        }
    effects->paintWindow( w, mask, region, data );
    }

void SlideEffect::postPaintScreen()
    {
    if( slide )
        effects->addRepaintFull();
    effects->postPaintScreen();
    }

// Gives a position of the given desktop when all desktops are arranged in a grid
QRect SlideEffect::desktopRect( int desktop, bool scaled ) const
    {
    int x, y;
    Qt::Orientation orientation;
    effects->calcDesktopLayout( &x, &y, &orientation );
    --desktop; // make it start with 0
    QRect rect;
    if( orientation == Qt::Horizontal )
        rect = QRect(( desktop % x ) * displayWidth(), ( desktop / x ) * displayHeight(),
            displayWidth(), displayHeight());
    else
        rect = QRect(( desktop / y ) * displayWidth(), ( desktop % y ) * displayHeight(),
            displayWidth(), displayHeight());
    if( !scaled )
        return rect;
    QRect current = desktopRect( effects->currentDesktop(), false );
    double progress = mTimeLine.value();
    rect = QRect( qRound( interpolate( rect.x() - current.x(), rect.x() / double( x ), progress )),
        qRound( interpolate( rect.y() - current.y(), rect.y() / double( y ), progress )),
        qRound( interpolate( rect.width(), displayWidth() / double( x ), progress )),
        qRound( interpolate( rect.height(), displayHeight() / double( y ), progress )));
    return rect;
    }

void SlideEffect::desktopChanged( int old )
    {
    if( effects->activeFullScreenEffect() && effects->activeFullScreenEffect() != this )
        return;
    
    if( slide ) // old slide still in progress
        {
        QPoint diffPos = desktopRect( old, false ).topLeft() - slide_start_pos;
        int w = 0;
        int h = 0;
        if( effects->optionRollOverDesktops())
            {
            int x, y;
            Qt::Orientation orientation;
            effects->calcDesktopLayout( &x, &y, &orientation );
            w = x * displayWidth();
            h = y * displayHeight();
            // wrap around if shorter
            if( diffPos.x() > 0 && diffPos.x() > w / 2 )
                diffPos.setX( diffPos.x() - w );
            if( diffPos.x() < 0 && abs( diffPos.x()) > w / 2 )
                diffPos.setX( diffPos.x() + w );
            if( diffPos.y() > 0 && diffPos.y() > h / 2 )
                diffPos.setY( diffPos.y() - h );
            if( diffPos.y() < 0 && abs( diffPos.y()) > h / 2 )
                diffPos.setY( diffPos.y() + h );
            }
        QPoint currentPos = slide_start_pos + mTimeLine.value() * diffPos;
        QRegion currentRegion = QRect( currentPos, QSize( displayWidth(), displayHeight()));
        if( effects->optionRollOverDesktops())
            {
            currentRegion |= ( currentRegion & QRect( -w, 0, w, h )).translated( w, 0 );
            currentRegion |= ( currentRegion & QRect( 0, -h, w, h )).translated( 0, h );
            currentRegion |= ( currentRegion & QRect( w, 0, w, h )).translated( -w, 0 );
            currentRegion |= ( currentRegion & QRect( 0, h, w, h )).translated( 0, -h );
            }
        QRect rect = desktopRect( effects->currentDesktop(), false );
        if( currentRegion.contains( rect ))
            { // current position is in new current desktop (e.g. quickly changing back),
              // don't do full progress
            if( abs( currentPos.x() - rect.x()) > abs( currentPos.y() - rect.y()))
                mTimeLine.setProgress(1 - abs( currentPos.x() - rect.x()) / double( displayWidth()));
            else
                mTimeLine.setProgress(1 - abs( currentPos.y() - rect.y()) / double( displayHeight()));
            }
        else // current position is not on current desktop, do full progress
            mTimeLine.setProgress(0);
        diffPos = rect.topLeft() - currentPos;
        if( mTimeLine.value() <= 0 )
            {
            // Compute starting point for this new move (given current and end positions)
            slide_start_pos = rect.topLeft() - diffPos * 1 / ( 1 - mTimeLine.value() );
            }
        else
            { // at the end, stop
            slide = false;
            mTimeLine.setProgress(0);
            }
        }
    else
        {
        mTimeLine.setProgress(0);
        slide_start_pos = desktopRect( old, false ).topLeft();
        slide = true;
        }
    effects->addRepaintFull();
    }

} // namespace

#include "slide.moc"
