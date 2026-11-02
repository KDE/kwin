/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2022 Marco Martin <mart@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <qqmlregistration.h>

#include <QAbstractItemModel>
#include <QPointer>

namespace KWin
{

class Tile;

class TileModel : public QAbstractItemModel
{
    Q_OBJECT
    QML_ELEMENT

    Q_PROPERTY(KWin::Tile *tile READ tile WRITE setTile NOTIFY tileChanged)

public:
    enum Roles {
        TileRole = Qt::UserRole + 1,
    };
    explicit TileModel(QObject *parent = nullptr);

    Tile *tile() const;
    void setTile(Tile *tile);

    // QAbstractItemModel overrides
    QHash<int, QByteArray> roleNames() const override;
    QVariant data(const QModelIndex &index, int role) const override;
    Qt::ItemFlags flags(const QModelIndex &index) const override;
    QModelIndex index(int row, int column,
                      const QModelIndex &parent = QModelIndex()) const override;
    QModelIndex parent(const QModelIndex &index) const override;
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;

Q_SIGNALS:
    void tileChanged();

private:
    void beginInsertTile(Tile *childTile, int position);
    void endInsertTile();
    void beginRemoveTile(Tile *childTile);
    void endRemoveTile();

    Tile *tileFromModelIndex(const QModelIndex &index) const;

    void watch(Tile *tile);
    void unwatch(Tile *tile);

    void watchTree(Tile *tile);
    void unwatchTree(Tile *tile);

    QPointer<Tile> m_rootTile;
};

} // namespace KWin
