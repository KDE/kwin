/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2022 Marco Martin <mart@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "tile.h"
#include "utils/common.h"
#include <kwin_export.h>

namespace KWin
{

class VirtualDesktop;

class KWIN_EXPORT QuickRootTile : public Tile
{
    Q_OBJECT
public:
    QuickRootTile(TileManager *tiling, VirtualDesktop *desktop);
    ~QuickRootTile();

    Tile *tileForMode(QuickTileMode mode);
    Tile *tileForBorder(ElectricBorder border);

    qreal horizontalSplit() const;
    void setHorizontalSplit(qreal split);

    qreal verticalSplit() const;
    void setVerticalSplit(qreal split);

    Tile *tileForWindow(Window *window) const;

private:
    void relayoutToFit(Tile *tile);
    void tryReset();

    Tile *m_resizedTile = nullptr;

    Tile *m_leftVerticalTile = nullptr;
    Tile *m_rightVerticalTile = nullptr;

    Tile *m_topHorizontalTile = nullptr;
    Tile *m_bottomHorizontalTile = nullptr;

    Tile *m_topLeftTile = nullptr;
    Tile *m_topRightTile = nullptr;
    Tile *m_bottomLeftTile = nullptr;
    Tile *m_bottomRightTile = nullptr;
};

} // namespace KWin
