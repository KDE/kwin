/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2022 Marco Martin <mart@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "customtile.h"
#include "core/output.h"
#include "effect/globals.h"
#include "tilemanager.h"
#include "window.h"

#include <QScopedValueRollback>

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
}

void CustomTile::setRelativeGeometry(const RectF &geom)
{
    if (relativeGeometry() == geom) {
        return;
    }

    RectF finalGeom = geom.intersected(RectF(0, 0, 1, 1));
    finalGeom.setWidth(std::max(finalGeom.width(), minimumSize().width()));
    finalGeom.setHeight(std::max(finalGeom.height(), minimumSize().height()));
    // We couldn't set the minimum size and still remain in boundaries, do nothing
    if (finalGeom.right() > 1 || finalGeom.bottom() > 1) {
        return;
    }

    auto *parentT = static_cast<CustomTile *>(parentTile());

    if (!m_geometryLock && !relativeGeometry().isEmpty() && parentT) {
        QScopedValueRollback<bool> geometryLock(m_geometryLock, true);
        if (finalGeom.left() != relativeGeometry().left()) {
            Tile *tile = nextTileAt(Qt::LeftEdge);
            if (tile) {
                RectF tileGeom = tile->relativeGeometry();
                tileGeom.setRight(finalGeom.left());
                if (tileGeom.width() <= tile->minimumSize().width()) {
                    return;
                }
                tile->setRelativeGeometry(tileGeom);
                // The other tile geometry may not be what we set due to size constraints
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
                    return;
                }
                tile->setRelativeGeometry(tileGeom);
                finalGeom.setBottom(tile->relativeGeometry().top());
            } else {
                // We are at the bottom border of the screen, we are always at 1
                finalGeom.setBottom(1);
            }
        }
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
    if (!m_geometryLock && !childrenT.isEmpty()) {
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

QList<CustomTile *> CustomTile::split(KWin::Tile::LayoutDirection newDirection)
{
    auto *parentT = static_cast<CustomTile *>(parentTile());

    QList<CustomTile *> splitTiles;

    // If we are m_rootLayoutTile always create children, not siblings
    if (parentT && (parentT->childCount() < 2 || parentT->layoutDirection() == newDirection)) {
        // Add a new cell to the current layout
        setLayoutDirection(newDirection);
        parentT->setLayoutDirection(newDirection);
        RectF newGeo;
        if (newDirection == LayoutDirection::Horizontal) {
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
        splitTiles << parentT->createChildAt<CustomTile>(newGeo, layoutDirection(), row() + 1);
    } else {
        // Do a new layout and put tiles inside
        setLayoutDirection(newDirection);
        auto newGeo = relativeGeometry();
        if (newDirection == LayoutDirection::Horizontal) {
            // Do a new layout with 2 cells inside this one
            newGeo.setWidth(relativeGeometry().width() / 2);
            splitTiles << createChildAt<CustomTile>(newGeo, newDirection, childCount());
            newGeo.moveLeft(newGeo.x() + newGeo.width());
            splitTiles << createChildAt<CustomTile>(newGeo, newDirection, childCount());
        } else if (newDirection == LayoutDirection::Vertical) {
            // Do a new layout with 2 cells inside this one
            newGeo.setHeight(relativeGeometry().height() / 2);
            splitTiles << createChildAt<CustomTile>(newGeo, newDirection, childCount());
            newGeo.moveTop(newGeo.y() + newGeo.height());
            splitTiles << createChildAt<CustomTile>(newGeo, newDirection, childCount());
        }

        const auto windows = m_windows;
        for (auto *w : windows) {
            Tile *tile = m_tiling->rootTile(m_desktop)->pick(w->moveResizeGeometry().center());
            unmanage(w);
            if (tile) {
                tile->manage(w);
            }
        }
    }

    return splitTiles;
}

RootTile::RootTile(TileManager *tiling, VirtualDesktop *desktop)
    : CustomTile(tiling, nullptr)
{
    m_desktop = desktop;
    setParent(tiling);
    setRelativeGeometry({0, 0, 1, 1});
}

Tile *RootTile::tileForWindow(Window *window)
{
    if (windows().contains(window)) {
        return this;
    }

    for (Tile *tile : descendants()) {
        if (tile->windows().contains(window)) {
            return tile;
        }
    }

    return nullptr;
}

Tile *RootTile::pick(const QPointF &point) const
{
    const auto tiles = descendants();
    qreal minimumDistance = std::numeric_limits<qreal>::max();
    Tile *ret = nullptr;

    for (auto *t : tiles) {
        if (!t->isLayout()) {
            const auto r = t->absoluteGeometry();
            // It's possible for tiles to overlap, so take the one which center is nearer to mouse pos
            qreal distance = (r.center() - point).manhattanLength();
            if (!r.contains(point)) {
                // This gives a strong preference for tiles that contain the point
                // still base on distance though as floating tiles can overlap
                distance += m_tiling->output()->geometryF().width();
            }
            if (distance < minimumDistance) {
                minimumDistance = distance;
                ret = t;
            }
        }
    }
    return ret;
}

Tile *RootTile::pick(qreal x, qreal y) const
{
    return pick(QPointF(x, y));
}

} // namespace KWin

#include "moc_customtile.cpp"
