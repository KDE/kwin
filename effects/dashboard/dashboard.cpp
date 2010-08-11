/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2010 Andreas Demmer <ademmer@opensuse.org>

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

#include "dashboard.h"

namespace KWin
{
KWIN_EFFECT(dashboard, DashboardEffect)

DashboardEffect::DashboardEffect()
    : transformWindow( false )
{
    // propagate that the effect is loaded
    propagate();

    // read settings
    reconfigure(ReconfigureAll);
    }

DashboardEffect::~DashboardEffect()
    {
    unpropagate();
    }

void DashboardEffect::propagate()
    {
    // TODO: better namespacing for atoms
    atom = XInternAtom( display(), "_WM_EFFECT_KDE_DASHBOARD", false );
    effects->registerPropertyType( atom, true );

    // TODO: maybe not the best way to propagate the loaded effect
    unsigned char dummy = 0;
    XChangeProperty( display(), rootWindow(), atom, atom, 8, PropModeReplace, &dummy, 1 );
    }

void DashboardEffect::unpropagate()
    {
    effects->registerPropertyType(atom, false);
    XDeleteProperty(display(), rootWindow(), atom);
    }

void DashboardEffect::reconfigure( ReconfigureFlags )
    {
    // read settings again
    KConfigGroup config = EffectsHandler::effectConfig("Dashboard");

    brightness = config.readEntry("Brightness", "50");
    saturation = config.readEntry("Saturation", "50");
    blur = config.readEntry("Blur", false);
    }

void DashboardEffect::paintWindow( EffectWindow* w, int mask, QRegion region, WindowPaintData& data )
    {
    if( transformWindow && ( w != window ) && w->isManaged() )
        {
        // dashboard active, transform other windows
        data.brightness = brightness.toDouble() / 100;
        data.saturation = saturation.toDouble() / 100;
        }

    effects->paintWindow( w, mask, region, data );
    }

void DashboardEffect::postPaintScreen()
    {
    if( transformWindow )
        {
        if( retransformWindow )
            {
            retransformWindow = false;
            transformWindow = false;
            effects->addRepaintFull();
            }
        }

    effects->postPaintScreen();
    }

bool DashboardEffect::isDashboard ( EffectWindow *w )
    {
    if( w->windowClass() == "dashboard dashboard")
        {
        return true;
        }
    else
        {
        return false;
        }
    }

void DashboardEffect::windowActivated( EffectWindow *w )
    {
    if( !w )
        return;

    // apply effect on dashboard activation
    if( isDashboard( w ) )
        {
        transformWindow = true;
        window = w;

        if ( blur )
            {
            w->setData( WindowBlurBehindRole, w->geometry() );
            }

        effects->addRepaintFull();
        }
    else
        {
        if( transformWindow)
            {
            retransformWindow = true;
            effects->addRepaintFull();
            }
        }
   }

void DashboardEffect::windowAdded( EffectWindow* w )                                   
    {                                                                                      
    propertyNotify( w, atom );        

                                                    
    if( isDashboard( w ) )                            
        {                                                                                  
        // Tell other windowAdded() effects to ignore this window                          
        w->setData( WindowAddedGrabRole, QVariant::fromValue( static_cast<void*>( this )));
	w->addRepaintFull();                                                               
        }   
    }       
                                                                                           
void DashboardEffect::windowClosed( EffectWindow* w )                                  
    {   
    propertyNotify( w, atom );        

                                                    
    if( isDashboard( w ) )                            
        {      
        // Tell other windowClosed() effects to ignore this window                          
        w->setData( WindowClosedGrabRole, QVariant::fromValue( static_cast<void*>( this )));
	w->addRepaintFull();                                                               
        }   
    }

} // namespace
