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
    m_geometryLock = true;
}

CustomTile *CustomTile::createChildAt(const RectF &relativeGeometry, LayoutDirection layoutDirection, int position)
{
    CustomTile *tile = new CustomTile(manager(), this);
    connect(tile, &CustomTile::layoutModified, this, &CustomTile::layoutModified);
    tile->setRelativeGeometry(relativeGeometry);
    tile->setLayoutDirection(layoutDirection);
    insertChild(position, tile);
    return tile;
}

CustomTile *CustomTile::createChildAt(const QSizeF &relativeSize, LayoutDirection direction, int position)
{
    RectF initialGeometry;
    if (childCount() == 0) {
        initialGeometry = relativeGeometry();
    } else {
        if (position == childCount()) {
            Tile *last = childTile(childCount() - 1);
            if (layoutDirection() == LayoutDirection::Horizontal) {
                initialGeometry = RectF(QPointF(last->relativeGeometry().right(), last->relativeGeometry().top()),
                                   QPointF(last->relativeGeometry().right(), last->relativeGeometry().bottom()));
            } else {
                initialGeometry = RectF(QPointF(last->relativeGeometry().left(), last->relativeGeometry().bottom()),
                                   QPointF(last->relativeGeometry().right(), last->relativeGeometry().bottom()));
            }
        } else {
            Tile *tile = childTile(position);
            if (layoutDirection() == LayoutDirection::Horizontal) {
                initialGeometry = RectF(QPointF(tile->relativeGeometry().left(), tile->relativeGeometry().top()),
                                   QPointF(tile->relativeGeometry().left(), tile->relativeGeometry().bottom()));
            } else {
                initialGeometry = RectF(QPointF(tile->relativeGeometry().left(), tile->relativeGeometry().top()),
                                   QPointF(tile->relativeGeometry().right(), tile->relativeGeometry().top()));
            }
        }
    }

    CustomTile *tile = new CustomTile(manager(), this);
    connect(tile, &CustomTile::layoutModified, this, &CustomTile::layoutModified);
    tile->setRelativeGeometry(initialGeometry);
    tile->setLayoutDirection(direction);
    insertChild(position, tile);

    if (position == childCount() - 1) {
        if (layoutDirection() == LayoutDirection::Horizontal) {
            initialGeometry.setLeft(initialGeometry.left() - relativeSize.width());
        } else {
            initialGeometry.setTop(initialGeometry.top() - relativeSize.height());
        }
    } else {
        if (layoutDirection() == LayoutDirection::Horizontal) {
            initialGeometry.setRight(initialGeometry.right() + relativeSize.width());
        } else {
            initialGeometry.setBottom(initialGeometry.bottom() + relativeSize.height());
        }
    }

    tile->setRelativeGeometry(initialGeometry);

    return tile;
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

    if (!m_geometryLock && parentT && parentT->layoutDirection() != LayoutDirection::Floating) {
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
        return gravity != Gravity::Center;
    }

    return Tile::supportsResizeGravity(gravity);
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
        if (newDirection == LayoutDirection::Floating) {
            newGeo = RectF(relativeGeometry().left() + 0.1, relativeGeometry().top() + 0.1, 0.3, 0.2);
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
            newGeo = RectF(startGeom.left() + 0.05, startGeom.top() + 0.05, 0.3, 0.25);
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
    const auto parent = static_cast<CustomTile *>(parentTile());
    if (!parent || parent->layoutDirection() != LayoutDirection::Floating) {
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

    parentT->removeChild(this);
    m_parentTile = nullptr;

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
    if (!parentT->isRoot() && parentT->childCount() == 1 && parentT->layoutDirection() != Tile::LayoutDirection::Floating) {
        auto *lastTile = static_cast<CustomTile *>(parentT->childTile(0));
        if (lastTile->childCount() == 0) {
            lastTile->remove();
        }
    }

    const auto windows = std::exchange(m_windows, {});
    for (Window *window : windows) {
        Tile *tile = m_tiling->rootTile(m_desktop)->pick(window->moveResizeGeometry().center());
        if (tile) {
            tile->manage(window);
        }
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

void CustomTile::wrap(LayoutDirection direction)
{
    if (m_layoutDirection == direction) {
        return;
    }

    if (m_children.size() == 1 && static_cast<CustomTile *>(m_children[0])->layoutDirection() == direction) {
        adoptChildren(m_children[0]->childTiles());
        destroyChild(m_children[0]);
    } else {
        const auto childTiles = m_children;

        CustomTile *containerTile = createChildAt(relativeGeometry(), layoutDirection(), childCount());
        containerTile->adoptChildren(childTiles);
    }

    setLayoutDirection(direction);
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

static Tile *closestTile(const QList<Tile *> &tiles, const QPointF &point)
{
    Tile *bestTile = nullptr;
    qreal bestScore;

    for (Tile *tile : tiles) {
        const RectF absoluteGeometry = tile->absoluteGeometry();
        if (absoluteGeometry.contains(point)) {
            return tile;
        }

        const QPointF closestPoint(std::clamp(point.x(), absoluteGeometry.left(), absoluteGeometry.right()),
                                   std::clamp(point.y(), absoluteGeometry.top(), absoluteGeometry.bottom()));
        const qreal score = (closestPoint - point).manhattanLength();
        if (!bestTile || score < bestScore) {
            bestTile = tile;
            bestScore = score;
        }
    }

    return bestTile;
}

RectF TileOperation::outline() const
{
    const RectF geometry = tile->absoluteGeometry();

    if (tile->windows().isEmpty()) {
        return geometry;
    } else {
        return geometry.shrunkBy(QMarginsF(geometry.width() * 0.3, geometry.height() * 0.3, geometry.width() * 0.3, geometry.height() * 0.3));
    }
}

void TileOperation::apply(Window *window)
{
    if (!window->requestedTile()) {
        tile->manage(window);
    } else {
        Tile *formerTile = window->requestedTile();

        const auto from = formerTile->windows();
        const auto to = tile->windows();

        for (const auto &window : from) {
            tile->manage(window);
        }

        for (const auto &window : to) {
            formerTile->manage(window);
        }
    }
}

RectF InsertTileOperation::outline() const
{
    const RectF referenceRect = reference->absoluteGeometry();
    const RectF parentRect = reference->parentTile()->absoluteGeometry();

    RectF outlineRect;
    if (direction == Tile::LayoutDirection::Horizontal) {
        outlineRect = RectF(0, parentRect.top(), 50, parentRect.height());
        if (insertionPoint == TileInsertionPoint::Before) {
            outlineRect.moveLeft(referenceRect.left());
        } else {
            outlineRect.moveRight(referenceRect.right());
        }
    } else {
        outlineRect = RectF(parentRect.left(), 0, parentRect.width(), 50);
        if (insertionPoint == TileInsertionPoint::Before) {
            outlineRect.moveTop(referenceRect.top());
        } else {
            outlineRect.moveBottom(referenceRect.bottom());
        }
    }

    return outlineRect;
}

void InsertTileOperation::apply(Window *window)
{
    CustomTile *parentTile = static_cast<CustomTile *>(reference->parentTile());
    CustomTile *childTile;

    if (parentTile->layoutDirection() == direction) {
        const int referenceIndex = parentTile->childTiles().indexOf(reference);

        const RectF parentRect = parentTile->relativeGeometry();
        const QSizeF preferredSize = parentRect.size() / (parentTile->childCount() + 1);

        if (insertionPoint == TileInsertionPoint::Before) {
            childTile = parentTile->createChildAt(preferredSize, Tile::LayoutDirection::Horizontal, referenceIndex);
        } else {
            childTile = parentTile->createChildAt(preferredSize, Tile::LayoutDirection::Horizontal, referenceIndex + 1);
        }
    } else {
        parentTile->wrap(direction);

        if (insertionPoint == TileInsertionPoint::Before) {
            childTile = parentTile->createChildAt(QSizeF(0.5, 0.5), Tile::LayoutDirection::Horizontal, 0);
        } else {
            childTile = parentTile->createChildAt(QSizeF(0.5, 0.5), Tile::LayoutDirection::Horizontal, 1);
        }
    }

    childTile->manage(window);
}

RectF SplitTileOperation::outline() const
{
    const RectF referenceRect = reference->absoluteGeometry();

    RectF outlineRect;
    if (direction == Tile::LayoutDirection::Horizontal) {
        outlineRect = RectF(0, referenceRect.top(), referenceRect.width() * 0.3, referenceRect.height());
        if (insertionPoint == TileInsertionPoint::Before) {
            outlineRect.moveLeft(referenceRect.left());
        } else {
            outlineRect.moveRight(referenceRect.right());
        }
    } else {
        outlineRect = RectF(referenceRect.left(), 0, referenceRect.width(), referenceRect.height() * 0.3);
        if (insertionPoint == TileInsertionPoint::Before) {
            outlineRect.moveTop(referenceRect.top());
        } else {
            outlineRect.moveBottom(referenceRect.bottom());
        }
    }

    return outlineRect;
}

void SplitTileOperation::apply(Window *window)
{
    const auto splitTiles = static_cast<CustomTile *>(reference)->split(direction);

    Tile *formerTile;
    Tile *newTile;

    if (insertionPoint == TileInsertionPoint::Before) {
        newTile = splitTiles[0];
        formerTile = splitTiles[1];
    } else {
        newTile = splitTiles[1];
        formerTile = splitTiles[0];
    }

    const auto windows = reference->windows();
    for (Window *window : windows) {
        formerTile->manage(window);
    }

    newTile->manage(window);
}

RectF InteractiveTileOperation::outline() const
{
    if (const auto tileOperation = std::get_if<TileOperation>(this)) {
        return tileOperation->outline();
    } else if (const auto insertOperation = std::get_if<InsertTileOperation>(this)) {
        return insertOperation->outline();
    } else if (const auto splitOperation = std::get_if<SplitTileOperation>(this)) {
        return splitOperation->outline();
    }

    Q_UNREACHABLE_RETURN(RectF());
}

void InteractiveTileOperation::apply(Window *window)
{
    if (const auto tileOperation = std::get_if<TileOperation>(this)) {
        tileOperation->apply(window);
    } else if (const auto insertOperation = std::get_if<InsertTileOperation>(this)) {
        insertOperation->apply(window);
    } else if (const auto splitOperation = std::get_if<SplitTileOperation>(this)) {
        splitOperation->apply(window);
    }
}

std::optional<InteractiveTileOperation> RootTile::interactiveTileOp(const Window *window, const QPointF &point) const
{
    const qreal siblingThreshold = 0.1;
    const qreal splitThreshold = 0.3;

    const Tile *candidateTile = this;
    while (candidateTile) {
        Tile *nextTile = closestTile(candidateTile->childTiles(), point);
        if (nextTile->isLayout()) {
            candidateTile = nextTile;
            continue;
        }

        const bool splittable = !nextTile->windows().isEmpty() && !nextTile->windows().contains(window);
        const RectF nextTileRect = nextTile->absoluteGeometry();

        const qreal relativeDistanceX = (point.x() - nextTileRect.left()) / nextTileRect.width();
        const qreal relativeDistanceY = (point.y() - nextTileRect.top()) / nextTileRect.height();

        if (relativeDistanceX >= splitThreshold && relativeDistanceX < (1 - splitThreshold) &&
            relativeDistanceY >= splitThreshold && relativeDistanceY < (1 - splitThreshold)) {
            if (!splittable) {
                return std::nullopt;
            }
            return TileOperation {
                .tile = nextTile,
            };
        }

        if (relativeDistanceX < siblingThreshold) {
            return InsertTileOperation {
                .reference = nextTile,
                .direction = LayoutDirection::Horizontal,
                .insertionPoint = TileInsertionPoint::Before,
            };
        } else if (splittable && relativeDistanceX < splitThreshold) {
            return SplitTileOperation {
                .reference = nextTile,
                .direction = LayoutDirection::Horizontal,
                .insertionPoint = TileInsertionPoint::Before,
            };
        }

        if (relativeDistanceX >= 1 - siblingThreshold) {
            return InsertTileOperation {
                .reference = nextTile,
                .direction = LayoutDirection::Horizontal,
                .insertionPoint = TileInsertionPoint::After,
            };
        } else if (splittable && relativeDistanceX >= 1 - splitThreshold) {
            return SplitTileOperation {
                .reference = nextTile,
                .direction = LayoutDirection::Horizontal,
                .insertionPoint = TileInsertionPoint::After,
            };
        }

        if (relativeDistanceY < siblingThreshold) {
            return InsertTileOperation {
                .reference = nextTile,
                .direction = LayoutDirection::Vertical,
                .insertionPoint = TileInsertionPoint::Before,
            };
        } else if (splittable && relativeDistanceY < splitThreshold) {
            return SplitTileOperation {
                .reference = nextTile,
                .direction = LayoutDirection::Vertical,
                .insertionPoint = TileInsertionPoint::Before,
            };
        }

        if (relativeDistanceY >= 1 - siblingThreshold) {
            return InsertTileOperation {
                .reference = nextTile,
                .direction = LayoutDirection::Vertical,
                .insertionPoint = TileInsertionPoint::After,
            };
        } else if (splittable && relativeDistanceY >= 1 - splitThreshold) {
            return SplitTileOperation {
                .reference = nextTile,
                .direction = LayoutDirection::Vertical,
                .insertionPoint = TileInsertionPoint::After,
            };
        }

        return TileOperation {
            .tile = nextTile,
        };
    }

    return std::nullopt;
}

} // namespace KWin

#include "moc_customtile.cpp"
