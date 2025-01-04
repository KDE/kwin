/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2022 Marco Martin <mart@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "tile.h"
#include "core/output.h"
#include "tilemanager.h"
#include "virtualdesktops.h"
#include "window.h"
#include "workspace.h"

#include <cmath>

namespace KWin
{

QSizeF Tile::s_minimumSize = QSizeF(0.15, 0.15);

Tile::Tile(TileManager *tiling, Tile *parent)
    : QObject(parent)
    , m_parentTile(parent)
    , m_tiling(tiling)
{
    if (m_parentTile) {
        m_padding = m_parentTile->padding();
    }
    connect(Workspace::self(), &Workspace::configChanged, this, &Tile::windowGeometryChanged);
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
    for (auto *w : std::as_const(m_windows)) {
        Tile *tile = m_tiling->bestTileForPosition(w->moveResizeGeometry().center());
        w->requestTile(tile);
    }
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

void Tile::setGeometryFromWindow(const QRectF &geom)
{
    setGeometryFromAbsolute(geom + QMarginsF(m_padding, m_padding, m_padding, m_padding));
}

void Tile::setGeometryFromAbsolute(const QRectF &geom)
{
    const QRectF outGeom = m_tiling->output()->geometryF();
    const QRectF relGeom((geom.x() - outGeom.x()) / outGeom.width(),
                         (geom.y() - outGeom.y()) / outGeom.height(),
                         geom.width() / outGeom.width(),
                         geom.height() / outGeom.height());

    setRelativeGeometry(relGeom);
}

void Tile::setRelativeGeometry(const QRectF &geom)
{
    QRectF constrainedGeom = geom;
    constrainedGeom.setWidth(std::max(constrainedGeom.width(), s_minimumSize.width()));
    constrainedGeom.setHeight(std::max(constrainedGeom.height(), s_minimumSize.height()));

    if (m_relativeGeometry == constrainedGeom) {
        return;
    }

    m_relativeGeometry = constrainedGeom;

    Q_EMIT relativeGeometryChanged();
    Q_EMIT absoluteGeometryChanged();
    Q_EMIT windowGeometryChanged();

    for (auto *w : std::as_const(m_windows)) {
        w->moveResize(windowGeometry());
    }
}

QRectF Tile::relativeGeometry() const
{
    return m_relativeGeometry;
}

QRectF Tile::absoluteGeometry() const
{
    const QRectF geom = m_tiling->output()->geometryF();
    return QRectF(std::round(geom.x() + m_relativeGeometry.x() * geom.width()),
                  std::round(geom.y() + m_relativeGeometry.y() * geom.height()),
                  std::round(m_relativeGeometry.width() * geom.width()),
                  std::round(m_relativeGeometry.height() * geom.height()));
}

QRectF Tile::absoluteGeometryInScreen() const
{
    const QRectF geom = m_tiling->output()->geometryF();
    return QRectF(std::round(m_relativeGeometry.x() * geom.width()),
                  std::round(m_relativeGeometry.y() * geom.height()),
                  std::round(m_relativeGeometry.width() * geom.width()),
                  std::round(m_relativeGeometry.height() * geom.height()));
}

QRectF Tile::windowGeometry() const
{
    // Apply half padding between tiles and full against the screen edges
    QMarginsF effectiveMargins;
    effectiveMargins.setLeft(m_relativeGeometry.left() > 0.0 ? m_padding / 2.0 : m_padding);
    effectiveMargins.setTop(m_relativeGeometry.top() > 0.0 ? m_padding / 2.0 : m_padding);
    effectiveMargins.setRight(m_relativeGeometry.right() < 1.0 ? m_padding / 2.0 : m_padding);
    effectiveMargins.setBottom(m_relativeGeometry.bottom() < 1.0 ? m_padding / 2.0 : m_padding);

    const auto geom = absoluteGeometry();
    return geom.intersected(workspace()->clientArea(MaximizeArea, m_tiling->output(), VirtualDesktopManager::self()->currentDesktop())) - effectiveMargins;
}

QRectF Tile::maximizedWindowGeometry() const
{
    const auto geom = absoluteGeometry();
    return geom.intersected(workspace()->clientArea(MaximizeArea, m_tiling->output(), VirtualDesktopManager::self()->currentDesktop()));
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
    for (auto *w : std::as_const(m_windows)) {
        w->moveResize(windowGeometry());
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

void Tile::resizeFromGravity(Gravity gravity, int x_root, int y_root)
{
    if (!m_parentTile) {
        return;
    }

    const QRectF outGeom = m_tiling->output()->geometryF();
    const QPointF relativePos = QPointF((x_root - outGeom.x()) / outGeom.width(), (y_root - outGeom.y()) / outGeom.height());
    QRectF newGeom = m_relativeGeometry;

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

void Tile::addWindow(Window *window)
{
    if (!window->isClient()) {
        return;
    }
    if (!m_windows.contains(window)) {
        // Don't resize the window here, it was already resized in the configureEvent
        m_windows.append(window);
        Q_EMIT windowAdded(window);
        Q_EMIT windowsChanged();
    }
}

void Tile::removeWindow(Window *window)
{
    // We already ensure there is a single copy of window in m_windows
    if (m_windows.removeOne(window)) {
        Q_EMIT windowRemoved(window);
        Q_EMIT windowsChanged();
    }
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
        for (auto *w : std::as_const(m_windows)) {
            Tile *tile = m_tiling->bestTileForPosition(w->moveResizeGeometry().center());
            w->requestTile(tile);
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

void Tile::visitDescendants(std::function<void(const Tile *child)> callback) const
{
    callback(this);
    for (const Tile *child : m_children) {
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
