/*****************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2007 Lubos Lunak <l.lunak@kde.org>
Copyright (C) 2007 Christian Nitschkowski <christian.nitschkowski@kdemail.net>

You can Freely distribute this program under the GNU General Public
License. See the file "COPYING" for the exact licensing terms.
******************************************************************/

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
    if( active == NULL )
        {
        if( !w->isDock() || dim_panels )
            dim = true;
        else
            dim = false;
        }
    else if( dim_by_group && active->group() == w->group())
        dim = false;
    else if( !dim_by_group && active == w )
        dim = false;
    else if( w->isDock())
        dim = dim_panels;
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
