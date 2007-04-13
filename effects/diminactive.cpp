/*****************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2007 Lubos Lunak <l.lunak@kde.org>

You can Freely distribute this program under the GNU General Public
License. See the file "COPYING" for the exact licensing terms.
******************************************************************/


#include "diminactive.h"

namespace KWin
{

KWIN_EFFECT( DimInactive, DimInactiveEffect )

DimInactiveEffect::DimInactiveEffect()
    : active( NULL )
    {
    dim_panels = false; // TODO config option
    dim_by_group = true; // TODO config option
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
        data.brightness *= 0.75;
        data.saturation *= 0.75;
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
