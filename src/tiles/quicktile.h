/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2022 Marco Martin <mart@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef QUICKTILE_H
#define QUICKTILE_H

#include "tile.h"
#include "utils/common.h"
#include <kwin_export.h>

namespace KWin
{

class QuickRootTile;

class KWIN_EXPORT QuickTile : public Tile
{
    Q_OBJECT
public:
    QuickTile(TileManager *tiling, ElectricBorder border, QuickRootTile *parentItem = nullptr);
    ~QuickTile();

    void setRelativeGeometry(const QRectF &geom) override;

private:
    QuickRootTile *m_root = nullptr;
    ElectricBorder m_border;
    bool m_geometryLock = false;
};

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
    std::unique_ptr<QuickTile> m_leftVerticalTile;
    std::unique_ptr<QuickTile> m_rightVerticalTile;

    std::unique_ptr<QuickTile> m_topHorizontalTile;
    std::unique_ptr<QuickTile> m_bottomHorizontalTile;

    std::unique_ptr<QuickTile> m_topLeftTile;
    std::unique_ptr<QuickTile> m_topRightTile;
    std::unique_ptr<QuickTile> m_bottomLeftTile;
    std::unique_ptr<QuickTile> m_bottomRightTile;
};

} // namespace KWin

#endif
