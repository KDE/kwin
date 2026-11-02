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

/*!
 * \qmluncreatabletype CustomTile
 * \inqmlmodule org.kde.kwin
 *
 * \brief The CustomTile represents a scriptable tile.
 */
class KWIN_EXPORT CustomTile : public Tile
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("")

    /*!
     * \qmlproperty Tile::LayoutDirection CustomTile::layoutDirection
     *
     * This property specifies the direction in which children tiles are arranged.
     */
    Q_PROPERTY(KWin::Tile::LayoutDirection layoutDirection READ layoutDirection WRITE setLayoutDirection NOTIFY layoutDirectionChanged)

public:
    CustomTile(TileManager *tiling, CustomTile *parentItem = nullptr);

    CustomTile *createChildAt(const RectF &relativeGeometry, LayoutDirection direction, int position);

    void setRelativeGeometry(const RectF &geom) override;
    bool supportsResizeGravity(KWin::Gravity gravity) override;

    /*!
     * Move a floating tile by an amount of pixels. not supported on horizontal and vertical layouts
     */
    Q_INVOKABLE void moveByPixels(const QPointF &delta);

    /*!
     * Remove this tile from the tile tree.
     */
    Q_INVOKABLE void remove();

    /*!
     * Splits the current tile along the specified \a newDirection, either creating two children
     * or a new sibling. Returns the two new tiles created by splitting this one.
     */
    Q_INVOKABLE QList<KWin::CustomTile *> split(KWin::Tile::LayoutDirection newDirection);

    void setLayoutDirection(Tile::LayoutDirection dir);
    Tile::LayoutDirection layoutDirection() const;

    CustomTile *nextTileAt(Qt::Edge edge) const;

    CustomTile *nextNonLayoutTileAt(Qt::Edge edge) const;

Q_SIGNALS:
    void layoutDirectionChanged(KWin::Tile::LayoutDirection direction);
    void layoutModified();

private:
    Tile::LayoutDirection m_layoutDirection = LayoutDirection::Floating;
    bool m_geometryLock = false;
};

class KWIN_EXPORT RootTile : public CustomTile
{
    Q_OBJECT

public:
    RootTile(TileManager *tiling, VirtualDesktop *desktop);

    Tile *tileForWindow(Window *window);

    Q_INVOKABLE KWin::Tile *pick(const QPointF &point) const;
    Q_INVOKABLE KWin::Tile *pick(qreal x, qreal y) const;
};

KWIN_EXPORT QDebug operator<<(QDebug debug, const CustomTile *tile);

} // namespace KWin
