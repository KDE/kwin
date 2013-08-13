/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2013 Martin Gräßlin <mgraesslin@kde.org>

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
#include "screenedgeitem.h"
#include <config-kwin.h>
#ifdef KWIN_BUILD_SCREENEDGES
#include "screenedge.h"
#endif

namespace KWin
{

ScreenEdgeItem::ScreenEdgeItem(QObject* parent)
    : QObject(parent)
    , m_enabled(true)
    , m_edge(NoEdge)
{
}

ScreenEdgeItem::~ScreenEdgeItem()
{
}

void ScreenEdgeItem::setEnabled(bool enabled)
{
    if (m_enabled == enabled) {
        return;
    }
    disableEdge();
    m_enabled = enabled;
    enableEdge();
    emit enabledChanged();
}

void ScreenEdgeItem::setEdge(Edge edge)
{
    if (m_edge == edge) {
        return;
    }
    disableEdge();
    m_edge = edge;
    enableEdge();
    emit edgeChanged();
}

void ScreenEdgeItem::enableEdge()
{
    if (!m_enabled || m_edge == NoEdge) {
        return;
    }
#ifdef KWIN_BUILD_SCREENEDGES
    ScreenEdges::self()->reserve(static_cast<ElectricBorder>(m_edge), this, "borderActivated");
#endif
}

void ScreenEdgeItem::disableEdge()
{
    if (!m_enabled || m_edge == NoEdge) {
        return;
    }
#ifdef KWIN_BUILD_SCREENEDGES
    ScreenEdges::self()->unreserve(static_cast<ElectricBorder>(m_edge), this);
#endif
}

bool ScreenEdgeItem::borderActivated(ElectricBorder edge)
{
    if (edge != static_cast<ElectricBorder>(m_edge) || !m_enabled) {
        return false;
    }
    emit activated();
    return true;
}

} // namespace
