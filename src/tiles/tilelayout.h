/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2024 Marco Martin <mart@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QObject>

#include "tile.h"
#include "window.h"

#include <kwin_export.h>

namespace KWin
{

/**
 * Custom tiling zones management per output.
 */
class KWIN_EXPORT TileLayout : public QObject
{
    Q_OBJECT

public:
    explicit TileLayout(QObject *parent = nullptr);
    ~TileLayout() override;

    void setAssociation(Window *window, Tile *tile);
    void removeAssociation(Window *window, Tile *tile);

    Tile *tileForWindow(Window *window) const;
    QList<Window *> windowsForTile(Tile *tile) const;

    void forgetWindow(Window *window);
    void forgetTile(Tile *tile);

    QHash<Window *, Tile *> windowTileMap() const;

Q_SIGNALS:
    void windowAssociated(Window *window, KWin::Tile *tile);
    void associationRemoved(Window *window, KWin::Tile *tile);

private:
    QHash<Window *, Tile *> m_tileForWindow;
    QMultiHash<Tile *, Window *> m_windowsForTile;
};

KWIN_EXPORT QDebug operator<<(QDebug debug, const TileLayout *tileLayout);

} // namespace KWin
