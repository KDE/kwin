/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2022 Marco Martin <mart@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "quicktile.h"

namespace KWin
{

QuickRootTile::QuickRootTile(TileManager *tiling, Tile *parentItem)
    : Tile(tiling, parentItem)
{
    setPadding(0.0);
    setRelativeGeometry(QRectF(0, 0, 1, 1));
    setQuickTileMode(QuickTileFlag::Maximize);

    auto createTile = [this, &tiling](const QRectF &geometry, QuickTileMode tileMode) {
        Tile *tile = new Tile(tiling, this);
        tile->setPadding(0.0);
        tile->setQuickTileMode(tileMode);
        tile->setRelativeGeometry(geometry);

        connect(tile, &Tile::relativeGeometryChanged, this, [this, tile]() {
            relayoutToFit(tile);
        });

        return std::unique_ptr<Tile>(tile);
    };

    m_leftVerticalTile = createTile(QRectF(0, 0, 0.5, 1), QuickTileFlag::Left);
    m_rightVerticalTile = createTile(QRectF(0.5, 0, 0.5, 1), QuickTileFlag::Right);
    m_topHorizontalTile = createTile(QRectF(0, 0, 1, 0.5), QuickTileFlag::Top);
    m_bottomHorizontalTile = createTile(QRectF(0, 0.5, 1, 0.5), QuickTileFlag::Bottom);

    m_topLeftTile = createTile(QRectF(0, 0, 0.5, 0.5), QuickTileFlag::Top | QuickTileFlag::Left);
    m_topRightTile = createTile(QRectF(0.5, 0, 0.5, 0.5), QuickTileFlag::Top | QuickTileFlag::Right);
    m_bottomLeftTile = createTile(QRectF(0, 0.5, 0.5, 0.5), QuickTileFlag::Bottom | QuickTileFlag::Left);
    m_bottomRightTile = createTile(QRectF(0.5, 0.5, 0.5, 0.5), QuickTileFlag::Bottom | QuickTileFlag::Right);
}

QuickRootTile::~QuickRootTile()
{
}

void QuickRootTile::relayoutToFit(Tile *tile)
{
    if (m_resizedTile) {
        return;
    }

    m_resizedTile = tile;

    const QRectF geometry = tile->relativeGeometry();

    if (m_topHorizontalTile.get() == tile) {
        setVerticalSplit(geometry.bottom());
    } else if (m_bottomHorizontalTile.get() == tile) {
        setVerticalSplit(geometry.top());
    } else if (m_leftVerticalTile.get() == tile) {
        setHorizontalSplit(geometry.right());
    } else if (m_rightVerticalTile.get() == tile) {
        setHorizontalSplit(geometry.left());
    } else if (m_topLeftTile.get() == tile) {
        setHorizontalSplit(geometry.right());
        setVerticalSplit(geometry.bottom());
    } else if (m_topRightTile.get() == tile) {
        setHorizontalSplit(geometry.left());
        setVerticalSplit(geometry.bottom());
    } else if (m_bottomRightTile.get() == tile) {
        setHorizontalSplit(geometry.left());
        setVerticalSplit(geometry.top());
    } else if (m_bottomLeftTile.get() == tile) {
        setHorizontalSplit(geometry.right());
        setVerticalSplit(geometry.top());
    }

    m_resizedTile = nullptr;
}

Tile *QuickRootTile::tileForMode(QuickTileMode mode)
{
    switch (mode) {
    case QuickTileMode(QuickTileFlag::Left):
        return m_leftVerticalTile.get();
    case QuickTileMode(QuickTileFlag::Right):
        return m_rightVerticalTile.get();
    case QuickTileMode(QuickTileFlag::Top):
        return m_topHorizontalTile.get();
    case QuickTileMode(QuickTileFlag::Bottom):
        return m_bottomHorizontalTile.get();
    case QuickTileMode(QuickTileFlag::Left | QuickTileFlag::Top):
        return m_topLeftTile.get();
    case QuickTileMode(QuickTileFlag::Right | QuickTileFlag::Top):
        return m_topRightTile.get();
    case QuickTileMode(QuickTileFlag::Left | QuickTileFlag::Bottom):
        return m_bottomLeftTile.get();
    case QuickTileMode(QuickTileFlag::Right | QuickTileFlag::Bottom):
        return m_bottomRightTile.get();
    case QuickTileMode(QuickTileFlag::Maximize):
    case QuickTileMode(QuickTileFlag::Horizontal):
    case QuickTileMode(QuickTileFlag::Vertical):
        return this;
    default:
        return nullptr;
    }
}

Tile *QuickRootTile::tileForBorder(ElectricBorder border)
{
    switch (border) {
    case ElectricTop:
        return m_topHorizontalTile.get();
    case ElectricTopRight:
        return m_topRightTile.get();
    case ElectricRight:
        return m_rightVerticalTile.get();
    case ElectricBottomRight:
        return m_bottomRightTile.get();
    case ElectricBottom:
        return m_bottomHorizontalTile.get();
    case ElectricBottomLeft:
        return m_bottomLeftTile.get();
    case ElectricLeft:
        return m_leftVerticalTile.get();
    case ElectricTopLeft:
        return m_topLeftTile.get();
    case ElectricNone:
    default:
        return nullptr;
    }
}

qreal QuickRootTile::horizontalSplit() const
{
    return m_leftVerticalTile->relativeGeometry().right();
}

void QuickRootTile::setHorizontalSplit(qreal split)
{
    const QSizeF minSize = minimumSize(); // minimum size is the same for all tiles
    const qreal effectiveSplit = std::clamp(split, minSize.width(), 1.0 - minSize.width());

    auto geom = m_leftVerticalTile->relativeGeometry();
    geom.setRight(effectiveSplit);
    m_leftVerticalTile->setRelativeGeometry(geom);

    geom = m_rightVerticalTile->relativeGeometry();
    geom.setLeft(effectiveSplit);
    m_rightVerticalTile->setRelativeGeometry(geom);

    geom = m_topLeftTile->relativeGeometry();
    geom.setRight(effectiveSplit);
    m_topLeftTile->setRelativeGeometry(geom);

    geom = m_topRightTile->relativeGeometry();
    geom.setLeft(effectiveSplit);
    m_topRightTile->setRelativeGeometry(geom);

    geom = m_bottomLeftTile->relativeGeometry();
    geom.setRight(effectiveSplit);
    m_bottomLeftTile->setRelativeGeometry(geom);

    geom = m_bottomRightTile->relativeGeometry();
    geom.setLeft(effectiveSplit);
    m_bottomRightTile->setRelativeGeometry(geom);
}

qreal QuickRootTile::verticalSplit() const
{
    return m_topHorizontalTile->relativeGeometry().bottom();
}

void QuickRootTile::setVerticalSplit(qreal split)
{
    const QSizeF minSize = minimumSize(); // minimum size is the same for all tiles
    const qreal effectiveSplit = std::clamp(split, minSize.height(), 1.0 - minSize.height());

    auto geom = m_topHorizontalTile->relativeGeometry();
    geom.setBottom(effectiveSplit);
    m_topHorizontalTile->setRelativeGeometry(geom);

    geom = m_bottomHorizontalTile->relativeGeometry();
    geom.setTop(effectiveSplit);
    m_bottomHorizontalTile->setRelativeGeometry(geom);

    geom = m_topLeftTile->relativeGeometry();
    geom.setBottom(effectiveSplit);
    m_topLeftTile->setRelativeGeometry(geom);

    geom = m_topRightTile->relativeGeometry();
    geom.setBottom(effectiveSplit);
    m_topRightTile->setRelativeGeometry(geom);

    geom = m_bottomLeftTile->relativeGeometry();
    geom.setTop(effectiveSplit);
    m_bottomLeftTile->setRelativeGeometry(geom);

    geom = m_bottomRightTile->relativeGeometry();
    geom.setTop(effectiveSplit);
    m_bottomRightTile->setRelativeGeometry(geom);
}

} // namespace KWin
