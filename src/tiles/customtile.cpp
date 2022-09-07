/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2022 Marco Martin <mart@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "customtile.h"
#include "core/output.h"
#include "tilemanager.h"

namespace KWin
{

QDebug operator<<(QDebug debug, const CustomTile *tile)
{
    QDebugStateSaver saver(debug);
    debug.nospace();
    if (tile) {
        debug << tile->metaObject()->className() << '(' << static_cast<const void *>(tile);
        debug << tile->relativeGeometry() << tile->layoutDirection();
        debug << ')';
    } else {
        debug << "Tile(0x0)";
    }
    return debug;
}

CustomTile::CustomTile(TileManager *tiling, CustomTile *parentItem)
    : Tile(tiling, parentItem)
{
    m_geometryLock = true;
    connect(this, &CustomTile::layoutModified, parentItem, &CustomTile::layoutModified);
    connect(this, &CustomTile::isMaximizeAreaChanged, parentItem, &CustomTile::layoutModified);
}

CustomTile::~CustomTile()
{
    auto *pt = static_cast<CustomTile *>(parentTile());

    // If was a floating layout, put it again in the mode of its parent
    if (pt && pt->childCount() == 0 && pt->layoutDirection() == CustomTile::LayoutDirection::Floating) {
        pt->setLayoutDirection(CustomTile::LayoutDirection::Horizontal);
    }
}

CustomTile *CustomTile::createChild(const QRectF &relativeGeometry, LayoutDirection layoutDirection, int position)
{
    auto *tile = Tile::createChild<CustomTile>(relativeGeometry, position);
    tile->setLayoutDirection(layoutDirection);
    return tile;
}

void CustomTile::setRelativeGeometry(const QRectF &geom)
{
    if (relativeGeometry() == geom) {
        return;
    }

    QRectF finalGeom = geom.intersected(QRectF(0, 0, 1, 1));
    finalGeom.setWidth(qMax(finalGeom.width(), minimumSize().width()));
    finalGeom.setHeight(qMax(finalGeom.height(), minimumSize().height()));
    // We couldn't set the minimum size and still remain in boundaries, do nothing
    if (finalGeom.right() > 1 || finalGeom.bottom() > 1) {
        return;
    }

    auto *parentT = static_cast<CustomTile *>(parentTile());

    if (!m_geometryLock && parentT && parentT->layoutDirection() != LayoutDirection::Floating) {
        m_geometryLock = true;
        if (finalGeom.left() != relativeGeometry().left()) {
            auto *tile = nextTileAt(Qt::LeftEdge);
            if (tile) {
                auto tileGeom = tile->relativeGeometry();
                tileGeom.setRight(finalGeom.left());
                tile->setRelativeGeometry(tileGeom);
                // The other tile gometry may be not what we set due to size constraints
                finalGeom.setLeft(tile->relativeGeometry().right());
            } else {
                // We are at the left border of the screen, we are always at 0
                finalGeom.setLeft(0);
            }
        }
        if (finalGeom.top() != relativeGeometry().top()) {
            auto *tile = nextTileAt(Qt::TopEdge);
            if (tile) {
                auto tileGeom = tile->relativeGeometry();
                tileGeom.setBottom(finalGeom.top());
                tile->setRelativeGeometry(tileGeom);
                finalGeom.setTop(tile->relativeGeometry().bottom());
            } else {
                // We are at the top border of the screen, we are always at 0
                finalGeom.setTop(0);
            }
        }
        if (finalGeom.right() != relativeGeometry().right()) {
            auto *tile = nextTileAt(Qt::RightEdge);
            if (tile) {
                auto tileGeom = tile->relativeGeometry();
                tileGeom.setLeft(finalGeom.right());
                tile->setRelativeGeometry(tileGeom);
                finalGeom.setRight(tile->relativeGeometry().left());
            } else {
                // We are at the right border of the screen, we are always at 1
                finalGeom.setRight(1);
            }
        }
        if (finalGeom.bottom() != relativeGeometry().bottom()) {
            auto *tile = nextTileAt(Qt::BottomEdge);
            if (tile) {
                auto tileGeom = tile->relativeGeometry();
                tileGeom.setTop(finalGeom.bottom());
                tile->setRelativeGeometry(tileGeom);
                finalGeom.setBottom(tile->relativeGeometry().top());
            } else {
                // We are at the bottom border of the screen, we are always at 1
                finalGeom.setBottom(1);
            }
        }
        m_geometryLock = false;
    } else if (parentT && parentT->layoutDirection() == LayoutDirection::Floating) {
        finalGeom = geom.intersected(parentT->relativeGeometry());
    }

    auto childrenT = childTiles();

    // Resize all the children to fit in new size
    for (auto t : childrenT) {
        auto childGeom = t->relativeGeometry();
        childGeom = childGeom.intersected(finalGeom);
        if (layoutDirection() == LayoutDirection::Horizontal) {
            childGeom.setTop(finalGeom.top());
            childGeom.setBottom(finalGeom.bottom());
        } else if (layoutDirection() == LayoutDirection::Vertical) {
            childGeom.setLeft(finalGeom.left());
            childGeom.setRight(finalGeom.right());
        }

        t->setRelativeGeometry(childGeom);
    }
    // Make sure the last child item fills the area
    // TODO: ensure all children don't go below minimum size/resize all children
    if (!m_geometryLock && !childrenT.isEmpty() && layoutDirection() != LayoutDirection::Floating) {
        auto childGeom = childrenT.last()->relativeGeometry();
        childGeom.setRight(finalGeom.right());
        childGeom.setBottom(finalGeom.bottom());
        childrenT.last()->setRelativeGeometry(childGeom);
    }

    Tile::setRelativeGeometry(finalGeom);
    if (parentT) {
        Q_EMIT parentT->layoutModified();
    }
    m_geometryLock = false;
}

bool CustomTile::supportsResizeGravity(KWin::Gravity gravity)
{
    if (layoutDirection() == LayoutDirection::Floating) {
        return true;
    }

    return Tile::supportsResizeGravity(gravity);
}

void CustomTile::split(KWin::CustomTile::LayoutDirection newDirection)
{
    auto *parentT = static_cast<CustomTile *>(parentTile());

    // If we are m_rootLayoutTile always create childrens, not siblings
    if (parentT && (parentT->childCount() < 2 || parentT->layoutDirection() == newDirection)) {
        // Add a new cell to the current layout
        setLayoutDirection(newDirection);
        QRectF newGeo;
        if (newDirection == LayoutDirection::Floating) {
            newGeo = QRectF(relativeGeometry().left() + 0.1, relativeGeometry().top() + 0.1, 0.3, 0.2);
            newGeo.setLeft(qMax(newGeo.left(), parentT->relativeGeometry().left()));
            newGeo.setTop(qMax(newGeo.top(), parentT->relativeGeometry().top()));
            newGeo.setRight(qMin(newGeo.right(), parentT->relativeGeometry().right()));
            newGeo.setBottom(qMin(newGeo.bottom(), parentT->relativeGeometry().bottom()));
        } else if (newDirection == LayoutDirection::Horizontal) {
            newGeo = relativeGeometry();
            newGeo.setWidth(relativeGeometry().width() / 2);
            Tile::setRelativeGeometry(newGeo);
            newGeo.moveLeft(newGeo.x() + newGeo.width());
        } else if (newDirection == LayoutDirection::Vertical) {
            newGeo = relativeGeometry();
            newGeo.setHeight(relativeGeometry().height() / 2);
            Tile::setRelativeGeometry(newGeo);
            newGeo.moveTop(newGeo.y() + newGeo.height());
        }

        manager()->addTile(newGeo, layoutDirection(), row() + 1, parentT);
    } else {
        // Do a new layout and put tiles inside
        setLayoutDirection(newDirection);
        auto newGeo = relativeGeometry();
        if (newDirection == LayoutDirection::Floating) {
            // Do a new layout with one floating tile inside
            auto startGeom = relativeGeometry();
            if (!childTiles().isEmpty()) {
                startGeom = childTiles().last()->relativeGeometry();
            }
            newGeo = QRectF(startGeom.left() + 0.05, startGeom.top() + 0.05, 0.3, 0.25);
            newGeo.setLeft(qMax(newGeo.left(), relativeGeometry().left()));
            newGeo.setTop(qMax(newGeo.top(), relativeGeometry().top()));
            newGeo.setRight(qMin(newGeo.right(), relativeGeometry().right()));
            newGeo.setBottom(qMin(newGeo.bottom(), relativeGeometry().bottom()));
            manager()->addTile(newGeo, newDirection, -1, this);
        } else if (newDirection == LayoutDirection::Horizontal) {
            // Do a new layout with 2 cells inside this one
            newGeo.setWidth(relativeGeometry().width() / 2);
            manager()->addTile(newGeo, newDirection, -1, this);
            newGeo.moveLeft(newGeo.x() + newGeo.width());
            manager()->addTile(newGeo, newDirection, -1, this);
        } else if (newDirection == LayoutDirection::Vertical) {
            // Do a new layout with 2 cells inside this one
            newGeo.setHeight(relativeGeometry().height() / 2);
            manager()->addTile(newGeo, newDirection, -1, this);
            newGeo.moveTop(newGeo.y() + newGeo.height());
            manager()->addTile(newGeo, newDirection, -1, this);
        }
    }
}

void CustomTile::moveByPixels(const QPointF &delta)
{
    if (static_cast<CustomTile *>(parentTile())->layoutDirection() != LayoutDirection::Floating) {
        return;
    }

    const auto outGeom = manager()->output()->geometry();
    const auto relativeMove = QPointF(delta.x() / outGeom.width(), delta.y() / outGeom.height());
    auto geom = relativeGeometry();
    geom.moveTo(geom.topLeft() + relativeMove);

    setRelativeGeometry(geom);
}

void CustomTile::remove()
{
    auto *parentT = static_cast<CustomTile *>(parentTile());
    if (!parentT) {
        return;
    }

    auto *prev = previousSibling();
    auto *next = nextSibling();

    manager()->removeTile(this);

    if (parentT->layoutDirection() == LayoutDirection::Horizontal) {
        if (prev && next) {
            auto geom = prev->relativeGeometry();
            geom.setRight(relativeGeometry().center().x());
            prev->setRelativeGeometry(geom);
            geom = next->relativeGeometry();
            geom.setLeft(relativeGeometry().center().x());
            next->setRelativeGeometry(geom);
        } else if (prev) {
            auto geom = prev->relativeGeometry();
            geom.setRight(relativeGeometry().right());
            prev->setRelativeGeometry(geom);
        } else if (next) {
            auto geom = next->relativeGeometry();
            geom.setLeft(relativeGeometry().left());
            next->setRelativeGeometry(geom);
        }
    } else if (parentT->layoutDirection() == LayoutDirection::Vertical) {
        if (prev && next) {
            auto geom = prev->relativeGeometry();
            geom.setBottom(relativeGeometry().center().y());
            prev->setRelativeGeometry(geom);
            geom = next->relativeGeometry();
            geom.setTop(relativeGeometry().center().y());
            next->setRelativeGeometry(geom);
        } else if (prev) {
            auto geom = prev->relativeGeometry();
            geom.setBottom(relativeGeometry().bottom());
            prev->setRelativeGeometry(geom);
        } else if (next) {
            auto geom = next->relativeGeometry();
            geom.setTop(relativeGeometry().top());
            next->setRelativeGeometry(geom);
        }
    }
}

CustomTile *CustomTile::nextTileAt(Qt::Edge edge) const
{
    auto *parentT = static_cast<CustomTile *>(parentTile());

    // TODO: implement geometry base searching for floating?
    if (!parentT || parentT->layoutDirection() == LayoutDirection::Floating) {
        return nullptr;
    }

    Tile *sibling = nullptr;

    const int index = row();
    int layoutRows;
    int layoutColumns;
    switch (parentT->layoutDirection()) {
    case LayoutDirection::Vertical:
        layoutRows = qMax(1, parentT->childCount());
        layoutColumns = 1;
        break;
    case LayoutDirection::Horizontal:
    default:
        layoutColumns = qMax(1, parentT->childCount());
        layoutRows = 1;
        break;
    }
    int row = index / layoutColumns;
    int column = index % layoutColumns;

    switch (edge) {
    case Qt::LeftEdge:
        if (column > 0) {
            sibling = previousSibling();
        }
        break;
    case Qt::TopEdge:
        if (row > 0) {
            sibling = parentT->childTiles()[layoutColumns * (row - 1) + column];
        }
        break;
    case Qt::RightEdge:
        if (column < layoutColumns - 1) {
            sibling = nextSibling();
        }
        break;
    case Qt::BottomEdge:
        if (row < layoutRows - 1) {
            const int newIndex = layoutColumns * (row + 1) + column;
            if (newIndex < parentT->childCount()) {
                sibling = parentT->childTiles()[newIndex];
            }
        }
        break;
    }

    if (sibling) {
        return static_cast<CustomTile *>(sibling);
    } else {
        return parentT->nextTileAt(edge);
    }
}

void CustomTile::setLayoutDirection(CustomTile::LayoutDirection dir)
{
    if (m_layoutDirection == dir) {
        return;
    }

    m_layoutDirection = dir;
    Q_EMIT layoutDirectionChanged(dir);
}

CustomTile::LayoutDirection CustomTile::layoutDirection() const
{
    return m_layoutDirection;
}

RootTile::RootTile(TileManager *tiling)
    : CustomTile(tiling, nullptr)
{
    setParent(tiling);
    setRelativeGeometry({0, 0, 1, 1});
}

RootTile::~RootTile()
{
}

} // namespace KWin

#include "moc_customtile.cpp"
