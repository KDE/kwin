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

#include "spiral.h"

#include "client.h"
#include "tiling/tile.h"

namespace KWin
{

Spiral::Spiral(Workspace *w)
    : TilingLayout(w)
{
}

Spiral::~Spiral()
{
}

void Spiral::arrange(QRect wgeom)
{
    QList<Tile *> tiled(tiles());

    QMutableListIterator<Tile *> it(tiled);
    while (it.hasNext()) {
        Tile *t = it.next();
        if (t->ignoreGeometry())
            it.remove();
    }

    int n = tiled.length();
    int i = 1;

    foreach (Tile * t, tiled) {
        if (t->floating())
            continue;

        if (i < n) {
            if (i % 2 == 0)
                wgeom.setHeight(wgeom.height() / 2);
            else
                wgeom.setWidth(wgeom.width() / 2);
        }

        if (i % 4 == 0)
            wgeom.moveLeft(wgeom.x() - wgeom.width());
        else if (i % 2 == 0 || (i % 4 == 3 && i < n))
            wgeom.moveLeft(wgeom.x() + wgeom.width());

        if (i % 4 == 1 && i != 1)
            wgeom.moveTop(wgeom.y() - wgeom.height());
        else if ((i % 2 == 1 && i != 1)
                || (i % 4 == 0 && i < n))
            wgeom.moveTop(wgeom.y() + wgeom.height());

        t->setGeometry(wgeom);
        t->commit();
        i++;
    }
}
}
