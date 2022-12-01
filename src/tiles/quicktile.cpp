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

    m_leftVerticalTile = std::make_unique<QuickTile>(tiling, ElectricBorder::ElectricLeft, this);
    m_rightVerticalTile = std::unique_ptr<QuickTile>(new QuickTile(tiling, ElectricBorder::ElectricRight, this));

    m_topHorizontalTile = std::unique_ptr<QuickTile>(new QuickTile(tiling, ElectricBorder::ElectricTop, this));
    m_bottomHorizontalTile = std::unique_ptr<QuickTile>(new QuickTile(tiling, ElectricBorder::ElectricBottom, this));

    m_topLeftTile = std::unique_ptr<QuickTile>(new QuickTile(tiling, ElectricBorder::ElectricTopLeft, this));
    m_topRightTile = std::unique_ptr<QuickTile>(new QuickTile(tiling, ElectricBorder::ElectricTopRight, this));
    m_bottomLeftTile = std::unique_ptr<QuickTile>(new QuickTile(tiling, ElectricBorder::ElectricBottomLeft, this));
    m_bottomRightTile = std::unique_ptr<QuickTile>(new QuickTile(tiling, ElectricBorder::ElectricBottomRight, this));
}

QuickRootTile::~QuickRootTile()
{
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
    auto geom = m_leftVerticalTile->relativeGeometry();
    geom.setRight(split);
    m_leftVerticalTile->setRelativeGeometry(geom);

    geom = m_rightVerticalTile->relativeGeometry();
    geom.setLeft(split);
    m_rightVerticalTile->setRelativeGeometry(geom);

    geom = m_topLeftTile->relativeGeometry();
    geom.setRight(split);
    m_topLeftTile->setRelativeGeometry(geom);

    geom = m_topRightTile->relativeGeometry();
    geom.setLeft(split);
    m_topRightTile->setRelativeGeometry(geom);

    geom = m_bottomLeftTile->relativeGeometry();
    geom.setRight(split);
    m_bottomLeftTile->setRelativeGeometry(geom);

    geom = m_bottomRightTile->relativeGeometry();
    geom.setLeft(split);
    m_bottomRightTile->setRelativeGeometry(geom);
}

qreal QuickRootTile::verticalSplit() const
{
    return m_topHorizontalTile->relativeGeometry().bottom();
}

void QuickRootTile::setVerticalSplit(qreal split)
{
    auto geom = m_topHorizontalTile->relativeGeometry();
    geom.setBottom(split);
    m_topHorizontalTile->setRelativeGeometry(geom);

    geom = m_bottomHorizontalTile->relativeGeometry();
    geom.setTop(split);
    m_bottomHorizontalTile->setRelativeGeometry(geom);

    geom = m_topLeftTile->relativeGeometry();
    geom.setBottom(split);
    m_topLeftTile->setRelativeGeometry(geom);

    geom = m_topRightTile->relativeGeometry();
    geom.setBottom(split);
    m_topRightTile->setRelativeGeometry(geom);

    geom = m_bottomLeftTile->relativeGeometry();
    geom.setTop(split);
    m_bottomLeftTile->setRelativeGeometry(geom);

    geom = m_bottomRightTile->relativeGeometry();
    geom.setTop(split);
    m_bottomRightTile->setRelativeGeometry(geom);
}

QuickTile::QuickTile(TileManager *tiling, ElectricBorder border, QuickRootTile *parentItem)
    : Tile(tiling, parentItem)
    , m_root(parentItem)
    , m_border(border)
{
    m_geometryLock = true;
    setPadding(0);
    switch (border) {
    case ElectricTop:
        setQuickTileMode(QuickTileFlag::Top);
        setRelativeGeometry(QRectF(0, 0, 1, 0.5));
        break;
    case ElectricTopRight:
        setQuickTileMode(QuickTileFlag::Top | QuickTileFlag::Right);
        setRelativeGeometry(QRectF(0.5, 0, 0.5, 0.5));
        break;
    case ElectricRight:
        setQuickTileMode(QuickTileFlag::Right);
        setRelativeGeometry(QRectF(0.5, 0, 0.5, 1));
        break;
    case ElectricBottomRight:
        setQuickTileMode(QuickTileFlag::Bottom | QuickTileFlag::Right);
        setRelativeGeometry(QRectF(0.5, 0.5, 0.5, 0.5));
        break;
    case ElectricBottom:
        setQuickTileMode(QuickTileFlag::Bottom);
        setRelativeGeometry(QRectF(0, 0.5, 1, 0.5));
        break;
    case ElectricBottomLeft:
        setQuickTileMode(QuickTileFlag::Bottom | QuickTileFlag::Left);
        setRelativeGeometry(QRectF(0, 0.5, 0.5, 0.5));
        break;
    case ElectricLeft:
        setQuickTileMode(QuickTileFlag::Left);
        setRelativeGeometry(QRectF(0, 0, 0.5, 1));
        break;
    case ElectricTopLeft:
        setQuickTileMode(QuickTileFlag::Top | QuickTileFlag::Left);
        setRelativeGeometry(QRectF(0, 0, 0.5, 0.5));
        break;
    case ElectricNone:
        setQuickTileMode(QuickTileFlag::None);
    default:
        break;
    }
}

QuickTile::~QuickTile()
{
}

void QuickTile::setRelativeGeometry(const QRectF &geom)
{
    if (relativeGeometry() == geom) {
        return;
    }

    if (!m_geometryLock) {
        m_geometryLock = true;
        switch (m_border) {
        case ElectricTop:
            m_root->setVerticalSplit(geom.bottom());
            break;
        case ElectricTopRight:
            m_root->setHorizontalSplit(geom.left());
            m_root->setVerticalSplit(geom.bottom());
            break;
        case ElectricRight:
            m_root->setHorizontalSplit(geom.left());
            break;
        case ElectricBottomRight:
            m_root->setHorizontalSplit(geom.left());
            m_root->setVerticalSplit(geom.top());
            break;
        case ElectricBottom:
            m_root->setVerticalSplit(geom.top());
            break;
        case ElectricBottomLeft:
            m_root->setHorizontalSplit(geom.right());
            m_root->setVerticalSplit(geom.top());
            break;
        case ElectricLeft:
            m_root->setHorizontalSplit(geom.right());
            break;
        case ElectricTopLeft:
            m_root->setHorizontalSplit(geom.right());
            m_root->setVerticalSplit(geom.bottom());
            break;
        case ElectricNone:
        default:
            break;
        }
    }

    Tile::setRelativeGeometry(geom);
    m_geometryLock = false;
}

} // namespace KWin
