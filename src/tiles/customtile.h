/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2022 Marco Martin <mart@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "tile.h"

#include <kwin_export.h>

namespace KWin
{

class KWIN_EXPORT CustomTile : public Tile
{
    Q_OBJECT
    Q_PROPERTY(KWin::Tile::LayoutDirection layoutDirection READ layoutDirection WRITE setLayoutDirection NOTIFY layoutDirectionChanged)

public:
    CustomTile(TileManager *tiling, CustomTile *parentItem = nullptr);

    CustomTile *createChildAt(const QRectF &relativeGeometry, LayoutDirection direction, int position);

    void setRelativeGeometry(const QRectF &geom) override;
    bool supportsResizeGravity(KWin::Gravity gravity) override;

    /**
     * move a floating tile by an amount of pixels. not supported on horizontal and vertical layouts
     */
    Q_INVOKABLE void moveByPixels(const QPointF &delta);
    Q_INVOKABLE void remove();
    /**
     * Splits the current tile, either creating two children or a new sibling
     * @returns the two new tiles created by splitting this one
     */
    Q_INVOKABLE QList<CustomTile *> split(KWin::Tile::LayoutDirection newDirection);

    void setLayoutDirection(Tile::LayoutDirection dir);
    // Own direction
    Tile::LayoutDirection layoutDirection() const;

    CustomTile *nextTileAt(Qt::Edge edge) const;

    CustomTile *nextNonLayoutTileAt(Qt::Edge edge) const;

Q_SIGNALS:
    void layoutDirectionChanged(Tile::LayoutDirection direction);
    void layoutModified();

private:
    Tile::LayoutDirection m_layoutDirection = LayoutDirection::Floating;
    bool m_geometryLock = false;
};

class RootTile : public CustomTile
{
    Q_OBJECT
public:
    RootTile(TileManager *tiling);
};

KWIN_EXPORT QDebug operator<<(QDebug debug, const CustomTile *tile);

} // namespace KWin
