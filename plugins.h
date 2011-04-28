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

#ifndef KWIN_PLUGINS_H
#define KWIN_PLUGINS_H

#include <kdecoration_plugins_p.h>

namespace KWin
{

class PluginMgr
    : public KDecorationPlugins
{
public:
    PluginMgr();
    virtual bool provides(Requirement);
    /**
     * @returns @c true if there is no decoration plugin.
     **/
    bool hasNoDecoration() const;
protected:
    virtual void error(const QString& error_msg);
private:
    void setNoDecoration(bool noDecoration);
    bool m_noDecoration;
};

} // namespace

#endif
