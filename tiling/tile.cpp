/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2009 Nikhil Marathe <nsm.nikhil@gmail.com>

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

#include "tile.h"

#include <assert.h>

#include <QQueue>

#include "client.h"
#include "workspace.h"
#include "tiling/tiling.h"

namespace KWin
{

Tile::Tile(Client *c, const QRect& area)
    : m_client(c),
      m_floating(false)
{
    Q_ASSERT(c != NULL);
    setGeometry(area);
    m_prevGeom = c->geometry();
    if (!c->isResizable())
        floatTile();
}

/*
 * NOTE: Why isn't left/right/parent copied?
 * Because they might be deleted at any point, so we can't keep pointers to them
 * Also it doesn't make sense in the areas where copy is actually going to be used.
 * Since we will be getting a new parent and children.
 */
Tile::Tile(const Tile& orig)
    : m_client(orig.client()),
      m_prevGeom(orig.m_prevGeom),
      m_floating(orig.floating())
{
    setGeometry(orig.geometry());
}

Tile::~Tile()
{
    restorePreviousGeometry();
}

void Tile::commit()
{
    m_client->setGeometry(geometry(), ForceGeometrySet);
}

void Tile::setGeometry(int x, int y, int w, int h)
{
    QRect old = m_geom;
    m_geom.setTopLeft(QPoint(x, y));
    m_geom.setWidth(w);
    m_geom.setHeight(h);

    if (old == m_geom)
        return;

    if (floating())
        m_prevGeom = m_geom;

}

void Tile::floatTile()
{
    if (floating()) return;

    // note, order of setting m_floating to true
    // then calling restore is important
    // childGeometryChanged will check for ignoreGeometry()
    m_floating = true;

    restorePreviousGeometry();

    commit();
    client()->workspace()->tiling()->notifyTilingWindowActivated(client());
    // TODO: notify layout manager
}

void Tile::unfloatTile()
{
    if (!floating()) return;

    m_floating = false;
    m_prevGeom = m_client->geometry();

    setGeometry(m_client->workspace()->clientArea(PlacementArea, m_client));
    commit();
    // TODO: notify layout manager

}

void Tile::restorePreviousGeometry()
{
    if (m_prevGeom.isNull()) {
        QRect area = m_client->workspace()->clientArea(PlacementArea, m_client);
        m_client->workspace()->place(m_client, area);
    } else {
        m_client->setGeometry(m_prevGeom, ForceGeometrySet);
    }
    setGeometry(m_client->geometry());
}

bool Tile::minimized() const
{
    return m_client->isMinimized();
}

void Tile::focus()
{
    m_client->workspace()->activateClient(m_client, true);
}

void Tile::dumpTile(const QString& indent) const
{
    kDebug(1212) << indent << m_client
                 << (floating() ? "floating" : "not floating")
                 << (ignoreGeometry() ? "ignored" : "tiled")
                 << m_geom ;
}
}
