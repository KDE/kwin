/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2022 Marco Martin <mart@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "customtile.h"
#include "core/output.h"
#include "tilemanager.h"
#include "window.h"

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
    setQuickTileMode(QuickTileFlag::Custom);
    m_geometryLock = true;
}

CustomTile *CustomTile::createChildAt(const QRectF &relativeGeometry, LayoutDirection layoutDirection, int position)
{
    CustomTile *tile = new CustomTile(manager(), this);
    connect(tile, &CustomTile::layoutModified, this, &CustomTile::layoutModified);
    tile->setRelativeGeometry(relativeGeometry);
    tile->setLayoutDirection(layoutDirection);
    manager()->model()->beginInsertTile(tile, position);
    insertChild(position, tile);
    manager()->model()->endInsertTile();
    return tile;
}

void CustomTile::setRelativeGeometry(const QRectF &geom)
{
    if (relativeGeometry() == geom) {
        return;
    }

    QRectF finalGeom = geom.intersected(QRectF(0, 0, 1, 1));
    finalGeom.setWidth(std::max(finalGeom.width(), minimumSize().width()));
    finalGeom.setHeight(std::max(finalGeom.height(), minimumSize().height()));
    // We couldn't set the minimum size and still remain in boundaries, do nothing
    if (finalGeom.right() > 1 || finalGeom.bottom() > 1) {
        return;
    }

    auto *parentT = static_cast<CustomTile *>(parentTile());

    if (!m_geometryLock && parentT && parentT->layoutDirection() != LayoutDirection::Floating) {
        m_geometryLock = true;
        if (finalGeom.left() != relativeGeometry().left()) {
            Tile *tile = nextTileAt(Qt::LeftEdge);
            if (tile) {
                QRectF tileGeom = tile->relativeGeometry();
                tileGeom.setRight(finalGeom.left());
                if (tileGeom.width() <= tile->minimumSize().width()) {
                    m_geometryLock = false;
                    return;
                }
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
                if (tileGeom.height() <= tile->minimumSize().height()) {
                    m_geometryLock = false;
                    return;
                }
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
                if (tileGeom.width() <= tile->minimumSize().width()) {
                    m_geometryLock = false;
                    return;
                }
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
                if (tileGeom.height() <= tile->minimumSize().height()) {
                    m_geometryLock = false;
                    return;
                }
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

    const auto childrenT = childTiles();

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

    if (!childrenT.isEmpty()) {
        auto childGeom = childrenT.first()->relativeGeometry();
        if (layoutDirection() == LayoutDirection::Horizontal) {
            childGeom.setLeft(finalGeom.left());
        } else if (layoutDirection() == LayoutDirection::Vertical) {
            childGeom.setTop(finalGeom.top());
        }
        childrenT.first()->setRelativeGeometry(childGeom);
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
        return gravity != Gravity::None;
    }

    return Tile::supportsResizeGravity(gravity);
}

QList<CustomTile *> CustomTile::split(KWin::Tile::LayoutDirection newDirection)
{
    auto *parentT = static_cast<CustomTile *>(parentTile());

    QList<CustomTile *> splitTiles;

    // If we are m_rootLayoutTile always create childrens, not siblings
    if (parentT && (parentT->childCount() < 2 || parentT->layoutDirection() == newDirection)) {
        // Add a new cell to the current layout
        setLayoutDirection(newDirection);
        QRectF newGeo;
        if (newDirection == LayoutDirection::Floating) {
            newGeo = QRectF(relativeGeometry().left() + 0.1, relativeGeometry().top() + 0.1, 0.3, 0.2);
            newGeo.setLeft(std::max(newGeo.left(), parentT->relativeGeometry().left()));
            newGeo.setTop(std::max(newGeo.top(), parentT->relativeGeometry().top()));
            newGeo.setRight(std::min(newGeo.right(), parentT->relativeGeometry().right()));
            newGeo.setBottom(std::min(newGeo.bottom(), parentT->relativeGeometry().bottom()));
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

        splitTiles << this;
        splitTiles << parentT->createChildAt(newGeo, layoutDirection(), row() + 1);
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
            newGeo.setLeft(std::max(newGeo.left(), relativeGeometry().left()));
            newGeo.setTop(std::max(newGeo.top(), relativeGeometry().top()));
            newGeo.setRight(std::min(newGeo.right(), relativeGeometry().right()));
            newGeo.setBottom(std::min(newGeo.bottom(), relativeGeometry().bottom()));
            splitTiles << createChildAt(newGeo, newDirection, childCount());
        } else if (newDirection == LayoutDirection::Horizontal) {
            // Do a new layout with 2 cells inside this one
            newGeo.setWidth(relativeGeometry().width() / 2);
            splitTiles << createChildAt(newGeo, newDirection, childCount());
            newGeo.moveLeft(newGeo.x() + newGeo.width());
            splitTiles << createChildAt(newGeo, newDirection, childCount());
        } else if (newDirection == LayoutDirection::Vertical) {
            // Do a new layout with 2 cells inside this one
            newGeo.setHeight(relativeGeometry().height() / 2);
            splitTiles << createChildAt(newGeo, newDirection, childCount());
            newGeo.moveTop(newGeo.y() + newGeo.height());
            splitTiles << createChildAt(newGeo, newDirection, childCount());
        }
    }

    return splitTiles;
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

    manager()->model()->beginRemoveTile(this);
    parentT->removeChild(this);
    m_parentTile = nullptr;
    manager()->model()->endRemoveTile();
    manager()->tileRemoved(this);

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

    // On linear layouts remove the last one and promote the layout as leaf
    if (parentT->layoutDirection() != Tile::LayoutDirection::Floating && parentT->childCount() == 1) {
        auto *lastTile = static_cast<CustomTile *>(parentT->childTile(0));
        if (lastTile->childCount() == 0) {
            lastTile->remove();
        }
    }

    const auto windows = std::exchange(m_windows, {});
    for (Window *window : windows) {
        window->requestTile(m_tiling->bestTileForPosition(window->moveResizeGeometry().center()));
    }

    deleteLater(); // not using "delete this" because QQmlEngine will crash
}

CustomTile *CustomTile::nextTileAt(Qt::Edge edge) const
{
    auto *parentT = static_cast<CustomTile *>(parentTile());

    // TODO: implement geometry base searching for floating?
    if (!parentT) {
        return nullptr;
    }

    Tile *sibling = nullptr;

    const int index = row();
    int layoutRows;
    int layoutColumns;
    switch (parentT->layoutDirection()) {
    case LayoutDirection::Vertical:
        layoutRows = std::max(1, parentT->childCount());
        layoutColumns = 1;
        break;
    case LayoutDirection::Horizontal:
    default:
        layoutColumns = std::max(1, parentT->childCount());
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

CustomTile *CustomTile::nextNonLayoutTileAt(Qt::Edge edge) const
{
    auto tile = nextTileAt(edge);
    // Loop through the tiles until we find a child tile that is not a layout
    while (tile && tile->isLayout()) {
        tile = qobject_cast<CustomTile *>(tile->childTiles().first());
    }
    return tile;
}

void CustomTile::setLayoutDirection(Tile::LayoutDirection dir)
{
    if (m_layoutDirection == dir) {
        return;
    }

    m_layoutDirection = dir;
    Q_EMIT layoutDirectionChanged(dir);
}

Tile::LayoutDirection CustomTile::layoutDirection() const
{
    return m_layoutDirection;
}

RootTile::RootTile(TileManager *tiling)
    : CustomTile(tiling, nullptr)
{
    setParent(tiling);
    setRelativeGeometry({0, 0, 1, 1});
}

} // namespace KWin

#include "moc_customtile.cpp"
