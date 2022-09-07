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

QSizeF Tile::s_minimumSize = QSizeF(0.1, 0.1);

Tile::Tile(TileManager *tiling, Tile *parent)
    : QObject(parent)
    , m_parentTile(parent)
    , m_tiling(tiling)
{
    connect(m_tiling->output(), &Output::informationChanged, this, &Tile::absoluteGeometryChanged);
    connect(m_tiling->output(), &Output::informationChanged, this, &Tile::windowGeometryChanged);
    if (Workspace::self()) {
        connect(Workspace::self(), &Workspace::configChanged, this, &Tile::windowGeometryChanged);
    } else {
        connect(kwinApp(), &Application::workspaceCreated, this, [this]() {
            connect(m_tiling->output(), &Output::informationChanged, this, &Tile::windowGeometryChanged);
            Q_EMIT windowGeometryChanged();
        });
    }
}

Tile::~Tile()
{
    if (m_parentTile) {
        m_parentTile->removeChild(this);
    }
}

bool Tile::supportsResizeGravity(Gravity gravity)
{
    if (!m_parentTile) {
        return false;
    }

    switch (gravity) {
    case Gravity::None:
        return true;
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
    default:
        return false;
    }
}

void Tile::setGeometryFromWindow(const QRectF &geom)
{
    setGeometryFromAbsolute(geom + m_padding);
}

void Tile::setGeometryFromAbsolute(const QRectF &geom)
{
    const auto outGeom = m_tiling->output()->geometry();
    const QRectF relGeom((geom.x() - outGeom.x()) / outGeom.width(),
                         (geom.y() - outGeom.y()) / outGeom.height(),
                         geom.width() / outGeom.width(),
                         geom.height() / outGeom.height());

    setRelativeGeometry(relGeom);
}

void Tile::setRelativeGeometry(const QRectF &geom)
{
    if (m_relativeGeometry == geom) {
        return;
    }

    m_relativeGeometry = geom;

    Q_EMIT relativeGeometryChanged(geom);
    Q_EMIT absoluteGeometryChanged();
    Q_EMIT windowGeometryChanged();
}

QRectF Tile::relativeGeometry() const
{
    return m_relativeGeometry;
}

QRectF Tile::absoluteGeometry() const
{
    const auto geom = m_tiling->output()->geometry(); // workspace()->clientArea(MaximizeArea, m_tiling->output(), VirtualDesktopManager::self()->currentDesktop());
    return QRectF(qRound(geom.x() + m_relativeGeometry.x() * geom.width()),
                  qRound(geom.y() + m_relativeGeometry.y() * geom.height()),
                  qRound(m_relativeGeometry.width() * geom.width()),
                  qRound(m_relativeGeometry.height() * geom.height()));
}

QRectF Tile::windowGeometry() const
{
    const auto geom = absoluteGeometry();
    return geom.intersected(workspace()->clientArea(MaximizeArea, m_tiling->output(), VirtualDesktopManager::self()->currentDesktop())) - m_padding;
}

QRectF Tile::maximizedWindowGeometry() const
{
    const auto geom = absoluteGeometry();
    return geom.intersected(workspace()->clientArea(MaximizeArea, m_tiling->output(), VirtualDesktopManager::self()->currentDesktop()));
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

QMarginsF Tile::padding() const
{
    return m_padding;
}

void Tile::setPadding(const QMarginsF &padding)
{
    if (m_padding == padding) {
        return;
    }

    m_padding = padding;
    Q_EMIT absoluteGeometryChanged();
}

void Tile::setMaximizeArea(bool isMaximize)
{
    if (isMaximize == m_isMaximizeArea) {
        return;
    }
    m_isMaximizeArea = isMaximize;
    Q_EMIT isMaximizeAreaChanged(isMaximize);
}

bool Tile::isMaximizeArea() const
{
    return m_isMaximizeArea;
}

void Tile::resizeByPixels(qreal delta, Qt::Edge edge)
{
    if (!m_parentTile) {
        return;
    }

    const auto outGeom = m_tiling->output()->geometry();
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

void Tile::insertChild(int position, Tile *item)
{
    const bool wasEmpty = m_children.isEmpty();
    if (position < 0) {
        m_children.append(item);
    } else {
        m_children.insert(qBound(0, position, m_children.length()), item);
    }
    if (wasEmpty) {
        Q_EMIT isLayoutChanged(true);
    }
    Q_EMIT childTilesChanged();
}

void Tile::destroyChild(Tile *tile)
{
    removeChild(tile);
    tile->deleteLater();
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
    return m_children.toList();
}

Tile *Tile::childTile(int row)
{
    if (row < 0 || row >= m_children.size())
        return nullptr;
    return m_children.at(row);
}

int Tile::childCount() const
{
    return m_children.count();
}

QList<Tile *> Tile::descendants() const
{
    QList<Tile *> tiles;
    for (auto *t : m_children) {
        tiles << t << t->descendants();
    }
    return tiles;
}

Tile *Tile::parentTile() const
{
    return m_parentTile;
}

TileManager *Tile::manager() const
{
    return m_tiling;
}

int Tile::row() const
{
    if (m_parentTile) {
        return m_parentTile->m_children.indexOf(const_cast<Tile *>(this));
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
        return m_parentTile->childTiles()[r - 1];
    }
}

} // namespace KWin

#include "moc_tile.cpp"
