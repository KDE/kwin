/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2022 Marco Martin <mart@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "customtile.h"
#include "quicktile.h"
#include "scripting/tilemodel.h"
#include "tile.h"
#include "utils/common.h"
#include <kwin_export.h>

#include <QAbstractItemModel>
#include <QObject>
#include <QRectF>

#include <QJsonValue>

class QTimer;

namespace KWin
{

class Output;
class Tile;
class TileModel;

/**
 * Custom tiling zones management per output.
 */
class KWIN_EXPORT TileManager : public QObject
{
    Q_OBJECT
    Q_PROPERTY(KWin::Tile *rootTile READ rootTile NOTIFY rootTileChanged)
    Q_PROPERTY(TileModel *model READ model NOTIFY modelChanged)

public:
    explicit TileManager(Output *parent = nullptr);
    ~TileManager() override;

    bool tearingDown() const;

    Output *output() const;

    Q_INVOKABLE KWin::Tile *bestTileForPosition(qreal x, qreal y); // For scripting
    RootTile *rootTile(VirtualDesktop *desktop) const;
    RootTile *rootTile() const;
    QuickRootTile *quickRootTile(VirtualDesktop *desktop) const;
    QuickRootTile *quickRootTile() const;
    KWin::Tile *quickTile(QuickTileMode mode) const;

    TileModel *model() const;

    Tile *tileForWindow(Window *window, VirtualDesktop *desktop);
    void forgetWindow(Window *window, VirtualDesktop *desktop);

Q_SIGNALS:
    void tileRemoved(KWin::Tile *tile);
    void rootTileChanged(CustomTile *rootTile);
    void modelChanged(TileModel *model);

private:
    void readSettings(RootTile *rootTile);
    void saveSettings();
    QJsonObject tileToJSon(CustomTile *parentTile);
    CustomTile *parseTilingJSon(const QJsonValue &val, const QRectF &availableArea, CustomTile *parentTile);

    Q_DISABLE_COPY(TileManager)

    Output *m_output = nullptr;
    std::unique_ptr<QTimer> m_saveTimer;

    QHash<VirtualDesktop *, RootTile *> m_rootTiles;
    QHash<VirtualDesktop *, QuickRootTile *> m_quickRootTiles;

    bool m_tearingDown = false;
    friend class CustomTile;
};

KWIN_EXPORT QDebug operator<<(QDebug debug, const TileManager *tileManager);

} // namespace KWin
