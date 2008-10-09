/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2006 Rivo Laks <rivolaks@hot.ee>

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

#include "dialogparent.h"

namespace KWin
{

KWIN_EFFECT( dialogparent, DialogParentEffect )

DialogParentEffect::DialogParentEffect()
    {
    reconfigure( ReconfigureAll );
    }

void DialogParentEffect::reconfigure( ReconfigureFlags )
    {
    // How long does it take for the effect to get it's full strength (in ms)
    changeTime = animationTime( 200 );
    }

void DialogParentEffect::prePaintWindow( EffectWindow* w, WindowPrePaintData& data, int time )
    {
    // Check if this window has a modal dialog and change the window's
    //  effect's strength accordingly
    bool hasDialog = w->findModal() != NULL;
    if( hasDialog )
        {
        // Increase effect strength of this window
        effectStrength[w] = qMin(1.0, effectStrength[w] + time/changeTime);
        }
    else
        {
        effectStrength[w] = qMax(0.0, effectStrength[w] - time/changeTime);
        }

    // Call the next effect
    effects->prePaintWindow( w, data, time );
    }

void DialogParentEffect::paintWindow( EffectWindow* w, int mask, QRegion region, WindowPaintData& data )
    {
    double s = effectStrength[w];
    if(s > 0.0f)
        {
        // Brightness will be within [1.0; 0.6]
        data.brightness *= (1.0 - s * 0.4);
        // Saturation within [1.0; 0.4]
        data.saturation *= (1.0 - s * 0.6);
        }

    // Call the next effect.
    effects->paintWindow( w, mask, region, data );
    }

void DialogParentEffect::postPaintWindow( EffectWindow* w )
    {
    double s = effectStrength[w];

    // If strength is between 0 and 1, the effect is still in progress and the
    //  window has to be repainted during the next pass
    if( s > 0.0 && s < 1.0 )
        w->addRepaintFull(); // trigger next animation repaint

    // Call the next effect.
    effects->postPaintWindow( w );
    }

void DialogParentEffect::windowActivated( EffectWindow* w )
    {
    // If this window is a dialog, we need to repaint it's parent window, so
    //  that the effect could be run for it
    // Set the window to be faded (or NULL if no window is active).
    if( w && w->isModal() )
        {
        // w is a modal dialog
        EffectWindowList mainwindows = w->mainWindows();
        foreach( EffectWindow* parent, mainwindows )
            parent->addRepaintFull();
        }
    }

void DialogParentEffect::windowClosed( EffectWindow* w )
    {
    // If this window is a dialog, we need to repaint it's parent window, so
    //  that the effect could be run for it
    // Set the window to be faded (or NULL if no window is active).
    if ( w && w->isModal() )
        {
        // w is a modal dialog
        EffectWindowList mainwindows = w->mainWindows();
        foreach( EffectWindow* parent, mainwindows )
            parent->addRepaintFull();
        }
    }

} // namespace
