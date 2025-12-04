/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2022 Marco Martin <mart@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "quicktile.h"
#include "tilemanager.h"
#include "virtualdesktops.h"

namespace KWin
{

QuickRootTile::QuickRootTile(TileManager *tiling, VirtualDesktop *desktop)
    : Tile(tiling, nullptr)
{
    m_desktop = desktop;
    setParent(tiling);
    setPadding(0.0);
    setRelativeGeometry(RectF(0, 0, 1, 1));
    setQuickTileMode(QuickTileFlag::None);

    auto createTile = [this](const RectF &geometry, QuickTileMode tileMode) {
        Tile *tile = createChildAt<Tile>(geometry, childCount());
        tile->setPadding(0.0);
        tile->setQuickTileMode(tileMode);

        connect(tile, &Tile::relativeGeometryChanged, this, [this, tile]() {
            relayoutToFit(tile);
        });
        connect(tile, &Tile::windowRemoved, this, &QuickRootTile::tryReset);

        return tile;
    };

    m_leftVerticalTile = createTile(RectF(0, 0, 0.5, 1), QuickTileFlag::Left);
    m_rightVerticalTile = createTile(RectF(0.5, 0, 0.5, 1), QuickTileFlag::Right);
    m_topHorizontalTile = createTile(RectF(0, 0, 1, 0.5), QuickTileFlag::Top);
    m_bottomHorizontalTile = createTile(RectF(0, 0.5, 1, 0.5), QuickTileFlag::Bottom);

    m_topLeftTile = createTile(RectF(0, 0, 0.5, 0.5), QuickTileFlag::Top | QuickTileFlag::Left);
    m_topRightTile = createTile(RectF(0.5, 0, 0.5, 0.5), QuickTileFlag::Top | QuickTileFlag::Right);
    m_bottomLeftTile = createTile(RectF(0, 0.5, 0.5, 0.5), QuickTileFlag::Bottom | QuickTileFlag::Left);
    m_bottomRightTile = createTile(RectF(0.5, 0.5, 0.5, 0.5), QuickTileFlag::Bottom | QuickTileFlag::Right);
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

    const RectF geometry = tile->relativeGeometry();

    if (m_topHorizontalTile == tile) {
        setVerticalSplit(geometry.bottom());
    } else if (m_bottomHorizontalTile == tile) {
        setVerticalSplit(geometry.top());
    } else if (m_leftVerticalTile == tile) {
        setHorizontalSplit(geometry.right());
    } else if (m_rightVerticalTile == tile) {
        setHorizontalSplit(geometry.left());
    } else if (m_topLeftTile == tile) {
        setHorizontalSplit(geometry.right());
        setVerticalSplit(geometry.bottom());
    } else if (m_topRightTile == tile) {
        setHorizontalSplit(geometry.left());
        setVerticalSplit(geometry.bottom());
    } else if (m_bottomRightTile == tile) {
        setHorizontalSplit(geometry.left());
        setVerticalSplit(geometry.top());
    } else if (m_bottomLeftTile == tile) {
        setHorizontalSplit(geometry.right());
        setVerticalSplit(geometry.top());
    }

    m_resizedTile = nullptr;
}

Tile *QuickRootTile::tileForMode(QuickTileMode mode)
{
    switch (mode) {
    case QuickTileMode(QuickTileFlag::Left):
        return m_leftVerticalTile;
    case QuickTileMode(QuickTileFlag::Right):
        return m_rightVerticalTile;
    case QuickTileMode(QuickTileFlag::Top):
        return m_topHorizontalTile;
    case QuickTileMode(QuickTileFlag::Bottom):
        return m_bottomHorizontalTile;
    case QuickTileMode(QuickTileFlag::Left | QuickTileFlag::Top):
        return m_topLeftTile;
    case QuickTileMode(QuickTileFlag::Right | QuickTileFlag::Top):
        return m_topRightTile;
    case QuickTileMode(QuickTileFlag::Left | QuickTileFlag::Bottom):
        return m_bottomLeftTile;
    case QuickTileMode(QuickTileFlag::Right | QuickTileFlag::Bottom):
        return m_bottomRightTile;
    default:
        return nullptr;
    }
}

Tile *QuickRootTile::tileForBorder(ElectricBorder border)
{
    switch (border) {
    case ElectricTop:
        return m_topHorizontalTile;
    case ElectricTopRight:
        return m_topRightTile;
    case ElectricRight:
        return m_rightVerticalTile;
    case ElectricBottomRight:
        return m_bottomRightTile;
    case ElectricBottom:
        return m_bottomHorizontalTile;
    case ElectricBottomLeft:
        return m_bottomLeftTile;
    case ElectricLeft:
        return m_leftVerticalTile;
    case ElectricTopLeft:
        return m_topLeftTile;
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

void QuickRootTile::tryReset()
{
    if (!m_topLeftTile->windows().isEmpty()) {
        return;
    }

    if (!m_topRightTile->windows().isEmpty()) {
        return;
    }

    if (!m_bottomLeftTile->windows().isEmpty()) {
        return;
    }

    if (!m_bottomRightTile->windows().isEmpty()) {
        return;
    }

    if (m_leftVerticalTile->windows().isEmpty() && m_rightVerticalTile->windows().isEmpty()) {
        setHorizontalSplit(0.5);
    }

    if (m_topHorizontalTile->windows().isEmpty() && m_bottomHorizontalTile->windows().isEmpty()) {
        setVerticalSplit(0.5);
    }
}

Tile *QuickRootTile::tileForWindow(Window *window) const
{
    Tile *allTiles[] = {m_leftVerticalTile,
                        m_rightVerticalTile,
                        m_topHorizontalTile,
                        m_bottomHorizontalTile,
                        m_topLeftTile,
                        m_topRightTile,
                        m_bottomLeftTile,
                        m_bottomRightTile};

    for (Tile *tile : allTiles) {
        if (tile->windows().contains(window)) {
            return tile;
        }
    }
    return nullptr;
}

} // namespace KWin

#include "moc_quicktile.cpp"
