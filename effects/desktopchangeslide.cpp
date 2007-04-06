/*****************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2006 Lubos Lunak <l.lunak@kde.org>

You can Freely distribute this program under the GNU General Public
License. See the file "COPYING" for the exact licensing terms.
******************************************************************/

#include "desktopchangeslide.h"

#include <options.h>
#include <workspace.h>

namespace KWin
{

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
        *mask |= Scene::PAINT_SCREEN_TRANSFORMED | Scene::PAINT_SCREEN_BACKGROUND_FIRST;
    effects->prePaintScreen( mask, region, time );
    }

void DesktopChangeSlideEffect::prePaintWindow( EffectWindow* w, int* mask, QRegion* paint, QRegion* clip, int time )
    {
    if( progress != MAX_PROGRESS )
        {
        if( w->isOnAllDesktops())
            {
            if( painting_sticky )
                *mask |= Scene::PAINT_WINDOW_TRANSFORMED;
            else
                w->disablePainting( Scene::Window::PAINT_DISABLED_BY_DESKTOP );
            }
        else if( w->isOnDesktop( painting_desktop ))
            w->enablePainting( Scene::Window::PAINT_DISABLED_BY_DESKTOP );
        else
            w->disablePainting( Scene::Window::PAINT_DISABLED_BY_DESKTOP );
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
    QPoint destPos = desktopPos( Workspace::self()->currentDesktop());
    QPoint diffPos = destPos - startPos;
    int w = 0;
    int h = 0;
    if( options->rollOverDesktops )
        {
        int x, y;
        Qt::Orientation orientation;
        Workspace::self()->calcDesktopLayout( &x, &y, &orientation );
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
    if( options->rollOverDesktops )
        {
        currentRegion |= ( currentRegion & QRect( -w, 0, w, h )).translated( w, 0 );
        currentRegion |= ( currentRegion & QRect( 0, -h, w, h )).translated( 0, h );
        currentRegion |= ( currentRegion & QRect( w, 0, w, h )).translated( -w, 0 );
        currentRegion |= ( currentRegion & QRect( 0, h, w, h )).translated( 0, -h );
        }
    bool do_sticky = true;
    for( int desktop = 1;
         desktop <= Workspace::self()->numberOfDesktops();
         ++desktop )
        {
        QRect desktopRect( desktopPos( desktop ), displaySize );
        if( currentRegion.contains( desktopRect )) // part of the desktop needs painting
            {
            painting_desktop = desktop;
            painting_sticky = do_sticky;
            painting_diff = QPoint( currentPos - desktopRect.topLeft());
            if( options->rollOverDesktops )
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
            d.xTranslate -= painting_diff.x();
            d.yTranslate -= painting_diff.y();
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
            data.xTranslate += painting_diff.x();
            data.yTranslate += painting_diff.y();
            }
        }
    effects->paintWindow( w, mask, region, data );
    }

void DesktopChangeSlideEffect::postPaintScreen()
    {
    if( progress != MAX_PROGRESS )
        Workspace::self()->addRepaintFull(); // trigger next animation repaint
    effects->postPaintScreen();
    }

// Gives a position of the given desktop when all desktops are arranged in a grid
QPoint DesktopChangeSlideEffect::desktopPos( int desktop )
    {
    int x, y;
    Qt::Orientation orientation;
    Workspace::self()->calcDesktopLayout( &x, &y, &orientation );
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
        if( options->rollOverDesktops )
            {
            int x, y;
            Qt::Orientation orientation;
            Workspace::self()->calcDesktopLayout( &x, &y, &orientation );
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
        if( options->rollOverDesktops )
            {
            currentRegion |= ( currentRegion & QRect( -w, 0, w, h )).translated( w, 0 );
            currentRegion |= ( currentRegion & QRect( 0, -h, w, h )).translated( 0, h );
            currentRegion |= ( currentRegion & QRect( w, 0, w, h )).translated( -w, 0 );
            currentRegion |= ( currentRegion & QRect( 0, h, w, h )).translated( 0, -h );
            }
        QRect desktopRect( desktopPos( Workspace::self()->currentDesktop()), QSize( displayWidth(), displayHeight()));
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
        diffPos = ( currentPos - desktopRect.topLeft());
        startPos = desktopRect.topLeft() + diffPos / ( MAX_PROGRESS - progress ) * MAX_PROGRESS;
        }
    else
        {
        progress = 0;
        startPos = desktopPos( old );
        }
    Workspace::self()->addRepaintFull();
    }

} // namespace
