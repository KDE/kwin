/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2022 Marco Martin <mart@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "tilemodel.h"
#include "tiles/tile.h"

namespace KWin
{

TileModel::TileModel(QObject *parent)
    : QAbstractItemModel(parent)
{
}

Tile *TileModel::tile() const
{
    return m_rootTile;
}

void TileModel::setTile(Tile *tile)
{
    if (m_rootTile == tile) {
        return;
    }

    beginResetModel();

    if (m_rootTile) {
        unwatchTree(m_rootTile);
    }

    m_rootTile = tile;

    if (m_rootTile) {
        watchTree(m_rootTile);
    }

    endResetModel();

    Q_EMIT tileChanged();
}

Tile *TileModel::tileFromModelIndex(const QModelIndex &index) const
{
    return static_cast<Tile *>(index.internalPointer());
}

QHash<int, QByteArray> TileModel::roleNames() const
{
    return {
        {TileRole, QByteArrayLiteral("tile")},
    };
}

QVariant TileModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid()) {
        return QVariant();
    }

    if (role == TileRole) {
        return QVariant::fromValue(tileFromModelIndex(index));
    }

    return QVariant();
}

Qt::ItemFlags TileModel::flags(const QModelIndex &index) const
{
    if (!index.isValid()) {
        return Qt::NoItemFlags;
    }

    return QAbstractItemModel::flags(index);
}

QModelIndex TileModel::index(int row, int column, const QModelIndex &parent) const
{
    if (column > 0 || !hasIndex(row, column, parent)) {
        return QModelIndex();
    }

    Tile *parentItem;

    if (!parent.isValid()) {
        if (!m_rootTile) {
            return QModelIndex();
        }
        parentItem = m_rootTile;
    } else {
        parentItem = static_cast<Tile *>(parent.internalPointer());
    }

    Tile *childItem = parentItem->childTile(row);
    if (childItem) {
        return createIndex(row, column, childItem);
    }
    return QModelIndex();
}

QModelIndex TileModel::parent(const QModelIndex &index) const
{
    if (!index.isValid()) {
        return QModelIndex();
    }

    const Tile *childItem = tileFromModelIndex(index);
    const Tile *parentItem = childItem->parentTile();

    if (!parentItem || parentItem == m_rootTile) {
        return QModelIndex();
    }

    return createIndex(parentItem->row(), 0, parentItem);
}

int TileModel::rowCount(const QModelIndex &parent) const
{
    Tile *parentItem;
    if (parent.column() > 0 || !m_rootTile) {
        return 0;
    }

    if (!parent.isValid()) {
        parentItem = m_rootTile;
    } else {
        parentItem = static_cast<Tile *>(parent.internalPointer());
    }

    return parentItem->childCount();
}

int TileModel::columnCount(const QModelIndex &parent) const
{
    return 1;
}

void TileModel::beginInsertTile(Tile *tile, int position)
{
    watchTree(tile);

    Q_ASSERT(position >= 0);
    Tile *parentTile = static_cast<Tile *>(tile->parentTile());
    Q_ASSERT(parentTile);

    auto index = parentTile == tile->rootTile() ? QModelIndex() : createIndex(parentTile->row(), 0, parentTile);
    const int pos = std::clamp(position, 0, parentTile->childCount());

    beginInsertRows(index, pos, pos);
}

void TileModel::endInsertTile()
{
    endInsertRows();
}

void TileModel::beginRemoveTile(Tile *tile)
{
    const auto parentTile = static_cast<Tile *>(tile->parentTile());
    if (!parentTile) {
        return;
    }

    unwatchTree(tile);

    QModelIndex parentIndex = parentTile == static_cast<Tile *>(m_rootTile) ? QModelIndex() : createIndex(parentTile->row(), 0, parentTile);
    beginRemoveRows(parentIndex, tile->row(), tile->row());
}

void TileModel::endRemoveTile()
{
    endRemoveRows();
}

void TileModel::watch(Tile *tile)
{
    connect(tile, &Tile::aboutToInsertChildTile, this, &TileModel::beginInsertTile);
    connect(tile, &Tile::childTileInserted, this, &TileModel::endInsertTile);

    connect(tile, &Tile::aboutToRemoveChildTile, this, &TileModel::beginRemoveTile);
    connect(tile, &Tile::childTileRemoved, this, &TileModel::endRemoveTile);
}

void TileModel::watchTree(Tile *tile)
{
    watch(tile);

    for (const auto children = tile->childTiles(); const auto &child : children) {
        watch(child);
    }
}

void TileModel::unwatch(Tile *tile)
{
    disconnect(tile, &Tile::aboutToInsertChildTile, this, &TileModel::beginInsertTile);
    disconnect(tile, &Tile::childTileInserted, this, &TileModel::endInsertTile);

    disconnect(tile, &Tile::aboutToRemoveChildTile, this, &TileModel::beginRemoveTile);
    disconnect(tile, &Tile::childTileRemoved, this, &TileModel::endRemoveTile);
}

void TileModel::unwatchTree(Tile *tile)
{
    unwatch(tile);

    for (const auto children = tile->childTiles(); const auto &child : children) {
        unwatch(child);
    }
}

} // namespace KWin

#include "moc_tilemodel.cpp"
