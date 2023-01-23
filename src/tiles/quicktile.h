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

class KWIN_EXPORT QuickRootTile : public Tile
{
    Q_OBJECT
public:
    QuickRootTile(TileManager *tiling, Tile *parentItem = nullptr);
    ~QuickRootTile();

    Tile *tileForMode(QuickTileMode mode);
    Tile *tileForBorder(ElectricBorder border);

    qreal horizontalSplit() const;
    void setHorizontalSplit(qreal split);

    qreal verticalSplit() const;
    void setVerticalSplit(qreal split);

private:
    void relayoutToFit(Tile *tile);

    Tile *m_resizedTile = nullptr;

    std::unique_ptr<Tile> m_leftVerticalTile;
    std::unique_ptr<Tile> m_rightVerticalTile;

    std::unique_ptr<Tile> m_topHorizontalTile;
    std::unique_ptr<Tile> m_bottomHorizontalTile;

    std::unique_ptr<Tile> m_topLeftTile;
    std::unique_ptr<Tile> m_topRightTile;
    std::unique_ptr<Tile> m_bottomLeftTile;
    std::unique_ptr<Tile> m_bottomRightTile;
};

} // namespace KWin
