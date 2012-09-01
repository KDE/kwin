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
#include "config-kwin.h"

#include <kglobal.h>
#include <klocale.h>
#include <stdlib.h>
#include <QPixmap>

namespace KWin
{

PluginMgr::PluginMgr()
    : KDecorationPlugins(KGlobal::config())
    , m_noDecoration(false)
{
    defaultPlugin = (QPixmap::defaultDepth() > 8) ?
                    "kwin3_oxygen" : "kwin3_plastik";
#ifndef KWIN_BUILD_OXYGEN
    defaultPlugin = "kwin3_plastik";
#endif
#ifdef KWIN_BUILD_DECORATIONS
    loadPlugin("");   // load the plugin specified in cfg file
#else
    setNoDecoration(true);
#endif
}

void PluginMgr::error(const QString &error_msg)
{
    qWarning("%s", QString(i18n("KWin: ") + error_msg).toLocal8Bit().data());

    setNoDecoration(true);
}

bool PluginMgr::provides(Requirement)
{
    return false;
}

void PluginMgr::setNoDecoration(bool noDecoration)
{
    m_noDecoration = noDecoration;
}

bool PluginMgr::hasNoDecoration() const
{
    return m_noDecoration;
}

} // namespace
