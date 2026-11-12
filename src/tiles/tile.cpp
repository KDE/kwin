/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2022 Marco Martin <mart@kde.org>
    SPDX-FileCopyrightText: 2026 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "tile.h"
#include "core/output.h"
#include "effect/globals.h"
#include "quicktile.h"
#include "tilemanager.h"
#include "virtualdesktops.h"
#include "window.h"
#include "workspace.h"

#include <cmath>

#include <QScopedValueRollback>

namespace KWin
{

QDebug operator<<(QDebug debug, const Tile *tile)
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

Tile::Tile(TileManager *tiling, Tile *parent)
    : QObject(parent)
    , m_parentTile(parent)
    , m_tiling(tiling)
{
    if (m_parentTile) {
        m_padding = m_parentTile->padding();
        m_minimumSize = m_parentTile->m_minimumSize;
        m_desktop = m_parentTile->desktop();
    }
    connect(Workspace::self(), &Workspace::configChanged, this, &Tile::windowGeometryChanged);
    connect(Workspace::self(), &Workspace::windowRemoved, this, &Tile::unmanage);
}

Tile::~Tile()
{
    for (auto *t : std::as_const(m_children)) {
        // Prevents access upon child tiles destruction
        t->m_parentTile = nullptr;
    }
    if (m_parentTile) {
        m_parentTile->removeChild(this);
    }

    if (m_tiling->tearingDown()) {
        return;
    }

    // TODO: Look for alternative designs to evacuate windows from removed tiles.
    auto windows = m_windows;
    for (auto *w : windows) {
        if (m_quickTileMode != QuickTileFlag::Custom) {
            unmanage(w);
        } else {
            Tile *tile = nullptr;
            if (RootTile *root = m_tiling->rootTile(m_desktop)) {
                tile = root->pick(w->moveResizeGeometry().center());
            }
            unmanage(w);
            if (tile) {
                tile->manage(w);
            }
        }
    }
}

VirtualDesktop *Tile::desktop() const
{
    return m_desktop;
}

bool Tile::isActive() const
{
    return m_desktop == VirtualDesktopManager::self()->currentDesktop(m_tiling->output());
}

bool Tile::supportsResizeGravity(Gravity gravity)
{
    if (!m_parentTile) {
        return false;
    }

    switch (gravity) {
    case Gravity::Left:
        return m_relativeGeometry.left() > 0.0;
    case Gravity::Top:
        return m_relativeGeometry.top() > 0.0;
    case Gravity::Right:
        return m_relativeGeometry.right() < 1.0;
    case Gravity::Bottom:
        return m_relativeGeometry.bottom() < 1.0;
    case Gravity::TopLeft:
        return m_relativeGeometry.top() > 0.0 && m_relativeGeometry.left() > 0.0;
    case Gravity::TopRight:
        return m_relativeGeometry.top() > 0.0 && m_relativeGeometry.right() < 1.0;
    case Gravity::BottomLeft:
        return m_relativeGeometry.bottom() < 1.0 && m_relativeGeometry.left() > 0.0;
    case Gravity::BottomRight:
        return m_relativeGeometry.bottom() < 1.0 && m_relativeGeometry.right() < 1.0;
    case Gravity::Center:
    default:
        return false;
    }
}

Gravity Tile::resolveResizeGravity(Gravity gravity)
{
    if (supportsResizeGravity(gravity)) {
        return gravity;
    }

    const auto resolveEdge = [this](Gravity preferred, Gravity fallback) {
        if (supportsResizeGravity(preferred)) {
            return preferred;
        }
        if (supportsResizeGravity(fallback)) {
            return fallback;
        }
        return Gravity(Gravity::Center);
    };

    Gravity horizontal = Gravity::Center;
    Gravity vertical = Gravity::Center;

    switch (gravity) {
    case Gravity::Left:
    case Gravity::TopLeft:
    case Gravity::BottomLeft:
        horizontal = resolveEdge(Gravity::Left, Gravity::Right);
        break;
    case Gravity::Right:
    case Gravity::TopRight:
    case Gravity::BottomRight:
        horizontal = resolveEdge(Gravity::Right, Gravity::Left);
        break;
    default:
        break;
    }

    switch (gravity) {
    case Gravity::Top:
    case Gravity::TopLeft:
    case Gravity::TopRight:
        vertical = resolveEdge(Gravity::Top, Gravity::Bottom);
        break;
    case Gravity::Bottom:
    case Gravity::BottomLeft:
    case Gravity::BottomRight:
        vertical = resolveEdge(Gravity::Bottom, Gravity::Top);
        break;
    default:
        break;
    }

    if (horizontal == Gravity::Left) {
        if (vertical == Gravity::Top) {
            return Gravity::TopLeft;
        }
        if (vertical == Gravity::Bottom) {
            return Gravity::BottomLeft;
        }
        return Gravity::Left;
    }
    if (horizontal == Gravity::Right) {
        if (vertical == Gravity::Top) {
            return Gravity::TopRight;
        }
        if (vertical == Gravity::Bottom) {
            return Gravity::BottomRight;
        }
        return Gravity::Right;
    }
    return vertical;
}

void Tile::setRelativeGeometry(const RectF &geom)
{
    RectF constrainedGeom = geom;
    constrainedGeom.setWidth(std::max(constrainedGeom.width(), m_minimumSize.width()));
    constrainedGeom.setHeight(std::max(constrainedGeom.height(), m_minimumSize.height()));

    if (m_relativeGeometry == constrainedGeom) {
        return;
    }

    m_relativeGeometry = constrainedGeom;

    Q_EMIT relativeGeometryChanged();
    Q_EMIT absoluteGeometryChanged();
    Q_EMIT windowGeometryChanged();

    if (isActive()) {
        for (auto *w : std::as_const(m_windows)) {
            // Resize only if we are the currently managing tile for that window
            w->moveResize(windowGeometry());
        }
    }
}

void Tile::setRelativeGeometryAndArrange(const RectF &geom)
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

    if (m_quickTileMode == QuickTileFlag::Custom) {
        if (!m_geometryLock && !relativeGeometry().isEmpty() && m_parentTile) {
            QScopedValueRollback<bool> geometryLock(m_geometryLock, true);
            if (finalGeom.left() != relativeGeometry().left()) {
                Tile *tile = nextTileAt(Qt::LeftEdge);
                if (tile) {
                    RectF tileGeom = tile->relativeGeometry();
                    tileGeom.setRight(finalGeom.left());
                    if (tileGeom.width() <= tile->minimumSize().width()) {
                        return;
                    }
                    tile->setRelativeGeometryAndArrange(tileGeom);
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
                    tile->setRelativeGeometryAndArrange(tileGeom);
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
                    tile->setRelativeGeometryAndArrange(tileGeom);
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
                    tile->setRelativeGeometryAndArrange(tileGeom);
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

            t->setRelativeGeometryAndArrange(childGeom);
        }

        if (!childrenT.isEmpty()) {
            auto childGeom = childrenT.first()->relativeGeometry();
            if (layoutDirection() == LayoutDirection::Horizontal) {
                childGeom.setLeft(finalGeom.left());
            } else if (layoutDirection() == LayoutDirection::Vertical) {
                childGeom.setTop(finalGeom.top());
            }
            childrenT.first()->setRelativeGeometryAndArrange(childGeom);
        }

        // Make sure the last child item fills the area
        // TODO: ensure all children don't go below minimum size/resize all children
        if (!m_geometryLock && !childrenT.isEmpty()) {
            auto childGeom = childrenT.last()->relativeGeometry();
            childGeom.setRight(finalGeom.right());
            childGeom.setBottom(finalGeom.bottom());
            childrenT.last()->setRelativeGeometryAndArrange(childGeom);
        }
    }

    setRelativeGeometry(finalGeom);
    if (m_parentTile) {
        Q_EMIT m_parentTile->layoutModified();
    }
    m_geometryLock = false;
}

RectF Tile::relativeGeometry() const
{
    return m_relativeGeometry;
}

RectF Tile::absoluteGeometry() const
{
    const RectF geom = workspace()->clientArea(MaximizeArea, m_tiling->output());
    return RectF(std::round(geom.x() + m_relativeGeometry.x() * geom.width()),
                 std::round(geom.y() + m_relativeGeometry.y() * geom.height()),
                 std::round(m_relativeGeometry.width() * geom.width()),
                 std::round(m_relativeGeometry.height() * geom.height()));
}

RectF Tile::absoluteGeometryInScreen() const
{
    const RectF geom = workspace()->clientArea(MaximizeArea, m_tiling->output());
    return RectF(std::round(m_relativeGeometry.x() * geom.width()),
                 std::round(m_relativeGeometry.y() * geom.height()),
                 std::round(m_relativeGeometry.width() * geom.width()),
                 std::round(m_relativeGeometry.height() * geom.height()));
}

RectF Tile::windowGeometry() const
{
    // Apply half padding between tiles and full against the screen edges
    QMarginsF effectiveMargins;
    effectiveMargins.setLeft(m_relativeGeometry.left() > 0.0 ? m_padding / 2.0 : m_padding);
    effectiveMargins.setTop(m_relativeGeometry.top() > 0.0 ? m_padding / 2.0 : m_padding);
    effectiveMargins.setRight(m_relativeGeometry.right() < 1.0 ? m_padding / 2.0 : m_padding);
    effectiveMargins.setBottom(m_relativeGeometry.bottom() < 1.0 ? m_padding / 2.0 : m_padding);

    const auto geom = absoluteGeometry();
    return geom.intersected(m_tiling->output()->geometryF()) - effectiveMargins;
}

Qt::Edges Tile::anchors() const
{
    if (m_padding > 0) {
        return Qt::Edges();
    }
    Qt::Edges anchors = Qt::LeftEdge | Qt::TopEdge | Qt::RightEdge | Qt::BottomEdge;

    if (!qFuzzyCompare(m_relativeGeometry.left(), 0)) {
        anchors &= ~Qt::LeftEdge;
    }
    if (!qFuzzyCompare(m_relativeGeometry.top(), 0)) {
        anchors &= ~Qt::TopEdge;
    }
    if (!qFuzzyCompare(m_relativeGeometry.right(), 1)) {
        anchors &= ~Qt::RightEdge;
    }
    if (!qFuzzyCompare(m_relativeGeometry.bottom(), 1)) {
        anchors &= ~Qt::BottomEdge;
    }
    return anchors;
}

bool Tile::isRoot() const
{
    return !m_parentTile;
}

bool Tile::isLayout() const
{
    // Items with a single child are not allowed, unless the root which is *always* layout
    return m_children.count() > 0 || !m_parentTile;
}

bool Tile::canBeRemoved() const
{
    // The root tile can *never* be removed
    return m_parentTile;
}

qreal Tile::padding() const
{
    // Assume padding is all the same
    return m_padding;
}

void Tile::setPadding(qreal padding)
{
    if (m_padding == padding) {
        return;
    }

    m_padding = padding;

    for (auto *t : std::as_const(m_children)) {
        t->setPadding(padding);
    }
    if (isActive()) {
        for (auto *w : std::as_const(m_windows)) {
            // Resize only if we are the currently managing tile for that window
            w->moveResize(windowGeometry());
        }
    }

    Q_EMIT paddingChanged(padding);
    Q_EMIT windowGeometryChanged();
}

QSizeF Tile::minimumSize() const
{
    return m_minimumSize;
}

void Tile::setMinimumSize(const QSizeF &size)
{
    QSizeF clampedSize = size;
    clampedSize.setWidth(std::clamp(size.width(), 0.0, 1.0));
    clampedSize.setHeight(std::clamp(size.height(), 0.0, 1.0));

    if (m_minimumSize == clampedSize) {
        return;
    }

    m_minimumSize = clampedSize;

    setRelativeGeometryAndArrange(m_relativeGeometry);
    Q_EMIT minimumSizeChanged(clampedSize);
}

QuickTileMode Tile::quickTileMode() const
{
    return m_quickTileMode;
}

void Tile::setQuickTileMode(QuickTileMode mode)
{
    m_quickTileMode = mode;
}

Tile *Tile::rootTile() const
{
    if (!m_parentTile) {
        return const_cast<Tile *>(this);
    }
    Tile *candidate = m_parentTile;
    while (candidate->parentTile()) {
        candidate = candidate->parentTile();
    }
    return candidate;
}

void Tile::resizeByPixels(const QPointF &delta, Gravity gravity)
{
    Qt::Edges edges;
    switch (gravity) {
    case Gravity::TopLeft:
        edges = Qt::TopEdge | Qt::LeftEdge;
        break;
    case Gravity::BottomRight:
        edges = Qt::BottomEdge | Qt::RightEdge;
        break;
    case Gravity::BottomLeft:
        edges = Qt::BottomEdge | Qt::LeftEdge;
        break;
    case Gravity::TopRight:
        edges = Qt::TopEdge | Qt::RightEdge;
        break;
    case Gravity::Top:
        edges = Qt::TopEdge;
        break;
    case Gravity::Bottom:
        edges = Qt::BottomEdge;
        break;
    case Gravity::Left:
        edges = Qt::LeftEdge;
        break;
    case Gravity::Right:
        edges = Qt::RightEdge;
        break;
    case Gravity::Center:
        Q_UNREACHABLE();
        break;
    }

    resizeByPixels(delta, edges);
}

void Tile::resizeByPixels(qreal delta, Qt::Edge edge)
{
    if (edge & (Qt::LeftEdge | Qt::RightEdge)) {
        resizeByPixels(QPointF(delta, 0), edge);
    } else if (edge & (Qt::TopEdge | Qt::BottomEdge)) {
        resizeByPixels(QPointF(0, delta), edge);
    }
}

void Tile::resizeByPixels(const QPointF &delta, Qt::Edges edges)
{
    if (!m_parentTile) {
        return;
    }

    const RectF bounds = m_tiling->output()->geometryF();
    const QPointF relativeDelta(delta.x() / bounds.width(), delta.y() / bounds.height());

    auto newGeom = m_relativeGeometry;

    if (edges & Qt::LeftEdge) {
        newGeom.setLeft(newGeom.left() + relativeDelta.x());
    } else if (edges & Qt::RightEdge) {
        newGeom.setRight(newGeom.right() + relativeDelta.x());
    }

    if (edges & Qt::TopEdge) {
        newGeom.setTop(newGeom.top() + relativeDelta.y());
    } else if (edges & Qt::BottomEdge) {
        newGeom.setBottom(newGeom.bottom() + relativeDelta.y());
    }

    setRelativeGeometryAndArrange(newGeom);
}

bool Tile::manage(Window *window)
{
    if (!window->isResizable() || window->isAppletPopup()) {
        return false;
    }

    if (m_windows.contains(window)) {
        return true;
    }

    // If the window is not on the same tile's desktop
    // it can't be added
    if (!window->isOnDesktop(m_desktop)) {
        return false;
    }

    // There can be only one window owner per root tile, so make it forget
    // if anybody else had the association
    bool evacuated = false;
    const auto evacuate = [&evacuated, window](Tile *tile) {
        if (tile->remove(window)) {
            evacuated = true;
        }
    };

    const auto outputs = workspace()->outputs();
    for (LogicalOutput *output : outputs) {
        if (TileManager *manager = workspace()->tileManager(output)) {
            if (Tile *rootTile = manager->rootTile(m_desktop)) {
                rootTile->visitDescendants(evacuate);
            }
            if (Tile *rootTile = manager->quickRootTile(m_desktop)) {
                rootTile->visitDescendants(evacuate);
            }
        }
    }

    add(window);

    // We can actually manage the window geometry if our desktop is current
    // or if our desktop is the only desktop the window is in
    if (isActive()) {
        window->requestTile(this);
    } else if (evacuated) {
        window->requestTile(nullptr);
    }

    return true;
}

bool Tile::unmanage(Window *window)
{
    if (!remove(window)) {
        return false;
    }

    if (window->requestedTile() == this) {
        window->requestTile(nullptr);
    }
    return true;
}

void Tile::forget(Window *window)
{
    if (remove(window)) {
        window->forgetTile(this);
    }
}

void Tile::add(Window *window)
{
    Q_ASSERT(!m_windows.contains(window));
    m_windows.append(window);

    Q_EMIT windowAdded(window);
    Q_EMIT windowsChanged();
}

bool Tile::remove(Window *window)
{
    if (!m_windows.removeOne(window)) {
        return false;
    }

    Q_EMIT windowRemoved(window);
    Q_EMIT windowsChanged();
    return true;
}

QList<KWin::Window *> Tile::windows() const
{
    return m_windows;
}

void Tile::insertChild(int position, Tile *item)
{
    Q_EMIT aboutToInsertChildTile(item, position);

    Q_ASSERT(position >= 0);
    item->setParent(this);

    m_children.insert(std::clamp<qsizetype>(position, 0, m_children.length()), item);

    Q_EMIT childTileInserted();
    Q_EMIT childTilesChanged();
}

void Tile::removeChild(Tile *child)
{
    Q_EMIT aboutToRemoveChildTile(child);

    m_children.removeAll(child);

    Q_EMIT childTileRemoved();
    Q_EMIT childTilesChanged();
}

void Tile::remove()
{
    if (!m_parentTile) {
        return;
    }

    auto *prev = previousSibling();
    auto *next = nextSibling();

    const auto parentTile = m_parentTile;
    m_parentTile->removeChild(this);
    m_parentTile = nullptr;

    if (parentTile->layoutDirection() == LayoutDirection::Horizontal) {
        if (prev && next) {
            auto geom = prev->relativeGeometry();
            geom.setRight(relativeGeometry().center().x());
            prev->setRelativeGeometryAndArrange(geom);
            geom = next->relativeGeometry();
            geom.setLeft(relativeGeometry().center().x());
            next->setRelativeGeometryAndArrange(geom);
        } else if (prev) {
            auto geom = prev->relativeGeometry();
            geom.setRight(relativeGeometry().right());
            prev->setRelativeGeometryAndArrange(geom);
        } else if (next) {
            auto geom = next->relativeGeometry();
            geom.setLeft(relativeGeometry().left());
            next->setRelativeGeometryAndArrange(geom);
        }
    } else if (parentTile->layoutDirection() == LayoutDirection::Vertical) {
        if (prev && next) {
            auto geom = prev->relativeGeometry();
            geom.setBottom(relativeGeometry().center().y());
            prev->setRelativeGeometryAndArrange(geom);
            geom = next->relativeGeometry();
            geom.setTop(relativeGeometry().center().y());
            next->setRelativeGeometryAndArrange(geom);
        } else if (prev) {
            auto geom = prev->relativeGeometry();
            geom.setBottom(relativeGeometry().bottom());
            prev->setRelativeGeometryAndArrange(geom);
        } else if (next) {
            auto geom = next->relativeGeometry();
            geom.setTop(relativeGeometry().top());
            next->setRelativeGeometryAndArrange(geom);
        }
    }

    // On linear layouts remove the last one and promote the layout as leaf
    if (!parentTile->isRoot() && parentTile->childCount() == 1) {
        auto *lastTile = parentTile->childTile(0);
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

QList<Tile *> Tile::split(KWin::Tile::LayoutDirection newDirection)
{
    QList<Tile *> splitTiles;

    // If we are m_rootLayoutTile always create children, not siblings
    if (m_parentTile && (m_parentTile->childCount() < 2 || m_parentTile->layoutDirection() == newDirection)) {
        // Add a new cell to the current layout
        setLayoutDirection(newDirection);
        m_parentTile->setLayoutDirection(newDirection);
        RectF newGeo;
        if (newDirection == LayoutDirection::Horizontal) {
            newGeo = relativeGeometry();
            newGeo.setWidth(relativeGeometry().width() / 2);
            setRelativeGeometry(newGeo);
            newGeo.moveLeft(newGeo.x() + newGeo.width());
        } else if (newDirection == LayoutDirection::Vertical) {
            newGeo = relativeGeometry();
            newGeo.setHeight(relativeGeometry().height() / 2);
            setRelativeGeometry(newGeo);
            newGeo.moveTop(newGeo.y() + newGeo.height());
        }

        splitTiles << this;
        splitTiles << m_parentTile->createChildAt(newGeo, layoutDirection(), row() + 1);
    } else {
        // Do a new layout and put tiles inside
        setLayoutDirection(newDirection);
        auto newGeo = relativeGeometry();
        if (newDirection == LayoutDirection::Horizontal) {
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

QList<Tile *> Tile::childTiles() const
{
    return m_children;
}

Tile *Tile::childTile(int row)
{
    if (row < 0 || row >= m_children.size()) {
        return nullptr;
    }
    return m_children.value(row);
}

int Tile::childCount() const
{
    return m_children.count();
}

QList<Tile *> Tile::descendants() const
{
    QList<Tile *> tiles;
    for (auto *t : std::as_const(m_children)) {
        tiles << t << t->descendants();
    }
    return tiles;
}

Tile *Tile::parentTile() const
{
    return m_parentTile;
}

void Tile::visitDescendants(std::function<void(Tile *child)> callback)
{
    callback(this);
    for (Tile *child : m_children) {
        child->visitDescendants(callback);
    }
}

int Tile::row() const
{
    if (m_parentTile) {
        return m_parentTile->m_children.indexOf(this);
    }

    return -1;
}

Tile *Tile::nextSibling() const
{
    const int r = row();
    if (!m_parentTile || row() >= m_parentTile->childCount() - 1) {
        return nullptr;
    } else {
        return m_parentTile->childTiles()[r + 1];
    }
}

Tile *Tile::previousSibling() const
{
    const int r = row();
    if (r <= 0 || !m_parentTile) {
        return nullptr;
    } else {
        return m_parentTile->childTiles().at(r - 1);
    }
}

void Tile::setLayoutDirection(LayoutDirection dir)
{
    if (m_layoutDirection == dir) {
        return;
    }

    m_layoutDirection = dir;
    Q_EMIT layoutDirectionChanged(dir);
}

Tile::LayoutDirection Tile::layoutDirection() const
{
    return m_layoutDirection;
}

Tile *Tile::nextTileAt(Qt::Edge edge) const
{
    if (!m_parentTile) {
        return nullptr;
    }

    Tile *sibling = nullptr;

    const int index = row();
    int layoutRows;
    int layoutColumns;
    switch (m_parentTile->layoutDirection()) {
    case LayoutDirection::Vertical:
        layoutRows = std::max(1, m_parentTile->childCount());
        layoutColumns = 1;
        break;
    case LayoutDirection::Horizontal:
    default:
        layoutColumns = std::max(1, m_parentTile->childCount());
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
            sibling = m_parentTile->childTiles()[layoutColumns * (row - 1) + column];
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
            if (newIndex < m_parentTile->childCount()) {
                sibling = m_parentTile->childTiles()[newIndex];
            }
        }
        break;
    }

    if (sibling) {
        return sibling;
    } else {
        return m_parentTile->nextTileAt(edge);
    }
}

Tile *Tile::nextNonLayoutTileAt(Qt::Edge edge) const
{
    auto tile = nextTileAt(edge);
    // Loop through the tiles until we find a child tile that is not a layout
    while (tile && tile->isLayout()) {
        tile = tile->childTiles().first();
    }
    return tile;
}

RootTile::RootTile(TileManager *tiling, VirtualDesktop *desktop)
    : Tile(tiling, nullptr)
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

#include "moc_tile.cpp"
