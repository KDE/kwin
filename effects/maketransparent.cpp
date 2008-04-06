/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2006 Lubos Lunak <l.lunak@kde.org>

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

#include "maketransparent.h"

#include <kconfiggroup.h>

namespace KWin
{

KWIN_EFFECT( maketransparent, MakeTransparentEffect )

MakeTransparentEffect::MakeTransparentEffect()
    {
    KConfigGroup conf = effects->effectConfig("MakeTransparent");
    decoration = conf.readEntry( "Decoration", 1.0 );
    moveresize = conf.readEntry( "MoveResize", 0.8 );
    dialogs = conf.readEntry( "Dialogs", 1.0 );
    inactive = conf.readEntry( "Inactive", 1.0 );
    comboboxpopups = conf.readEntry( "ComboboxPopups", 1.0 );
    menus = conf.readEntry( "Menus", 1.0 );
    individualmenuconfig = conf.readEntry( "IndividualMenuConfig", false );
    if( individualmenuconfig )
        {
        dropdownmenus = conf.readEntry( "DropdownMenus", 1.0 );
        popupmenus = conf.readEntry( "PopupMenus", 1.0 );
        tornoffmenus = conf.readEntry( "TornOffMenus", 1.0 );
        }
    else
        {
        dropdownmenus = menus;
        popupmenus = menus;
        tornoffmenus = menus;
        }
    active = effects->activeWindow();
    }

void MakeTransparentEffect::prePaintWindow( EffectWindow* w, WindowPrePaintData& data, int time )
    {
    if( decoration != 1.0 && w->hasDecoration())
        {
        data.mask |= PAINT_WINDOW_TRANSLUCENT;
        // don't clear PAINT_WINDOW_OPAQUE, contents are not affected
        data.clip &= w->contentsRect().translated( w->pos()); // decoration cannot clip
        }
    if(( moveresize != 1.0 && ( w->isUserMove() || w->isUserResize()))
        || ( dialogs != 1.0 && w->isDialog()))
        {
        data.setTranslucent();
        }
    if( ( dropdownmenus != 1.0 && w->isDropdownMenu() )
        || ( popupmenus != 1.0 && w->isPopupMenu() )
        || ( tornoffmenus != 1.0 && w->isMenu() )
        || ( comboboxpopups != 1.0 && w->isComboBox() ) )
        {
        data.setTranslucent();
        }

    effects->prePaintWindow( w, data, time );
    }

void MakeTransparentEffect::paintWindow( EffectWindow* w, int mask, QRegion region, WindowPaintData& data )
    {
    if( inactive != 1.0 && isInactive(w) )
        {
        data.opacity *= inactive;
        }
    else
        {
        if( decoration != 1.0 && w->hasDecoration())
            data.decoration_opacity *= decoration;
        if( dialogs != 1.0 && w->isDialog())
            data.opacity *= dialogs;
        if( moveresize != 1.0 && ( w->isUserMove() || w->isUserResize()))
            data.opacity *= moveresize;
        if( dropdownmenus != 1.0 && w->isDropdownMenu() )
            data.opacity *= dropdownmenus;
        if( popupmenus != 1.0 && w->isPopupMenu() )
            data.opacity *= popupmenus;
        if( tornoffmenus != 1.0 && w->isMenu() )
            data.opacity *= tornoffmenus;
        if( comboboxpopups != 1.0 && w->isComboBox() )
            data.opacity *= comboboxpopups;
        }
    effects->paintWindow( w, mask, region, data );
    }

bool MakeTransparentEffect::isInactive( const EffectWindow* w ) const
    {
    if( active == w || w->isDock() || !w->isManaged() )
        return false;
    if( NULL != active && NULL != active->group() )
        if (active->group() == w->group() )
            return false;
    if( !w->isNormalWindow() && !w->isDialog() && !w->isDock() )
        return false;
    return true;
    }

void MakeTransparentEffect::windowUserMovedResized( EffectWindow* w, bool first, bool last )
    {
    if( moveresize != 1.0 && ( first || last ))
        w->addRepaintFull();
    }

void MakeTransparentEffect::windowActivated( EffectWindow* w )
    {
    if( inactive != 1.0 )
        {
        if( NULL != active && active != w )
            {
            if( ( NULL == w || w->group() != active->group() ) &&
                NULL != active->group() )
                {
                // Active group has changed. so repaint old group
                foreach( EffectWindow *tmp, active->group()->members() )
                    tmp->addRepaintFull();
                }
            else
                active->addRepaintFull();
            }

        if( NULL != w )
            {
            if (NULL != w->group() )
                {
                // Repaint windows in new group
                foreach( EffectWindow *tmp, w->group()->members() )
                    tmp->addRepaintFull();
                }
            else
                w->addRepaintFull();
            }
        }

    active = w;
    }

} // namespace
