/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2007 Lubos Lunak <l.lunak@kde.org>
Copyright (C) 2007 Christian Nitschkowski <christian.nitschkowski@kdemail.net>

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

#include <kconfiggroup.h>

#include "diminactive.h"

namespace KWin
{

KWIN_EFFECT( diminactive, DimInactiveEffect )

DimInactiveEffect::DimInactiveEffect()
    {
    KConfigGroup conf = EffectsHandler::effectConfig("DimInactive");

    dim_panels = conf.readEntry("DimPanels", false);
    dim_by_group = conf.readEntry("DimByGroup", true);
    dim_strength = conf.readEntry("Strength", 25);
    active = effects->activeWindow();
    }

void DimInactiveEffect::paintWindow( EffectWindow* w, int mask, QRegion region, WindowPaintData& data )
    {
    bool dim = false;
    if( active && dim_by_group && active->group() == w->group())
        dim = false;
    else if( active && !dim_by_group && active == w )
        dim = false;
    else if( w->isDock())
        dim = dim_panels;
    else if( !w->isNormalWindow() && !w->isDialog())
        dim = false;
    else
        dim = true;
    if( dim )
        {
        data.brightness *= (1.0 - (dim_strength / 100.0));
        data.saturation *= (1.0 - (dim_strength / 100.0));
        }
    effects->paintWindow( w, mask, region, data );
    }

void DimInactiveEffect::windowActivated( EffectWindow* w )
    {
    if( active != NULL )
        {
        if( dim_by_group )
            {
            if(( w == NULL || w->group() != active->group()) && active->group() != NULL )
                { // repaint windows that are not longer in active group
                foreach( EffectWindow* tmp, active->group()->members())
                    tmp->addRepaintFull();
                }
            }
        else
            active->addRepaintFull();
        }
    active = w;
    if( active != NULL )
        if( dim_by_group )
            {
            if( active->group() != NULL )
                { // repaint newly active windows
                foreach( EffectWindow* tmp, active->group()->members())
                    tmp->addRepaintFull();
                }
            }
        else
            active->addRepaintFull();
    }

} // namespace
