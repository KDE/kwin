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

#include "floating.h"

#include "tiling/tile.h"
#include "client.h"

namespace KWin
{

Floating::Floating(Workspace *w)
    : TilingLayout(w)
{
}

void Floating::postAddTile(Tile *t)
{
    if (t->floating())
        was_floating.insert(t);
}

void Floating::arrange(QRect wgeom)
{
    foreach (Tile * t, tiles()) {
        if (!t->floating())
            t->floatTile();
        workspace()->place(t->client(), wgeom);
        t->setGeometry(t->client()->geometry());
    }
}

void Floating::preRemoveTile(Tile *t)
{
    if (! was_floating.contains(t))
        t->unfloatTile();
}

Floating::~Floating()
{
}

}
