/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2009 Nikhil Marathe <nsm.nikhil@gmail.com>
Copyright (C) 2011 Arthur Arlt <a.arlt@stud.uni-heidelberg.de>

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

#ifndef KWIN_TILING_H
#define KWIN_TILING_H

#include <kdecoration.h>

#include "client.h"

#include <QtCore/QVector>

namespace KWin {

class Tile;
class TilingLayout;

class Tiling : public QObject {
    Q_OBJECT
public:
    Tiling(Workspace *w);
    ~Tiling();
    bool isEnabled() const;
    void setEnabled(bool tiling);
    bool tileable(Client *c);
    const QVector< TilingLayout* >& tilingLayouts() const;
    void initShortcuts(KActionCollection* keys);

    // The notification functions are called from
    // various points in existing code so that
    // tiling can take any action if required.
    // They are defined in tiling.cpp
    void notifyTilingWindowResize(Client *c, const QRect &moveResizeGeom, const QRect &orig);
    void notifyTilingWindowMove(Client *c, const QRect &moveResizeGeom, const QRect &orig);
    void notifyTilingWindowResizeDone(Client *c, const QRect &moveResizeGeom, const QRect &orig, bool canceled);
    void notifyTilingWindowMoveDone(Client *c, const QRect &moveResizeGeom, const QRect &orig, bool canceled);
    void notifyTilingWindowDesktopChanged(Client *c, int old_desktop);
    void notifyTilingWindowMaximized(Client *c, KDecorationDefines::WindowOperation op);

    KDecorationDefines::Position supportedTilingResizeMode(Client *c, KDecorationDefines::Position currentMode);

public Q_SLOTS:
    void createTile(KWin::Client *c);
    void removeTile(KWin::Client *c);
    // user actions, usually bound to shortcuts
    // and also provided through the D-BUS interface.
    void slotToggleTiling();
    void slotToggleFloating();
    void slotNextTileLayout();
    void slotPreviousTileLayout();

    // Changes the focused client
    void slotFocusTileLeft();
    void slotFocusTileRight();
    void slotFocusTileTop();
    void slotFocusTileBottom();
    // swaps active and adjacent client.
    void slotMoveTileLeft();
    void slotMoveTileRight();
    void slotMoveTileTop();
    void slotMoveTileBottom();
    void belowCursor();

    // NOTE: debug method
    void dumpTiles() const;

    void notifyTilingWindowActivated(KWin::Client *c);
private:
    // try to get a decent tile, either the one with
    // focus or the one below the mouse.
    Tile* getNiceTile() const;
    // int, and not Tile::Direction because
    // we are using a forward declaration for Tile
    Tile* findAdjacentTile(Tile *ref, int d);
    void focusTile(int d);
    void moveTile(int d);

    Workspace* m_workspace;
    bool m_enabled;
    // Each tilingLayout is for one virtual desktop.
    // The length is always one more than the number of
    // virtual desktops so that we can quickly index them
    // without having to remember to subtract one.
    QVector<TilingLayout *> m_tilingLayouts;
private Q_SLOTS:
    void slotResizeTilingLayouts();
    void notifyTilingWindowMinimizeToggled(KWin::Client *c);
    // updates geometry of tiles on all desktops,
    // this rearranges the tiles.
    void updateAllTiles();
};
}

#endif // KWIN_TILING_H
