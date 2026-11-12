/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2022 Marco Martin <mart@kde.org>
    SPDX-FileCopyrightText: 2026 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "core/rect.h"
#include "effect/globals.h"
#include "virtualdesktops.h"
#include <kwin_export.h>

#include <QObject>

namespace KWin
{

class Gravity;
class TileManager;
class VirtualDesktop;
class Window;

/*!
 * \qmluncreatabletype Tile
 * \inqmlmodule org.kde.kwin
 *
 * \brief The Tile represents an area on the screen that can contain windows.
 *
 * A tile may contain zero, one, or more windows. When a window is placed in a tile, it will fill
 * the entire tile.
 *
 * A tile may contain other tiles as children, in which case, the children sub-divide the space of
 * the parent tile. Every screen per virtual desktop has a tile tree associated with it.
 *
 * There are several types of tiles: QuickTile, a quick tiling implementation detail, and CustomTile.
 * Tiling scripts operate with CustomTile tiles.
 */
class KWIN_EXPORT Tile : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("")

    /*!
     * \qmlproperty RectF Tile::relativeGeometry
     *
     * The geometry of the tile relative to the size of the associated output. The relative geometry
     * is normalized. \c{relativeGeometry.width} of 1 corresponds to the width of the output.
     */
    Q_PROPERTY(KWin::RectF relativeGeometry READ relativeGeometry WRITE setRelativeGeometryAndArrange NOTIFY relativeGeometryChanged)

    /*!
     * \qmlproperty RectF Tile::absoluteGeometry
     *
     * The geometry of the tile in pixels in the global coordinate space, i.e. \c{0,0} corresponds
     * to the top-left corner of the bounding rectangle of all outputs.
     */
    Q_PROPERTY(KWin::RectF absoluteGeometry READ absoluteGeometry NOTIFY absoluteGeometryChanged)

    /*!
     * \qmlproperty RectF Tile::absoluteGeometryInScreen
     *
     * The geometry of the tile in pixels in the output-local coordinate space, i.e. \c{0,0}
     * corresponds to the top-left corner of the output.
     */
    Q_PROPERTY(KWin::RectF absoluteGeometryInScreen READ absoluteGeometryInScreen NOTIFY absoluteGeometryChanged)

    /*!
     * \qmlproperty real Tile::padding
     *
     * The distance that should be maintained between tiles as well as away from the screen edges.
     * The padding is specified in the logical pixels.
     */
    Q_PROPERTY(qreal padding READ padding WRITE setPadding NOTIFY paddingChanged)

    /*!
     * \qmlproperty size Tile::minimumSize
     *
     * The minimum size that the windows in this Tile can be resized to. In the logical pixels.
     */
    Q_PROPERTY(QSizeF minimumSize READ minimumSize WRITE setMinimumSize NOTIFY minimumSizeChanged)

    /*!
     * \qmlproperty Tile Tile::parent
     *
     * The parent Tile of this Tile. This property can be \c null if this is the root Tile.
     */
    Q_PROPERTY(KWin::Tile *parent READ parentTile CONSTANT)

    /*!
     * \qmlproperty list<Tile> Tile::tiles
     *
     * The child tiles.
     */
    Q_PROPERTY(QList<KWin::Tile *> tiles READ childTiles NOTIFY childTilesChanged)

    /*!
     * \qmlproperty list<Window> Tile::windows
     *
     * The managed windows.
     */
    Q_PROPERTY(QList<KWin::Window *> windows READ windows NOTIFY windowsChanged)

    /*!
     * \qmlproperty bool Tile::canBeRemoved
     *
     * Whether this Tile can be removed.
     */
    Q_PROPERTY(bool canBeRemoved READ canBeRemoved CONSTANT)

    /*!
     * \qmlproperty Tile::LayoutDirection CustomTile::layoutDirection
     *
     * This property specifies the direction in which children tiles are arranged.
     */
    Q_PROPERTY(KWin::Tile::LayoutDirection layoutDirection READ layoutDirection NOTIFY layoutDirectionChanged)

public:
    enum class LayoutDirection {
        Horizontal = 1,
        Vertical = 2,
    };
    Q_ENUM(LayoutDirection)

    explicit Tile(TileManager *tiling, Tile *parentItem = nullptr);
    ~Tile();

    VirtualDesktop *desktop() const;
    bool isActive() const;

    void setRelativeGeometryAndArrange(const RectF &geom);
    void setRelativeGeometry(const RectF &geom);

    bool supportsResizeGravity(Gravity gravity);

    /*!
     * Resolves the requested resize gravity to edges that can move this tile.
     * If an edge touches the output boundary, the opposite inner edge is used
     * when possible.
     */
    Gravity resolveResizeGravity(Gravity gravity);

    /*!
     * Geometry of the tile in units between 0 and 1 relative to the screen geometry
     */
    RectF relativeGeometry() const;

    /*!
     * Geometry of the tile in absolute coordinates
     */
    RectF absoluteGeometry() const;

    /*!
     * Geometry of the tile in absolute coordinates, but in screen coordinates,
     * ie the top left corner of rootTile always at 0,0
     */
    RectF absoluteGeometryInScreen() const;

    /*!
     * Absolute geometry minus the padding and reserved areas such as panels
     */
    RectF windowGeometry() const;

    /*!
     * Which edges of the tile touches an edge of the screen
     */
    Qt::Edges anchors() const;

    bool isRoot() const;
    bool isLayout() const;
    bool canBeRemoved() const;

    qreal padding() const;
    void setPadding(qreal padding);

    QSizeF minimumSize() const;
    void setMinimumSize(const QSizeF &size);

    QuickTileMode quickTileMode() const;
    void setQuickTileMode(QuickTileMode mode);

    Tile *rootTile() const;

    /*!
     * All tiles directly children of this tile
     */
    QList<Tile *> childTiles() const;

    /*!
     * All tiles descendant of this tile, recursive
     */
    QList<Tile *> descendants() const;

    /*!
     * Visit all tiles descendant of this tile, recursive
     */
    void visitDescendants(std::function<void(Tile *child)> callback);

    /*!
     * \qmlmethod void Tile::resizeByPixels(real delta, enumeration edge)
     *
     * Resize this tile by the given \a delta pixels along the specified \a edge. The tile size
     * may be left unchanged due to size constraints, etc.
     */
    Q_INVOKABLE void resizeByPixels(qreal delta, Qt::Edge edge);
    Q_INVOKABLE void resizeByPixels(const QPointF &delta, Qt::Edges edges);
    void resizeByPixels(const QPointF &delta, Gravity gravity);

    /*!
     * \qmlmethod void Tile::manage(Window window)
     *
     * Put the specified \a window to this tile. The window will be resized to match this tile.
     *
     * If the window is in another tile, it will be evacuated from that tile before getting moved
     * to this tile.
     */
    Q_INVOKABLE bool manage(KWin::Window *window);

    /*!
     * \qmlmethod void Tile::unmanage(Window window)
     *
     * Remove the specified \a window from this tile. The window will be resized to match its size
     * before it was put in the tile.
     */
    Q_INVOKABLE bool unmanage(KWin::Window *window);

    /*!
     * Remove this tile from the tile tree.
     */
    Q_INVOKABLE void remove();

    /*!
     * Splits the current tile along the specified \a newDirection, either creating two children
     * or a new sibling. Returns the two new tiles created by splitting this one.
     */
    Q_INVOKABLE QList<KWin::Tile *> split(KWin::Tile::LayoutDirection newDirection);

    void forget(Window *window);
    QList<KWin::Window *> windows() const;

    int row() const;
    int childCount() const;
    Tile *childTile(int row);
    Tile *nextSibling() const;
    Tile *previousSibling() const;
    Tile *parentTile() const;

    void setLayoutDirection(LayoutDirection dir);
    LayoutDirection layoutDirection() const;

    Tile *nextTileAt(Qt::Edge edge) const;

    Tile *nextNonLayoutTileAt(Qt::Edge edge) const;

    Tile *createChildAt(const RectF &relativeGeometry, int position)
    {
        return createChildAt(relativeGeometry, LayoutDirection::Horizontal, position);
    }

    Tile *createChildAt(const RectF &relativeGeometry, LayoutDirection direction, int position)
    {
        Tile *t = new Tile(m_tiling, this);
        connect(t, &Tile::layoutModified, this, &Tile::layoutModified);
        t->setRelativeGeometry(relativeGeometry);
        t->setLayoutDirection(direction);
        insertChild(position, t);
        return t;
    }

Q_SIGNALS:
    void activeChanged(bool active);
    void relativeGeometryChanged();
    void absoluteGeometryChanged();
    void windowGeometryChanged();
    void paddingChanged(qreal padding);
    void minimumSizeChanged(const QSizeF &size);
    void childTilesChanged();
    void windowsChanged();

    /*!
     * \qmlsignal windowAdded(Window window)
     *
     * This signal is emitted when the Tile starts managing a window.
     *
     * \sa manage()
     */
    void windowAdded(KWin::Window *window);

    /*!
     * \qmlsignal windowRemoved(Window window)
     *
     * This signal is emitted when the Tile no longer manages a window.
     *
     * \sa unmanage()
     */
    void windowRemoved(KWin::Window *window);

    void aboutToInsertChildTile(KWin::Tile *child, int index);
    void childTileInserted();
    void aboutToRemoveChildTile(KWin::Tile *child);
    void childTileRemoved();

    void layoutDirectionChanged(KWin::Tile::LayoutDirection direction);
    void layoutModified();

protected:
    void insertChild(int position, Tile *item);
    void removeChild(Tile *child);

    void add(Window *window);
    bool remove(Window *window);

    QList<Tile *> m_children;
    QList<Window *> m_windows;
    Tile *m_parentTile;

    VirtualDesktop *m_desktop = nullptr;
    TileManager *m_tiling;
    RectF m_relativeGeometry;
    QSizeF m_minimumSize = QSizeF(0.15, 0.15);
    LayoutDirection m_layoutDirection = LayoutDirection::Horizontal;
    QuickTileMode m_quickTileMode = QuickTileFlag::Custom;
    qreal m_padding = 4.0;
    bool m_geometryLock = false;
};

class KWIN_EXPORT RootTile : public Tile
{
    Q_OBJECT

public:
    RootTile(TileManager *tiling, VirtualDesktop *desktop);

    Tile *tileForWindow(Window *window);

    Q_INVOKABLE KWin::Tile *pick(const QPointF &point) const;
    Q_INVOKABLE KWin::Tile *pick(qreal x, qreal y) const;
};

KWIN_EXPORT QDebug operator<<(QDebug debug, const Tile *tile);

} // namespace KWin
