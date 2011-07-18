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

#ifndef KWIN_TILINGLAYOUT_H
#define KWIN_TILINGLAYOUT_H

#include <QRect>
#include <QList>

#include <kdecoration.h>

namespace KWin
{

class Workspace;
class Client;
class Tile;

class TilingLayout : public QObject
{
    Q_OBJECT
public:
    TilingLayout(Workspace *w);
    virtual ~TilingLayout();

    /**
     * Reimplement this to decide how the client(s) should
     * be resized.
     * Return true if an actual resize was attempted, false if not ( for whatever reason )
     */
    virtual bool clientResized(Client *c, const QRect &moveResizeGeom, const QRect &orig);
    void clientMoved(Client *c, const QRect &moveResizeGeom, const QRect &orig);
    void clientMinimizeToggled(Client *c);

    void commit();

    void setLayoutType(int t);
    int layoutType() const;

    void addTile(Tile *t);
    void addTile(Client *c);
    void removeTile(Tile *t);
    void removeTile(Client *c);
    void toggleFloatTile(Client *c);
    void swapTiles(Tile *a, Tile *b);

    /**
     * All tiling layouts do not allow the user to manually
     * resize clients. This method will be called when the user
     * attempts a resize. Return any valid position to allow
     * resizing in that direction. currentMode will be the direction
     * of resize attempted by the user. You do not have to return the same value.
     * If you do not want to allow resizing at all, or you do not
     * want to allow resizing for this client, then return KDecorationDefines::PositionCenter.
     */
    virtual KDecorationDefines::Position resizeMode(Client *c, KDecorationDefines::Position currentMode) const;

    const QList<Tile *>& tiles() const;
    Tile* findTile(Client *c) const;

protected:
    /**
     * Get a pointer to the Workspace.
     */
    Workspace * workspace() const;
    /**
     * Get a area in which the Tile can be placed.
     */
    const QRect layoutArea(Tile *t) const;
    /**
     * Hooks called after a tile is added to
     * layout and before it is removed.
     */
    // currently only required by floating layout
    virtual void postAddTile(Tile *t);
    virtual void preRemoveTile(Tile *t);

protected Q_SLOTS:
    void reconfigureTiling();

private:
    int findTilePos(Client *c) const;
    virtual void arrange(QRect wgeom) = 0;

    void addTileNoArrange(Tile *t);
    void removeTileNoArrange(Tile *t);

    Tile* findTileBelowPoint(const QPoint &p) const;


    QList<Tile *> m_tiles;
    int m_layoutType;
    Workspace *m_workspace;


    friend class TilingLayoutFactory;
};

inline void TilingLayout::setLayoutType(int t)
{
    m_layoutType = t;
}

inline int TilingLayout::layoutType() const
{
    return m_layoutType;
}

inline const QList<Tile *>& TilingLayout::tiles() const
{
    return m_tiles;
}

inline Workspace* TilingLayout::workspace() const
{
    return m_workspace;
}

inline void TilingLayout::postAddTile(Tile *t)
{
    Q_UNUSED(t)
}

inline void TilingLayout::preRemoveTile(Tile *t)
{
    Q_UNUSED(t)
}

} // end namespace

#endif
