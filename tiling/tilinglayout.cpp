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

#include "tiling/tilinglayout.h"

#include <QCursor>

#include "client.h"
#include "tiling/tile.h"
#include "workspace.h"
#include "tiling/tiling.h"

namespace KWin
{

TilingLayout::TilingLayout(Workspace *w)
    : QObject()
    , m_workspace(w)
{
    connect(m_workspace, SIGNAL(configChanged()), this, SLOT(reconfigureTiling()));
}

TilingLayout::~TilingLayout()
{
    qDeleteAll(m_tiles);
    m_tiles.clear();
}

int TilingLayout::findTilePos(Client *c) const
{
    int i = 0;
    foreach (Tile * t, m_tiles) {
        if (t->client() == c)
            return i;
        i++;
    }
    return -1;
}

Tile* TilingLayout::findTile(Client *c) const
{
    int i = findTilePos(c);
    if (i != -1)
        return m_tiles[ i ];
    return NULL;
}

void TilingLayout::clientMinimizeToggled(Client *c)
{
    // just rearrange since that will check for state
    Tile *t = findTile(c);
    if (t)
        arrange(layoutArea(t));
}

bool TilingLayout::clientResized(Client *c, const QRect &moveResizeGeom, const QRect &orig)
{
    if (moveResizeGeom == orig)
        return true;

    Tile *t = findTile(c);
    if (!t || t->ignoreGeometry()) {
        c->setGeometry(moveResizeGeom);
        return true;
    }

    return false;
}

// tries to swap the tile with the one in the new position right now
void TilingLayout::clientMoved(Client *c, const QRect &moveResizeGeom, const QRect &orig)
{
    if (moveResizeGeom == orig)
        return;

    Tile *t = findTile(c);
    if (!t) {
        c->setGeometry(moveResizeGeom);
        return;
    }
    if (t->floating()) {
        t->setGeometry(moveResizeGeom);
        t->commit();
        return;
    }

    Tile *r = findTileBelowPoint(QCursor::pos());
    // TODO: if the client moved in from another desktop, don't swap, add
    if (r && t) {
        swapTiles(r, t);
    }
}

void TilingLayout::swapTiles(Tile *a, Tile *b)
{
    if (a && b) {
        // t is the tile the user requested a move of
        // r is the tile below it
        int a_index = tiles().indexOf(a);
        int b_index = tiles().indexOf(b);

        // use m_tiles since tiles() is const
        // not sure how good an idea this is
        m_tiles.replace(a_index, b);
        m_tiles.replace(b_index, a);
        arrange(layoutArea(a));
    }
}

void TilingLayout::addTileNoArrange(Tile * t)
{
    if (findTile(t->client()))
        return;
    m_tiles.append(t);
    postAddTile(t);
}

void TilingLayout::addTile(Tile *t)
{
    addTileNoArrange(t);
    arrange(layoutArea(t));
}

void TilingLayout::addTile(Client *c)
{
    Q_UNUSED(c)
}

void TilingLayout::removeTileNoArrange(Tile * t)
{
    if (t == NULL)
        return;
    preRemoveTile(t);
    m_tiles.removeOne(t);
}

const QRect TilingLayout::layoutArea(Tile *t) const
{
    return m_workspace->clientArea(PlacementArea, t->client());
}

void TilingLayout::removeTile(Tile *t)
{
    if (t == NULL)
        return;
    removeTileNoArrange(t);
    if (!m_tiles.empty())
        arrange(layoutArea(m_tiles.first()));
}

void TilingLayout::removeTile(Client *c)
{
    removeTile(findTile(c));
}

void TilingLayout::toggleFloatTile(Client *c)
{
    Tile *t = findTile(c);
    if (t && t->floating())
        t->unfloatTile();
    else if (t)
        t->floatTile();

    if (t)
        arrange(layoutArea(t));
}

void TilingLayout::reconfigureTiling()
{
    //TODO also check 'untiled' windows to see if they are now requesting tiling
    foreach (Tile * t, tiles()) {
        if (t->client()->rules()->checkTilingOption(t->floating() ? 1 : 0) == 1)
            t->floatTile();
        else
            t->unfloatTile();
    }

    if (tiles().length() > 0)
        arrange(layoutArea(tiles().first()));

    foreach (Toplevel * t, workspace()->stackingOrder()) {
        if (Client *c = qobject_cast<Client*>(t)) {
            if (c->rules()->checkTilingOption(0) == 1)
                workspace()->tiling()->createTile(c);
        }
    }

}

Tile* TilingLayout::findTileBelowPoint(const QPoint &p) const
{
    foreach (Tile * t, tiles()) {
        if (t->floating())
            continue;
        if (t->geometry().contains(p))
            return t;
    }

    return NULL;
}

void TilingLayout::commit()
{
    foreach (Tile * t, m_tiles)
        t->commit();
}

/*
 * Default is to allow no resizing
 */
KDecorationDefines::Position TilingLayout::resizeMode(Client *c, KDecorationDefines::Position currentMode) const
{
    Tile *t = findTile(c);
    // if not tiled, allow resize
    if (!t)
        return currentMode;

    if (t && t->floating())
        return currentMode;
    // We return PositionCenter since it makes
    // no sense in resizing.
    return KDecorationDefines::PositionCenter;
}

} // end namespace

