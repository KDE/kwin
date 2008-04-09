/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 1999, 2000    Daniel M. Duley <mosfet@kde.org>
Copyright (C) 2003 Lubos Lunak <l.lunak@kde.org>

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

#include "plugins.h"

#include <kglobal.h>
#include <klocale.h>
#include <stdlib.h>
#include <QPixmap>

namespace KWin
{

PluginMgr::PluginMgr()
    : KDecorationPlugins( KGlobal::config())
    {
    defaultPlugin = (QPixmap::defaultDepth() > 8) ?
            "kwin3_ozone" : "kwin3_plastik";
    loadPlugin( "" ); // load the plugin specified in cfg file
    }

void PluginMgr::error( const QString &error_msg )
    {
    qWarning( "%s", (i18n("KWin: ") + error_msg +
                    i18n("\nKWin will now exit...")).toLocal8Bit().data() );
    exit(1);
    }

bool PluginMgr::provides( Requirement )
    {
    return false;
    }

} // namespace
