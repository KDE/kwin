/********************************************************************
Tabstrip KWin window decoration
This file is part of the KDE project.

Copyright (C) 2009 Jorge Mata <matamax123@gmail.com>
Copyright (C) 2009 Lucas Murray <lmurray@undefinedfire.com>

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*********************************************************************/

#include "tabstripfactory.h"
#include "tabstripdecoration.h"

#include <KConfig>
#include <KConfigGroup>
#include <KDebug>

extern "C"
    {
    KDE_EXPORT KDecorationFactory *create_factory()
        {
        return new TabstripFactory();
        }
    }

Qt::AlignmentFlag TabstripFactory::titlealign = Qt::AlignCenter;
bool TabstripFactory::show_icon = true;
QColor TabstripFactory::outline_color = Qt::white;

TabstripFactory::TabstripFactory()
    {
    initialized = false;
    readConfig();
    initialized = true;
    }

TabstripFactory::~TabstripFactory()
    {
    }

KDecoration *TabstripFactory::createDecoration( KDecorationBridge *bridge )
    {
    return ( new TabstripDecoration( bridge, this ) )->decoration();
    }

bool TabstripFactory::supports( Ability ability ) const
    {
    switch( ability )
        {
        case AbilityButtonMenu:
        case AbilityAnnounceColors:
        case AbilityButtonOnAllDesktops:
        case AbilityButtonSpacer:
        case AbilityButtonHelp:
        case AbilityButtonMinimize:
        case AbilityButtonMaximize:
        case AbilityButtonClose:
        case AbilityButtonAboveOthers:
        case AbilityButtonBelowOthers:
        case AbilityButtonShade:
        case AbilityClientGrouping:
            return true;
        default:
            return false;
        };
    }

bool TabstripFactory::readConfig()
    {
    KConfig config( "tabstriprc" );
    KConfigGroup cg = config.group( "General" );
    Qt::AlignmentFlag oldalign = titlealign;
    QString align = cg.readEntry( "TitleAlignment", "Center" );
    if( align == "Left" )
        titlealign = Qt::AlignLeft;
    else if( align == "Center" )
        titlealign = Qt::AlignHCenter;
    else if( align == "Right" )
        titlealign = Qt::AlignRight;
    show_icon = cg.readEntry( "ShowIcon", true );
    outline_color = cg.readEntry( "OutlineColor", QColor( Qt::white ) );
    return ( titlealign != oldalign );
    }

bool TabstripFactory::reset( unsigned long changed )
    {
    initialized = false;
    bool c_change = readConfig();
    initialized = true;
    if( c_change || ( changed & ( SettingDecoration | SettingButtons | SettingBorder ) ) )
        return true;
    else
        {
        resetDecorations( changed );
        return false;
        }
    }

QList< KDecorationDefines::BorderSize > TabstripFactory::borderSizes() const
    {
    return QList< BorderSize >() << BorderNormal;
    }
