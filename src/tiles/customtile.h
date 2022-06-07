/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2022 Marco Martin <mart@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef CUSTOMTILE_H
#define CUSTOMTILE_H

#include "tile.h"

#include <kwin_export.h>

namespace KWin
{

class KWIN_EXPORT CustomTile : public Tile
{
    Q_OBJECT
    Q_PROPERTY(KWin::CustomTile::LayoutDirection layoutDirection READ layoutDirection NOTIFY layoutDirectionChanged)

public:
    enum class LayoutDirection {
        Floating = 0,
        Horizontal = 1,
        Vertical = 2
    };
    Q_ENUM(LayoutDirection)

    ~CustomTile();

    CustomTile *createChild(const QRectF &relativeGeometry, LayoutDirection direction, int position);

    void setRelativeGeometry(const QRectF &geom) override;
    bool supportsResizeGravity(KWin::Gravity gravity) override;

    /**
     * move a floating tile by an amount of pixels. not supported on horizontal and vertical layouts
     */
    Q_INVOKABLE void moveByPixels(const QPointF &delta);
    Q_INVOKABLE void remove();
    Q_INVOKABLE void split(KWin::CustomTile::LayoutDirection newDirection);

    void setLayoutDirection(CustomTile::LayoutDirection dir);
    // Own direction
    CustomTile::LayoutDirection layoutDirection() const;

    CustomTile *nextTileAt(Qt::Edge edge) const;

Q_SIGNALS:
    void layoutDirectionChanged(CustomTile::LayoutDirection direction);
    void layoutModified();

protected:
    CustomTile(TileManager *tiling, CustomTile *parentItem = nullptr);

private:
    CustomTile::LayoutDirection m_layoutDirection = LayoutDirection::Floating;
    bool m_geometryLock = false;
    friend class Tile;
};

class RootTile : public CustomTile
{
    Q_OBJECT
public:
    RootTile(TileManager *tiling);
    ~RootTile();
};

KWIN_EXPORT QDebug operator<<(QDebug debug, const CustomTile *tile);

} // namespace KWin

#endif // CUSTOMTILE_H
