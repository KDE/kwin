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

#ifndef KWIN_TILE_H
#define KWIN_TILE_H

#include <QRect>
#include <QString>

namespace KWin
{

class Client;
class Workspace;

class Tile
{
public:
    enum Direction {
        Top,
        Right,
        Bottom,
        Left
    };

    enum LayoutMode {
        UseRatio, // uses m_ratio
        UseGeometry, // uses current geometry of children
        Equal // distribute equally
    };

    Tile(Client *c, const QRect& area);
    Tile(const Tile& orig);
    virtual ~Tile();
    void setGeometry(const QRect& area);
    void setGeometry(int x, int y, int w, int h);

    void resize(const QRect area);

    void restorePreviousGeometry();

    void commit();

    void focus();

    // :| the float datatype interferes with naming
    void floatTile();
    void unfloatTile();

    bool minimized() const;
    bool floating() const;
    bool ignoreGeometry() const;
    QRect geometry() const;
    Client* client() const;

    void dumpTile(const QString& indent = "") const;

private:

    // -------------
    // PROPERTIES
    // -------------

    // our client
    Client *m_client;

    // tiled geometry
    QRect m_geom;
    // before tiling was enabled, if any
    QRect m_prevGeom;

    bool m_floating;

};

inline QRect Tile::geometry() const
{
    return m_geom;
}

inline Client* Tile::client() const
{
    return m_client;
}

inline bool Tile::floating() const
{
    return m_floating;
}

/*
 * should be respected by all geometry modifying methods.
 * It returns true if the Tile is 'out' of the layout,
 * due to being minimized, floating or for some other reason.
 */
inline bool Tile::ignoreGeometry() const
{
    return minimized() || floating();
}

inline void Tile::setGeometry(const QRect& area)
{
    setGeometry(area.x(), area.y(), area.width(), area.height());
}

} // namespace
#endif

