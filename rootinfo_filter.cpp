/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2017 Martin Flöser <mgraesslin@kde.org>

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
#include "rootinfo_filter.h"
#include "netinfo.h"
#include "virtualdesktops.h"

namespace KWin
{

RootInfoFilter::RootInfoFilter(RootInfo *parent)
    : X11EventFilter(QVector<int>{XCB_PROPERTY_NOTIFY, XCB_CLIENT_MESSAGE})
    , m_rootInfo(parent)
{
}

bool RootInfoFilter::event(xcb_generic_event_t *event)
{
    NET::Properties dirtyProtocols;
    NET::Properties2 dirtyProtocols2;
    m_rootInfo->event(event, &dirtyProtocols, &dirtyProtocols2);
    if (dirtyProtocols & NET::DesktopNames)
        VirtualDesktopManager::self()->save();
    if (dirtyProtocols2 & NET::WM2DesktopLayout)
        VirtualDesktopManager::self()->updateLayout();
    return false;
}

}
