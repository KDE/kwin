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

#ifndef SPIRAL_H
#define SPIRAL_H

#include <Qt>

#include "tiling/tilinglayout.h"

namespace KWin
{
class Workspace;
class Tile;
class Client;

class Spiral : public TilingLayout
{
public:
    Spiral(Workspace *);
    ~Spiral();

    void addTile(Tile *t);
    void removeTile(Tile *t);

private:
    void arrange(QRect wgeom);

};
} // end namespace

#endif
