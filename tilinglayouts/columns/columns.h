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

#ifndef COLUMNS_H
#define COLUMNS_H

#include <Qt>

#include "tiling/tilinglayout.h"

namespace KWin
{
class Workspace;
class Tile;
class Client;

// simulates a 2 column right layout for now, make it arbitrary
// in columns and direction
class Columns : public TilingLayout
{
public:
    Columns(Workspace *);
    KDecorationDefines::Position resizeMode(Client *c, KDecorationDefines::Position currentMode) const;
    bool clientResized(Client *c, const QRect &moveResizeGeom, const QRect &orig);

private:
    void arrange(QRect wgeom);
    int m_leftWidth; // width of left column
};
} // end namespace

#endif
