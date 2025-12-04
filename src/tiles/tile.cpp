/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2022 Marco Martin <mart@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "tile.h"
#include "core/output.h"
#include "effect/globals.h"
#include "tilemanager.h"
#include "virtualdesktops.h"
#include "window.h"
#include "workspace.h"

#include <cmath>

namespace KWin
{

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
    return m_desktop == VirtualDesktopManager::self()->currentDesktop();
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
    case Gravity::None:
    default:
        return false;
    }
}

void Tile::setGeometryFromWindow(const RectF &geom)
{
    setGeometryFromAbsolute(geom + QMarginsF(m_padding, m_padding, m_padding, m_padding));
}

void Tile::setGeometryFromAbsolute(const RectF &geom)
{
    const RectF outGeom = m_tiling->output()->geometryF();
    const RectF relGeom((geom.x() - outGeom.x()) / outGeom.width(),
                        (geom.y() - outGeom.y()) / outGeom.height(),
                        geom.width() / outGeom.width(),
                        geom.height() / outGeom.height());

    setRelativeGeometry(relGeom);
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

RectF Tile::relativeGeometry() const
{
    return m_relativeGeometry;
}

RectF Tile::absoluteGeometry() const
{
    const RectF geom = m_tiling->output()->geometryF();
    return RectF(std::round(geom.x() + m_relativeGeometry.x() * geom.width()),
                 std::round(geom.y() + m_relativeGeometry.y() * geom.height()),
                 std::round(m_relativeGeometry.width() * geom.width()),
                 std::round(m_relativeGeometry.height() * geom.height()));
}

RectF Tile::absoluteGeometryInScreen() const
{
    const RectF geom = m_tiling->output()->geometryF();
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
    return geom.intersected(workspace()->clientArea(MaximizeArea, m_tiling->output(), m_desktop)) - effectiveMargins;
}

RectF Tile::maximizedWindowGeometry() const
{
    const auto geom = absoluteGeometry();
    return geom.intersected(workspace()->clientArea(MaximizeArea, m_tiling->output(), m_desktop));
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

    setRelativeGeometry(m_relativeGeometry);
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

void Tile::resizeFromGravity(Gravity gravity, int x_root, int y_root)
{
    if (!m_parentTile) {
        return;
    }

    const RectF outGeom = m_tiling->output()->geometryF();
    const QPointF relativePos = QPointF((x_root - outGeom.x()) / outGeom.width(), (y_root - outGeom.y()) / outGeom.height());
    RectF newGeom = m_relativeGeometry;

    switch (gravity) {
    case Gravity::TopLeft:
        newGeom.setTopLeft(relativePos - QPointF(m_padding / outGeom.width(), m_padding / outGeom.height()));
        break;
    case Gravity::BottomRight:
        newGeom.setBottomRight(relativePos + QPointF(m_padding / outGeom.width(), m_padding / outGeom.height()));
        break;
    case Gravity::BottomLeft:
        newGeom.setBottomLeft(relativePos + QPointF(-m_padding / outGeom.width(), m_padding / outGeom.height()));
        break;
    case Gravity::TopRight:
        newGeom.setTopRight(relativePos + QPointF(m_padding / outGeom.width(), -m_padding / outGeom.height()));
        break;
    case Gravity::Top:
        newGeom.setTop(relativePos.y() - m_padding / outGeom.height());
        break;
    case Gravity::Bottom:
        newGeom.setBottom(relativePos.y() + m_padding / outGeom.height());
        break;
    case Gravity::Left:
        newGeom.setLeft(relativePos.x() - m_padding / outGeom.width());
        break;
    case Gravity::Right:
        newGeom.setRight(relativePos.x() + m_padding / outGeom.width());
        break;
    case Gravity::None:
        Q_UNREACHABLE();
        break;
    }

    setRelativeGeometry(newGeom);
}

void Tile::resizeByPixels(qreal delta, Qt::Edge edge)
{
    if (!m_parentTile) {
        return;
    }

    const auto outGeom = m_tiling->output()->geometryF();
    auto newGeom = m_relativeGeometry;

    switch (edge) {
    case Qt::LeftEdge: {
        qreal relativeDelta = delta / outGeom.width();
        newGeom.setLeft(newGeom.left() + relativeDelta);
        break;
    }
    case Qt::TopEdge: {
        qreal relativeDelta = delta / outGeom.height();
        newGeom.setTop(newGeom.top() + relativeDelta);
        break;
    }
    case Qt::RightEdge: {
        qreal relativeDelta = delta / outGeom.width();
        newGeom.setRight(newGeom.right() + relativeDelta);
        break;
    }
    case Qt::BottomEdge: {
        qreal relativeDelta = delta / outGeom.height();
        newGeom.setBottom(newGeom.bottom() + relativeDelta);
        break;
    }
    }
    setRelativeGeometry(newGeom);
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
    Q_ASSERT(position >= 0);
    const bool wasEmpty = m_children.isEmpty();
    item->setParent(this);

    m_children.insert(std::clamp<qsizetype>(position, 0, m_children.length()), item);

    if (wasEmpty) {
        Q_EMIT isLayoutChanged(true);
        auto windows = m_windows;
        for (auto *w : windows) {
            Tile *tile = m_tiling->rootTile(m_desktop)->pick(w->moveResizeGeometry().center());
            unmanage(w);
            if (tile) {
                tile->manage(w);
            }
        }
    }

    Q_EMIT childTilesChanged();
}

void Tile::destroyChild(Tile *tile)
{
    removeChild(tile);
    delete tile;
}

void Tile::removeChild(Tile *child)
{
    const bool wasEmpty = m_children.isEmpty();
    const int idx = m_children.indexOf(child);
    m_children.removeAll(child);
    if (m_children.isEmpty() && !wasEmpty) {
        Q_EMIT isLayoutChanged(false);
    }
    if (idx > -1) {
        for (int i = idx; i < m_children.count(); ++i) {
            Q_EMIT m_children[i]->rowChanged(i);
        }
    }
    Q_EMIT childTilesChanged();
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

TileManager *Tile::manager() const
{
    return m_tiling;
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

} // namespace KWin

#include "moc_tile.cpp"
