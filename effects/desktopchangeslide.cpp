/*****************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2006 Lubos Lunak <l.lunak@kde.org>

You can Freely distribute this program under the GNU General Public
License. See the file "COPYING" for the exact licensing terms.
******************************************************************/

#include "desktopchangeslide.h"

namespace KWin
{

KWIN_EFFECT( DesktopChangeSlide, DesktopChangeSlideEffect )

const int MAX_PROGRESS = 500; // ms

DesktopChangeSlideEffect::DesktopChangeSlideEffect()
    : progress( MAX_PROGRESS )
    {
    }

void DesktopChangeSlideEffect::prePaintScreen( int* mask, QRegion* region, int time )
    {
    progress = qBound( 0, progress + time, MAX_PROGRESS );
    // PAINT_SCREEN_BACKGROUND_FIRST is needed because screen will be actually painted more than once,
    // so with normal screen painting second screen paint would erase parts of the first paint
    if( progress != MAX_PROGRESS )
        *mask |= PAINT_SCREEN_TRANSFORMED | PAINT_SCREEN_BACKGROUND_FIRST;
    effects->prePaintScreen( mask, region, time );
    }

void DesktopChangeSlideEffect::prePaintWindow( EffectWindow* w, int* mask, QRegion* paint, QRegion* clip, int time )
    {
    if( progress != MAX_PROGRESS )
        {
        if( w->isOnAllDesktops())
            {
            if( painting_sticky )
                *mask |= PAINT_WINDOW_TRANSFORMED;
            else
                w->disablePainting( EffectWindow::PAINT_DISABLED_BY_DESKTOP );
            }
        else if( w->isOnDesktop( painting_desktop ))
            w->enablePainting( EffectWindow::PAINT_DISABLED_BY_DESKTOP );
        else
            w->disablePainting( EffectWindow::PAINT_DISABLED_BY_DESKTOP );
        }
    effects->prePaintWindow( w, mask, paint, clip, time );
    }

void DesktopChangeSlideEffect::paintScreen( int mask, QRegion region, ScreenPaintData& data )
    {
    if( progress == MAX_PROGRESS )
        {
        effects->paintScreen( mask, region, data );
        return;
        }
    /*
     Transformations are done by remembering starting position of the change and the progress
     of it, the destination is computed from the current desktop. Positions of desktops
     are done using their topleft corner.
    */
    QPoint destPos = desktopPos( effects->currentDesktop());
    QPoint diffPos = destPos - startPos;
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
    QPoint currentPos = startPos + progress * diffPos / MAX_PROGRESS;
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
        QRect desktopRect( desktopPos( desktop ), displaySize );
        if( currentRegion.contains( desktopRect )) // part of the desktop needs painting
            {
            painting_desktop = desktop;
            painting_sticky = do_sticky;
            painting_diff = desktopRect.topLeft() - currentPos;
            if( effects->optionRollOverDesktops())
                {
                if( painting_diff.x() > displayWidth())
                    painting_diff.setX( painting_diff.x() - w );
                if( painting_diff.x() < -displayWidth())
                    painting_diff.setX( painting_diff.x() + w );
                if( painting_diff.y() > displayHeight())
                    painting_diff.setY( painting_diff.y() - h );
                if( painting_diff.y() < -displayHeight())
                    painting_diff.setY( painting_diff.y() + h );
                }
            do_sticky = false; // paint on-all-desktop windows only once
            ScreenPaintData d = data;
            d.xTranslate += painting_diff.x();
            d.yTranslate += painting_diff.y();
            // TODO mask parts that are not visible?
            effects->paintScreen( mask, region, d );
            }
        }
    }

void DesktopChangeSlideEffect::paintWindow( EffectWindow* w, int mask, QRegion region, WindowPaintData& data )
    {
    if( progress != MAX_PROGRESS )
        { // don't move windows on all desktops (compensate screen transformation)
        if( w->isOnAllDesktops()) // TODO also fix 'Workspace::movingClient'
            {
            data.xTranslate -= painting_diff.x();
            data.yTranslate -= painting_diff.y();
            }
        }
    effects->paintWindow( w, mask, region, data );
    }

void DesktopChangeSlideEffect::postPaintScreen()
    {
    if( progress != MAX_PROGRESS )
        effects->addRepaintFull(); // trigger next animation repaint
    effects->postPaintScreen();
    }

// Gives a position of the given desktop when all desktops are arranged in a grid
QPoint DesktopChangeSlideEffect::desktopPos( int desktop )
    {
    int x, y;
    Qt::Orientation orientation;
    effects->calcDesktopLayout( &x, &y, &orientation );
    --desktop; // make it start with 0
    if( orientation == Qt::Vertical )
        return QPoint(( desktop % x ) * displayWidth(), ( desktop / x ) * displayHeight());
    else
        return QPoint(( desktop / y ) * displayWidth(), ( desktop % y ) * displayHeight());
    }

void DesktopChangeSlideEffect::desktopChanged( int old )
    {
    if( progress != MAX_PROGRESS ) // old slide still in progress
        {
        QPoint diffPos = desktopPos( old ) - startPos;
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
        QPoint currentPos = startPos + progress * diffPos / MAX_PROGRESS;
        QRegion currentRegion = QRect( currentPos, QSize( displayWidth(), displayHeight()));
        if( effects->optionRollOverDesktops())
            {
            currentRegion |= ( currentRegion & QRect( -w, 0, w, h )).translated( w, 0 );
            currentRegion |= ( currentRegion & QRect( 0, -h, w, h )).translated( 0, h );
            currentRegion |= ( currentRegion & QRect( w, 0, w, h )).translated( -w, 0 );
            currentRegion |= ( currentRegion & QRect( 0, h, w, h )).translated( 0, -h );
            }
        QRect desktopRect( desktopPos( effects->currentDesktop()), QSize( displayWidth(), displayHeight()));
        if( currentRegion.contains( desktopRect ))
            { // current position is in new current desktop (e.g. quickly changing back),
              // don't do full progress
            if( abs( currentPos.x() - desktopRect.x()) > abs( currentPos.y() - desktopRect.y()))
                progress = MAX_PROGRESS - MAX_PROGRESS * abs( currentPos.x() - desktopRect.x()) / displayWidth();
            else
                progress = MAX_PROGRESS - MAX_PROGRESS * abs( currentPos.y() - desktopRect.y()) / displayHeight();
            }
        else // current position is not on current desktop, do full progress
            progress = 0;
        diffPos = desktopRect.topLeft() - currentPos;
        // Compute starting point for this new move (given current and end positions)
        startPos = desktopRect.topLeft() - diffPos * MAX_PROGRESS / ( MAX_PROGRESS - progress );
        }
    else
        {
        progress = 0;
        startPos = desktopPos( old );
        }
    effects->addRepaintFull();
    }

} // namespace
