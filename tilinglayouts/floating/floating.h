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

#ifndef FLOATING_H
#define FLOATING_H

#include <Qt>
#include <QSet>

#include "tiling/tilinglayout.h"
#include "tiling/tile.h"

namespace KWin
{
class Workspace;
class Floating : public TilingLayout
{
public:
    Floating(Workspace *);
    ~Floating();

private:
    void arrange(QRect wgeom);
    void postAddTile(Tile *t);
    void preRemoveTile(Tile *t);

    Tile::Direction m_dir;
    Tile *m_split;

    /*
     * Tiles are added to was_floating if they
     * were floating before being added to this layout
     *
     * When the layout is changed, was_floating is
     * referred to, to restore the final state of the
     * Tile
     */
    QSet<Tile *> was_floating;
};
} // end namespace

#endif
