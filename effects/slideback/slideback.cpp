/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2009 Michael Zanetti <michael_zanetti@gmx.net>

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

#include "slideback.h"

#include <kconfiggroup.h>
#include <kdebug.h>

namespace KWin
{

KWIN_EFFECT( slideback, SlideBackEffect )

SlideBackEffect::SlideBackEffect()
    {
    // updateStackingOrder();
    }

void SlideBackEffect::windowActivated( EffectWindow* w )
    {
    if( w == NULL )
        {
        return;
        }

    if( disabled || effects->activeFullScreenEffect() )  // TabBox or PresentWindows/Cube in progress
        {
        updateStackingOrder();
        disabled = false;
        return;
        }

    if( !isWindowUsable( w ) || !isWindowOnTop( w ) )     // Focus changed but stacking still the same
        {
        updateStackingOrder();
        return;
        }
    // Determine all windows on top of the activated one
    bool currentFound = false;
    foreach( EffectWindow *tmp, oldStackingOrder )
        {
        if( !currentFound )
            {
            if( tmp == w )
                {
                currentFound = true;
                }
            }
        else
            {
            if( isWindowUsable( tmp ) && tmp->isOnDesktop( w->desktop() ) )
                {
                // Do we have to move it?
                if( tmp->geometry().intersects( w->geometry() ) )
                    {
                    effects->setElevatedWindow( tmp, true );
                    elevatedList.append( tmp );

                    QRect slideRect = getSlideDestination( w->geometry(), tmp->geometry() );
                    motionManager.manage( tmp );
                    motionManager.moveWindow( tmp, slideRect );
                    destinationList.insert( tmp, slideRect );
                    coveringWindows.append( tmp );
                    }
                else
                    {
                    //Does it intersect with a moved (elevated) window and do we have to elevate it too?
                    foreach( EffectWindow *elevatedWindow, elevatedList )
                        {
                        if( tmp->geometry().intersects( elevatedWindow->geometry() ) )
                            {
                            effects->setElevatedWindow( tmp, true );
                            elevatedList.append( tmp );
                            break;
                            }
                        }

                    }
                }
            if( tmp->isDock() )
                {
                effects->setElevatedWindow( tmp, true );
                elevatedList.append( tmp );
                }
            }
        }
    // If a window is minimized it could happen that the panels stay elevated without any windows sliding.
    // clear all elevation settings
    if( !motionManager.managingWindows() )
        {
        foreach( EffectWindow* tmp, elevatedList )
            {
            effects->setElevatedWindow( tmp, false );
            }
        }
    updateStackingOrder();
    }

QRect SlideBackEffect::getSlideDestination( const QRect &windowUnderGeometry, const QRect &windowOverGeometry)
    {
    // Determine the shortest way:
    int leftSlide = windowUnderGeometry.left() - windowOverGeometry.right() - 20;
    int rightSlide = windowUnderGeometry.right() - windowOverGeometry.left() + 20;
    int upSlide = windowUnderGeometry.top() -  windowOverGeometry.bottom() - 20;
    int downSlide = windowUnderGeometry.bottom() - windowOverGeometry.top() + 20;

    int horizSlide = leftSlide;
    if( abs( horizSlide ) > abs( rightSlide ) )
        {
        horizSlide = rightSlide;
        }
    int vertSlide = upSlide;
    if( abs( vertSlide ) > abs( downSlide ) )
	{
	vertSlide = downSlide;
	}

    QRect slideRect = windowOverGeometry;
    if( abs( horizSlide ) < abs( vertSlide ) )
	{
	slideRect.moveLeft( slideRect.x() + horizSlide );
	}
    else
	{
	slideRect.moveTop( slideRect.y() + vertSlide );
	}
    return slideRect;
    }

void SlideBackEffect::updateStackingOrder()
    {
    usableOldStackingOrder = usableWindows( effects->stackingOrder() );
    oldStackingOrder = effects->stackingOrder();
    }

void SlideBackEffect::prePaintScreen( ScreenPrePaintData &data, int time )
    {
    if( motionManager.managingWindows() )
        {
        motionManager.calculate( time );
        data.mask |= Effect::PAINT_SCREEN_WITH_TRANSFORMED_WINDOWS;
        }
    effects->prePaintScreen( data, time );
    }

void SlideBackEffect::postPaintScreen()
    {
    if( motionManager.areWindowsMoving() )
        {
        effects->addRepaintFull();
        }
    effects->postPaintScreen();
    }

void SlideBackEffect::prePaintWindow( EffectWindow *w, WindowPrePaintData &data, int time )
    {
    if( motionManager.areWindowsMoving() )
        {
        if( motionManager.isManaging( w ) )
            {
            data.setTransformed();
            }
        }

    effects->prePaintWindow( w, data, time );
    }

void SlideBackEffect::paintWindow( EffectWindow *w, int mask, QRegion region, WindowPaintData &data )
    {
    if( !motionManager.managingWindows() && stackingOrderChanged() && ( w == newTopWindow() ) )
        {
        /* This can happen because of two reasons:
           - a window has received the focus earlier without beeing raised and is raised now. -> call windowActivated() now
           - paintWindow() is called with a new stackingOrder before activateWindow(). Bug? -> don't draw the overlapping content;*/
        foreach( EffectWindow *tmp, oldStackingOrder )
            {
            if( oldStackingOrder.lastIndexOf( tmp ) > oldStackingOrder.lastIndexOf( w ) && isWindowUsable( tmp ) )
                {
                region = region.subtracted( tmp->geometry() );
                }
            }
        // Add a full repaint to make sure the not painted area is repainted soon
        effects->addRepaintFull();
        // Finally call windowActivated in case a already active window is raised.
        if( !disabled )
            {
            windowActivated( w );
            }
        }
    if( motionManager.areWindowsMoving() )
        {
        if( motionManager.isManaging( w ) )
            {
            motionManager.apply( w, data );
            }
        }
    effects->paintWindow( w, mask, region, data );
    }

void SlideBackEffect::postPaintWindow( EffectWindow* w )
    {
    if( motionManager.isManaging( w ) )
        {
        if( destinationList.contains( w ) )
            {
            // has window reched its destination?
            if(( abs( motionManager.transformedGeometry( w ).x() - destinationList[w].x() ) < 1 ) &&
                    ( abs( motionManager.transformedGeometry( w ).y() - destinationList[w].y() ) < 1 ) )
                {
                // If we are still intersecting with the activeWindow it is moving. slide to somewhere else
                // restore the stacking order of all windows not intersecting any more except panels
                if( coveringWindows.contains( w ) )
                    {
                    foreach( EffectWindow *tmp, elevatedList )
                        {
                        if( coveringWindows.contains( tmp ) )
                            {
                            QRect newDestination = getSlideDestination( effects->activeWindow()->geometry(), tmp->geometry() );
                            motionManager.moveWindow( tmp, newDestination );
                            destinationList[tmp] = newDestination;
                            }
                        else
                            {
                            if( !tmp->isDock() )
                                {
                                effects->setElevatedWindow( tmp, false );
                                elevatedList.removeAll( tmp );
                                }
                            }
                        }
                    }
                else
                    {
                    // Move the window back where it belongs
                    motionManager.moveWindow( w, w->geometry() );
                    destinationList.remove( w );
                    }
                }
            }
        else
            {
            // is window back at its original position?
            if(( abs( motionManager.transformedGeometry( w ).x() - w->geometry().x() ) < 1 ) &&
                    ( abs( motionManager.transformedGeometry( w ).y() - w->geometry().y() ) < 1 ) )
                {
                motionManager.unmanage( w );
                }
            }
        if( coveringWindows.contains( w ) )
            {
            // It could happen that there is no aciveWindow() here if the user clicks the close-button on an inactive window.
            // Just skip... the window will be removed in windowDeleted() later
            if( effects->activeWindow() && !motionManager.transformedGeometry( w ).intersects( effects->activeWindow()->geometry() ) )
                {
                coveringWindows.removeAll( w );
                if( coveringWindows.isEmpty() )
                    {
                    // Restore correct stacking order
                    foreach( EffectWindow *tmp, elevatedList )
                        {
                        effects->setElevatedWindow( tmp, false );
                        }
                    elevatedList.clear();
                    }
                }
            }
        }
    effects->postPaintWindow( w );
    }

void SlideBackEffect::windowDeleted( EffectWindow* w )
    {
    usableOldStackingOrder.removeAll( w );
    oldStackingOrder.removeAll( w );
    coveringWindows.removeAll( w );
    elevatedList.removeAll( w );
    }

void SlideBackEffect::windowAdded( KWin::EffectWindow* w )
    {
    updateStackingOrder();
    }


void SlideBackEffect::tabBoxClosed()
    {
    disabled = true;
    }

bool SlideBackEffect::isWindowOnTop( EffectWindow* w )
    {
    return usableWindows( effects->stackingOrder() ).last() == w ? true : false;
    }

bool SlideBackEffect::isWindowUsable( EffectWindow* w )
    {
    return w && ( w->isNormalWindow() || w->isDialog() ) && !w->isDeleted() && !w->isMinimized();
    }

EffectWindowList SlideBackEffect::usableWindows( const EffectWindowList & allWindows )
    {
    EffectWindowList retList;
    foreach( EffectWindow *tmp, allWindows )
        {
        if( isWindowUsable( tmp ) )
            {
            retList.append( tmp );
            }
        }
    return retList;
    }

bool SlideBackEffect::stackingOrderChanged()
    {
    return !( usableOldStackingOrder == usableWindows( effects->stackingOrder() ) );
    }

EffectWindow* SlideBackEffect::newTopWindow()
    {
    EffectWindowList stacking = usableWindows( effects->stackingOrder() );
    return stacking.isEmpty() ? NULL : stacking.last();
    }

} //Namespace
