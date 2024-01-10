/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2022 Marco Martin <mart@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "tiles/customtile.h"
#include "tiles/tile.h"
#include <kwin_export.h>

#include <QAbstractItemModel>
#include <QObject>
#include <QRectF>

#include <QJsonValue>

class QTimer;

namespace KWin
{

class Output;
class CustomTile;
class TileManager;

/**
 * Custom tiling zones management per output.
 */
class KWIN_EXPORT TileModel : public QAbstractItemModel
{
    Q_OBJECT

public:
    enum Roles {
        TileRole = Qt::UserRole + 1
    };
    explicit TileModel(TileManager *parent = nullptr);
    ~TileModel() override;

    // QAbstractItemModel overrides
    QHash<int, QByteArray> roleNames() const override;
    QVariant data(const QModelIndex &index, int role) const override;
    Qt::ItemFlags flags(const QModelIndex &index) const override;
    QModelIndex index(int row, int column,
                      const QModelIndex &parent = QModelIndex()) const override;
    QModelIndex parent(const QModelIndex &index) const override;
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;

private:
    void beginInsertTile(CustomTile *tile, int position);
    void endInsertTile();
    void beginRemoveTile(CustomTile *tile);
    void endRemoveTile();

    // Not an uinique_pointer as the model is child of the manager so would cause a cyclic delete
    TileManager *m_tileManager;
    friend class CustomTile;

    Q_DISABLE_COPY(TileModel)
};

} // namespace KWin
