/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2022 Marco Martin <mart@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "tilemodel.h"
#include "tiles/tilemanager.h"
#include "workspace.h"

#include <KConfigGroup>
#include <KLocalizedString>
#include <KSharedConfig>

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTimer>

namespace KWin
{

TileModel::TileModel(TileManager *parent)
    : QAbstractItemModel(parent)
    , m_tileManager(parent)
{
}

TileModel::~TileModel()
{
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
        return QVariant::fromValue(static_cast<CustomTile *>(index.internalPointer()));
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

    CustomTile *parentItem;

    if (!parent.isValid()) {
        parentItem = m_tileManager->rootTile();
    } else {
        parentItem = static_cast<CustomTile *>(parent.internalPointer());
    }

    CustomTile *childItem = static_cast<CustomTile *>(parentItem->childTile(row));
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

    CustomTile *childItem = static_cast<CustomTile *>(index.internalPointer());
    CustomTile *parentItem = static_cast<CustomTile *>(childItem->parentTile());

    if (!parentItem || parentItem == m_tileManager->rootTile()) {
        return QModelIndex();
    }

    return createIndex(parentItem->row(), 0, parentItem);
}

int TileModel::rowCount(const QModelIndex &parent) const
{
    Tile *parentItem;
    if (parent.column() > 0) {
        return 0;
    }

    if (!parent.isValid()) {
        parentItem = m_tileManager->rootTile();
    } else {
        parentItem = static_cast<CustomTile *>(parent.internalPointer());
    }

    return parentItem->childCount();
}

int TileModel::columnCount(const QModelIndex &parent) const
{
    return 1;
}

void TileModel::beginInsertTile(CustomTile *tile, int position)
{
    Q_ASSERT(position >= 0);
    CustomTile *parentTile = static_cast<CustomTile *>(tile->parentTile());
    Q_ASSERT(parentTile);

    auto index = parentTile == m_tileManager->rootTile() ? QModelIndex() : createIndex(parentTile->row(), 0, parentTile);
    const int pos = std::clamp(position, 0, parentTile->childCount());

    beginInsertRows(index, pos, pos);
}

void TileModel::endInsertTile()
{
    endInsertRows();
}

void TileModel::beginRemoveTile(CustomTile *tile)
{
    const auto parentTile = static_cast<CustomTile *>(tile->parentTile());
    if (!parentTile) {
        qCWarning(KWIN_CORE) << "Can't remove the root tile";
        return;
    }

    QModelIndex parentIndex = parentTile == m_tileManager->rootTile() ? QModelIndex() : createIndex(parentTile->row(), 0, parentTile);
    beginRemoveRows(parentIndex, tile->row(), tile->row());
}

void TileModel::endRemoveTile()
{
    endRemoveRows();
}

} // namespace KWin

#include "moc_tilemodel.cpp"
