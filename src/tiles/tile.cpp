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
    if (m_relativeGeometry == geom) {
        return;
    }

    m_relativeGeometry = geom;

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

Tile *Tile::layoutReference(LayoutDirection layout) const
{
    Tile *candidate = m_parentTile;
    Tile *anchor = const_cast<Tile *>(this);

    while (candidate) {
        if (candidate->layoutDirection() == layout) {
            return anchor;
        }

        anchor = candidate;
        candidate = candidate->m_parentTile;
    }

    return nullptr;
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

void Tile::resizeByPixels(const QPointF &deltaInPixels, Qt::Edges edges)
{
    if (!m_parentTile) {
        return;
    }

    Tile *horizontalParent = nullptr;
    if (edges & (Qt::LeftEdge | Qt::RightEdge)) {
        if (Tile *reference = layoutReference(LayoutDirection::Horizontal)) {
            QList<Tile *> affected;
            qreal deltaSize;

            if (edges.testFlags(Qt::LeftEdge | Qt::RightEdge)) {
                deltaSize = deltaInPixels.x();
                affected = reference->parentTile()->childTiles();
            } else if (edges.testFlag(Qt::LeftEdge)) {
                deltaSize = -deltaInPixels.x();
                affected.append(reference);
                if (const auto sibling = reference->previousSibling()) {
                    affected.append(sibling);
                }
            } else if (edges.testFlag(Qt::RightEdge)) {
                deltaSize = deltaInPixels.x();
                affected.append(reference);
                if (const auto sibling = reference->nextSibling()) {
                    affected.append(sibling);
                }
            }

            if (affected.size() > 1) {
                const RectF parentRectInPixels = reference->parentTile()->absoluteGeometry();
                const qreal deltaInPercents = deltaSize / parentRectInPixels.width();

                qreal allowedDeltaInPercents = deltaInPercents;
                if (deltaInPercents <= 0) {
                    const qreal minWidthInPercents = reference->minWidthInPixels() / parentRectInPixels.width();
                    const qreal slack = std::min(0.0, minWidthInPercents - reference->m_coverage.width());
                    allowedDeltaInPercents = std::max(allowedDeltaInPercents, slack);
                } else {
                    for (Tile *tile : std::as_const(affected)) {
                        if (tile != reference) {
                            const qreal minWidthInPercents = tile->minWidthInPixels() / parentRectInPixels.width();
                            const qreal slack = std::max(0.0, tile->m_coverage.width() - minWidthInPercents);
                            allowedDeltaInPercents = std::min(allowedDeltaInPercents, slack);
                        }
                    }
                }

                for (Tile *tile : std::as_const(affected)) {
                    const qreal adjustment = tile == reference
                        ? allowedDeltaInPercents
                        : -allowedDeltaInPercents / (affected.size() - 1);

                    tile->m_coverage.rwidth() += adjustment;
                }

                horizontalParent = reference->parentTile();
            }
        }
    }

    Tile *verticalParent = nullptr;
    if (edges & (Qt::TopEdge | Qt::BottomEdge)) {
        if (Tile *reference = layoutReference(LayoutDirection::Vertical)) {
            QList<Tile *> affected;
            qreal deltaSize;

            if (edges.testFlags(Qt::TopEdge | Qt::BottomEdge)) {
                deltaSize = deltaInPixels.y();
                affected = reference->parentTile()->childTiles();
            } else if (edges.testFlag(Qt::TopEdge)) {
                deltaSize = -deltaInPixels.y();
                affected.append(reference);
                if (const auto sibling = reference->previousSibling()) {
                    affected.append(sibling);
                }
            } else if (edges.testFlag(Qt::BottomEdge)) {
                deltaSize = deltaInPixels.y();
                affected.append(reference);
                if (const auto sibling = reference->nextSibling()) {
                    affected.append(sibling);
                }
            }

            if (affected.size() > 1) {
                const RectF parentRectInPixels = reference->parentTile()->absoluteGeometry();
                const qreal deltaInPercents = deltaSize / parentRectInPixels.height();

                qreal allowedDeltaInPercents = deltaInPercents;
                if (deltaInPercents <= 0) {
                    const qreal minHeightInPercents = reference->minHeightInPixels() / parentRectInPixels.height();
                    const qreal slack = std::min(0.0, minHeightInPercents - reference->m_coverage.height());
                    allowedDeltaInPercents = std::max(allowedDeltaInPercents, slack);
                } else {
                    for (Tile *tile : std::as_const(affected)) {
                        if (tile != reference) {
                            const qreal minHeightInPercents = tile->minHeightInPixels() / parentRectInPixels.height();
                            const qreal slack = std::max(0.0, tile->m_coverage.height() - minHeightInPercents);
                            allowedDeltaInPercents = std::min(allowedDeltaInPercents, slack);
                        }
                    }
                }

                for (Tile *tile : std::as_const(affected)) {
                    const qreal adjustment = tile == reference
                        ? allowedDeltaInPercents
                        : -allowedDeltaInPercents / (affected.size() - 1);

                    tile->m_coverage.rheight() += adjustment;
                }

                verticalParent = reference->parentTile();
            }
        }
    }

    if (horizontalParent && verticalParent) {
        if (horizontalParent == m_parentTile) {
            // The vertical layout tile is higher in the tile tree.
            verticalParent->arrange();
        } else {
            horizontalParent->arrange();
        }

        return;
    }

    if (horizontalParent) {
        horizontalParent->arrange();
    } else if (verticalParent) {
        verticalParent->arrange();
    }
}

void Tile::resizeByPercents(const QPointF &delta, Qt::Edges edges)
{
    if (!m_parentTile) {
        return;
    }

    Tile *horizontalParent = nullptr;
    if (edges & (Qt::LeftEdge | Qt::RightEdge)) {
        if (Tile *reference = layoutReference(LayoutDirection::Horizontal)) {
            QList<Tile *> affected;
            qreal deltaSize;

            if (edges.testFlags(Qt::LeftEdge | Qt::RightEdge)) {
                deltaSize = delta.x();
                affected = reference->parentTile()->childTiles();
            } else if (edges.testFlag(Qt::LeftEdge)) {
                deltaSize = -delta.x();
                affected.append(reference);
                if (const auto sibling = reference->previousSibling()) {
                    affected.append(sibling);
                }
            } else if (edges.testFlag(Qt::RightEdge)) {
                deltaSize = delta.x();
                affected.append(reference);
                if (const auto sibling = reference->nextSibling()) {
                    affected.append(sibling);
                }
            }

            if (affected.size() > 1) {
                const RectF parentRectInPixels = reference->parentTile()->absoluteGeometry();

                qreal allowedDeltaInPercents = deltaSize;
                if (deltaSize <= 0) {
                    const qreal minWidthInPercents = reference->minWidthInPixels() / parentRectInPixels.width();
                    const qreal slack = std::min(0.0, minWidthInPercents - reference->m_coverage.width());
                    allowedDeltaInPercents = std::max(allowedDeltaInPercents, slack);
                } else {
                    for (Tile *tile : std::as_const(affected)) {
                        if (tile != reference) {
                            const qreal minWidthInPercents = tile->minWidthInPixels() / parentRectInPixels.width();
                            const qreal slack = std::max(0.0, tile->m_coverage.width() - minWidthInPercents);
                            allowedDeltaInPercents = std::min(allowedDeltaInPercents, slack);
                        }
                    }
                }

                for (Tile *tile : std::as_const(affected)) {
                    const qreal adjustment = tile == reference
                        ? allowedDeltaInPercents
                        : -allowedDeltaInPercents / (affected.size() - 1);

                    tile->m_coverage.rwidth() += adjustment;
                }

                horizontalParent = reference->parentTile();
            }
        }
    }

    Tile *verticalParent = nullptr;
    if (edges & (Qt::TopEdge | Qt::BottomEdge)) {
        if (Tile *reference = layoutReference(LayoutDirection::Vertical)) {
            QList<Tile *> affected;
            qreal deltaSize;

            if (edges.testFlags(Qt::TopEdge | Qt::BottomEdge)) {
                deltaSize = delta.y();
                affected = reference->parentTile()->childTiles();
            } else if (edges.testFlag(Qt::TopEdge)) {
                deltaSize = -delta.y();
                affected.append(reference);
                if (const auto sibling = reference->previousSibling()) {
                    affected.append(sibling);
                }
            } else if (edges.testFlag(Qt::BottomEdge)) {
                deltaSize = delta.y();
                affected.append(reference);
                if (const auto sibling = reference->nextSibling()) {
                    affected.append(sibling);
                }
            }

            if (affected.size() > 1) {
                const RectF parentRectInPixels = reference->parentTile()->absoluteGeometry();

                qreal allowedDeltaInPercents = deltaSize;
                if (deltaSize <= 0) {
                    const qreal minHeightInPercents = reference->minHeightInPixels() / parentRectInPixels.height();
                    const qreal slack = std::min(0.0, minHeightInPercents - reference->m_coverage.height());
                    allowedDeltaInPercents = std::max(allowedDeltaInPercents, slack);
                } else {
                    for (Tile *tile : std::as_const(affected)) {
                        if (tile != reference) {
                            const qreal minHeightInPercents = tile->minHeightInPixels() / parentRectInPixels.height();
                            const qreal slack = std::max(0.0, tile->m_coverage.height() - minHeightInPercents);
                            allowedDeltaInPercents = std::min(allowedDeltaInPercents, slack);
                        }
                    }
                }

                for (Tile *tile : std::as_const(affected)) {
                    const qreal adjustment = tile == reference
                        ? allowedDeltaInPercents
                        : -allowedDeltaInPercents / (affected.size() - 1);

                    tile->m_coverage.rheight() += adjustment;
                }

                verticalParent = reference->parentTile();
            }
        }
    }

    if (horizontalParent && verticalParent) {
        if (horizontalParent == m_parentTile) {
            // The vertical layout tile is higher in the tile tree.
            verticalParent->arrange();
        } else {
            horizontalParent->arrange();
        }

        return;
    }

    if (horizontalParent) {
        horizontalParent->arrange();
    } else if (verticalParent) {
        verticalParent->arrange();
    }
}

void Tile::resizeInPercents(const QSizeF &size)
{
    const QPointF delta(size.width() - m_coverage.width(),
                        size.height() - m_coverage.height());
    if (!delta.isNull()) {
        resizeByPercents(delta, Qt::LeftEdge | Qt::TopEdge | Qt::RightEdge | Qt::BottomEdge);
    }
}

void Tile::resizeHorizontallyInPercents(qreal width)
{
    const QPointF delta(width - m_coverage.width(), 0);
    if (!delta.isNull()) {
        resizeByPercents(delta, Qt::LeftEdge | Qt::RightEdge);
    }
}

void Tile::resizeVerticallyInPercents(qreal height)
{
    const QPointF delta(0, height - m_coverage.height());
    if (!delta.isNull()) {
        resizeByPercents(delta, Qt::TopEdge | Qt::BottomEdge);
    }
}

void Tile::arrange()
{
    if (quickTileMode() != QuickTileFlag::Custom) {
        return;
    }

    if (!isLayout()) {
        return;
    }

    if (m_layoutDirection == LayoutDirection::Horizontal) {
        qreal occupiedCoverage = 0;
        int emptyCount = 0;
        for (Tile *childTile : std::as_const(m_children)) {
            if (childTile->m_coverage.width()) {
                occupiedCoverage += childTile->m_coverage.width();
            } else {
                emptyCount++;
            }
        }

        qreal totalCoverage = 0;
        for (Tile *childTile : std::as_const(m_children)) {
            if (!childTile->m_coverage.width()) {
                qreal suggestedCoverage;
                if (occupiedCoverage < 0.9) {
                    suggestedCoverage = (1.0 - occupiedCoverage) / emptyCount;
                } else {
                    suggestedCoverage = 1.0 / m_children.size();
                }
                childTile->m_coverage.rwidth() = suggestedCoverage;
            }
            totalCoverage += childTile->m_coverage.width();
        }

        for (Tile *childTile : std::as_const(m_children)) {
            childTile->m_coverage.rwidth() /= totalCoverage;
            childTile->m_coverage.rheight() = 1;
        }

        qreal left = m_relativeGeometry.left();
        for (Tile *childTile : std::as_const(m_children)) {
            const RectF geometry(left,
                                 m_relativeGeometry.top(),
                                 m_relativeGeometry.width() * childTile->m_coverage.width(),
                                 m_relativeGeometry.height());
            left = geometry.right();

            childTile->setRelativeGeometry(geometry);
        }
    } else {
        qreal occupiedCoverage = 0;
        int emptyCount = 0;
        for (Tile *childTile : std::as_const(m_children)) {
            if (childTile->m_coverage.height()) {
                occupiedCoverage += childTile->m_coverage.height();
            } else {
                emptyCount++;
            }
        }

        qreal totalCoverage = 0;
        for (Tile *childTile : std::as_const(m_children)) {
            if (!childTile->m_coverage.height()) {
                qreal suggestedCoverage;
                if (occupiedCoverage < 0.9) {
                    suggestedCoverage = (1.0 - occupiedCoverage) / emptyCount;
                } else {
                    suggestedCoverage = 1.0 / m_children.size();
                }
                childTile->m_coverage.rheight() = suggestedCoverage;
            }
            totalCoverage += childTile->m_coverage.height();
        }

        for (Tile *childTile : std::as_const(m_children)) {
            childTile->m_coverage.rwidth() = 1;
            childTile->m_coverage.rheight() /= totalCoverage;
        }

        qreal top = m_relativeGeometry.top();
        for (Tile *childTile : std::as_const(m_children)) {
            const RectF geometry(m_relativeGeometry.left(),
                                 top,
                                 m_relativeGeometry.width(),
                                 m_relativeGeometry.height() * childTile->m_coverage.height());
            top = geometry.bottom();

            childTile->setRelativeGeometry(geometry);
        }
    }

    for (Tile *childTile : std::as_const(m_children)) {
        childTile->arrange();
    }

    Q_EMIT layoutModified();
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

Tile *Tile::createChildAt(int position)
{
    Tile *child = new Tile(m_tiling, this);
    connect(child, &Tile::layoutModified, this, &Tile::layoutModified);
    insertChild(position, child);
    return child;
}

Tile *Tile::createChildAt(const RectF &relativeGeometry, LayoutDirection direction, int position)
{
    Tile *child = new Tile(m_tiling, this);
    connect(child, &Tile::layoutModified, this, &Tile::layoutModified);

    if (!relativeGeometry.isEmpty() && !m_relativeGeometry.isEmpty()) {
        child->setCoverage(
            QSizeF(relativeGeometry.width() / m_relativeGeometry.width(),
                   relativeGeometry.height() / m_relativeGeometry.height()));
    }

    child->setRelativeGeometry(relativeGeometry);
    child->setLayoutDirection(direction);

    insertChild(position, child);
    return child;
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
            prev->m_coverage.rwidth() += m_coverage.width() / 2;
            next->m_coverage.rwidth() += m_coverage.width() / 2;
        } else if (prev) {
            prev->m_coverage.rwidth() += m_coverage.width();
        } else if (next) {
            next->m_coverage.rwidth() += m_coverage.width();
        }
    } else if (parentTile->layoutDirection() == LayoutDirection::Vertical) {
        if (prev && next) {
            prev->m_coverage.rheight() += m_coverage.height() / 2;
            next->m_coverage.rheight() += m_coverage.height() / 2;
        } else if (prev) {
            prev->m_coverage.rheight() += m_coverage.height();
        } else if (next) {
            next->m_coverage.rheight() += m_coverage.height();
        }
    }

    arrange();

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

QList<Tile *> Tile::splitFirst(KWin::Tile::LayoutDirection newDirection)
{
    if (!m_children.isEmpty()) {
        return {};
    }

    QList<Tile *> splitTiles;

    if (m_parentTile && m_parentTile->layoutDirection() == newDirection) {
        const int referenceIndex = m_parentTile->childTiles().indexOf(this);
        splitTiles << this;
        splitTiles << m_parentTile->createChildAt(referenceIndex + 1);
        m_parentTile->arrange();
    } else {
        setLayoutDirection(newDirection);

        splitTiles << createChildAt(childCount());
        splitTiles << createChildAt(childCount());
        arrange();

        const auto windows = m_windows;
        for (Window *window : windows) {
            splitTiles[0]->manage(window);
        }
    }

    return splitTiles;
}

QList<Tile *> Tile::splitLast(KWin::Tile::LayoutDirection newDirection)
{
    if (!m_children.isEmpty()) {
        return {};
    }

    QList<Tile *> splitTiles;

    if (m_parentTile && m_parentTile->layoutDirection() == newDirection) {
        const int referenceIndex = m_parentTile->childTiles().indexOf(this);
        splitTiles << this;
        splitTiles << m_parentTile->createChildAt(referenceIndex);
        m_parentTile->arrange();
    } else {
        setLayoutDirection(newDirection);

        splitTiles << createChildAt(childCount());
        splitTiles << createChildAt(childCount());
        arrange();

        const auto windows = m_windows;
        for (Window *window : windows) {
            splitTiles[1]->manage(window);
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
    setCoverage({1, 1});
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

QSizeF Tile::coverage() const
{
    return m_coverage;
}

void Tile::setCoverage(const QSizeF &size)
{
    m_coverage = size;
}

qreal Tile::minWidthInPixels() const
{
    qreal minWidth = 100;
    for (Window *window : std::as_const(m_windows)) {
        minWidth = std::max(minWidth, window->minSize().width() + 2 * m_padding);
    }
    return minWidth;
}

qreal Tile::minHeightInPixels() const
{
    qreal minHeight = 50;
    for (Window *window : std::as_const(m_windows)) {
        minHeight = std::max(minHeight, window->minSize().height() + 2 * m_padding);
    }
    return minHeight;
}

} // namespace KWin

#include "moc_tile.cpp"
