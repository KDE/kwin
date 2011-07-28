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

#include "columns.h"

#include "client.h"
#include "tiling/tile.h"

#include <kdecoration.h>

namespace KWin
{

// TODO: caching of actually tiled windows
// Columns is doing a lot of looping
// checking which tiles are actually *tiled*
// ( ie. not floating or minimized )
// This can probably be moved to TilingLayout
// and cached. But remember to preserve order!

Columns::Columns(Workspace *w)
    : TilingLayout(w)
    , m_leftWidth(0)
{
}

KDecorationDefines::Position Columns::resizeMode(Client *c, KDecorationDefines::Position currentMode) const
{
    Tile *t = findTile(c);

    if (!t)
        return currentMode;

    if (t && t->floating())
        return currentMode;

    QList<Tile *> tiled(tiles());

    QMutableListIterator<Tile *> i(tiled);
    while (i.hasNext()) {
        Tile *tile = i.next();
        if (tile->ignoreGeometry())
            i.remove();
    }

    if (tiled.first() == t
            && (currentMode == KDecorationDefines::PositionRight
                || currentMode == KDecorationDefines::PositionTopRight
                || currentMode == KDecorationDefines::PositionBottomRight)) {
        return KDecorationDefines::PositionRight;
    }

    // in right column so only left resize allowed
    if (tiled.contains(t)
            && (tiled.first() != t)
            && (currentMode == KDecorationDefines::PositionLeft
                || currentMode == KDecorationDefines::PositionTopLeft
                || currentMode == KDecorationDefines::PositionBottomLeft)) {
        return KDecorationDefines::PositionLeft;
    }

    return KDecorationDefines::PositionCenter;
}

bool Columns::clientResized(Client *c, const QRect &moveResizeGeom, const QRect &orig)
{
    if (TilingLayout::clientResized(c, moveResizeGeom, orig))
        return true;

    Tile *t = findTile(c);

    QList<Tile *> tiled(tiles());

    QMutableListIterator<Tile *> i(tiled);
    while (i.hasNext()) {
        Tile *tile = i.next();
        if (tile->ignoreGeometry())
            i.remove();
    }

    if (tiled.first() == t) {
        m_leftWidth = moveResizeGeom.width();
    } else {
        m_leftWidth = layoutArea(t).width() - moveResizeGeom.width();
    }

    arrange(layoutArea(t));
    return true;
}

void Columns::arrange(QRect wgeom)
{
    QList<Tile *> tiled(tiles());

    QMutableListIterator<Tile *> i(tiled);
    while (i.hasNext()) {
        Tile *t = i.next();
        if (t->ignoreGeometry())
            i.remove();
    }

    int n = tiled.length();
    if (n < 1)
        return;
    if (n == 1) {
        tiled.first()->setGeometry(wgeom);
        tiled.first()->commit();
        return;
    }

    // save the original before we mangle it
    int totalWidth = wgeom.width();
    if (m_leftWidth == 0)
        m_leftWidth = wgeom.width() / 2;

    if (n > 1)
        wgeom.setWidth(m_leftWidth);
    tiled.first()->setGeometry(wgeom);
    tiled.first()->commit();

    wgeom.moveLeft(wgeom.x() + m_leftWidth);
    wgeom.setWidth(totalWidth - m_leftWidth);
    int ht = wgeom.height() / (n - 1);
    wgeom.setHeight(ht);

    int mult = 0;
    int originalTop = wgeom.y();
    for (QList<Tile *>::const_iterator it = ++tiled.constBegin() ; it != tiled.constEnd() ; ++it) {
        if ((*it)->floating())
            continue;
        (*it)->setGeometry(wgeom);
        (*it)->commit();
        mult++;
        wgeom.moveTop(originalTop + mult * ht);
    }
}
}
