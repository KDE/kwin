/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2022 Marco Martin <mart@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "effect/globals.h"
#include <kwin_export.h>
#include <utils/common.h>

#include <QObject>
#include <QRectF>

namespace KWin
{

class TileManager;
class Window;

class KWIN_EXPORT Tile : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QRectF relativeGeometry READ relativeGeometry WRITE setRelativeGeometry NOTIFY relativeGeometryChanged)
    Q_PROPERTY(QRectF absoluteGeometry READ absoluteGeometry NOTIFY absoluteGeometryChanged)
    Q_PROPERTY(QRectF absoluteGeometryInScreen READ absoluteGeometryInScreen NOTIFY absoluteGeometryChanged)
    Q_PROPERTY(qreal padding READ padding WRITE setPadding NOTIFY paddingChanged)
    Q_PROPERTY(int positionInLayout READ row NOTIFY rowChanged)
    Q_PROPERTY(Tile *parent READ parentTile CONSTANT)
    Q_PROPERTY(QList<KWin::Tile *> tiles READ childTiles NOTIFY childTilesChanged)
    Q_PROPERTY(QList<KWin::Window *> windows READ windows NOTIFY windowsChanged)
    Q_PROPERTY(bool isLayout READ isLayout NOTIFY isLayoutChanged)
    Q_PROPERTY(bool canBeRemoved READ canBeRemoved CONSTANT)

public:
    enum class LayoutDirection {
        Floating = 0,
        Horizontal = 1,
        Vertical = 2
    };
    Q_ENUM(LayoutDirection)

    explicit Tile(TileManager *tiling, Tile *parentItem = nullptr);
    ~Tile();

    void setGeometryFromWindow(const QRectF &geom);
    void setGeometryFromAbsolute(const QRectF &geom);
    virtual void setRelativeGeometry(const QRectF &geom);

    virtual bool supportsResizeGravity(Gravity gravity);

    /**
     * Geometry of the tile in units between 0 and 1 relative to the screen geometry
     */
    QRectF relativeGeometry() const;

    /**
     * Geometry of the tile in absolute coordinates
     */
    QRectF absoluteGeometry() const;

    /**
     * Geometry of the tile in absolute coordinates, but in screen coordinates,
     * ie the top left corner of rootTile always at 0,0
     */
    QRectF absoluteGeometryInScreen() const;

    /**
     * Absolute geometry minus the padding and reserved areas such as panels
     */
    QRectF windowGeometry() const;

    /**
     * Absolute geometry minus the padding and reserved areas such as panels
     */
    QRectF maximizedWindowGeometry() const;

    /**
     * Which edges of the tile touches an edge of the screen
     */
    Qt::Edges anchors() const;

    bool isLayout() const;
    bool canBeRemoved() const;

    qreal padding() const;
    void setPadding(qreal padding);

    QuickTileMode quickTileMode() const;
    void setQuickTileMode(QuickTileMode mode);

    /**
     * All tiles directly children of this tile
     */
    QList<Tile *> childTiles() const;

    /**
     * All tiles descendant of this tile, recursive
     */
    QList<Tile *> descendants() const;

    /**
     * Visit all tiles descendant of this tile, recursive
     */
    void visitDescendants(std::function<void(const Tile *child)> callback) const;

    void resizeFromGravity(Gravity gravity, int x_root, int y_root);

    Q_INVOKABLE void resizeByPixels(qreal delta, Qt::Edge edge);

    void addWindow(Window *window);
    void removeWindow(Window *window);
    QList<KWin::Window *> windows() const;

    int row() const;
    int childCount() const;
    Tile *childTile(int row);
    Tile *nextSibling() const;
    Tile *previousSibling() const;
    Tile *parentTile() const;
    TileManager *manager() const;

    static inline QSizeF minimumSize()
    {
        return s_minimumSize;
    }

    void destroyChild(Tile *tile);

    template<class T>
    T *createChildAt(const QRectF &relativeGeometry, int position)
    {
        T *t = new T(m_tiling, static_cast<T *>(this));
        t->setRelativeGeometry(relativeGeometry);
        insertChild(position, t);
        return t;
    }

Q_SIGNALS:
    void relativeGeometryChanged();
    void absoluteGeometryChanged();
    void windowGeometryChanged();
    void paddingChanged(qreal padding);
    void rowChanged(int row);
    void isLayoutChanged(bool isLayout);
    void childTilesChanged();
    void windowAdded(Window *window);
    void windowRemoved(Window *window);
    void windowsChanged();

protected:
    void insertChild(int position, Tile *item);
    void removeChild(Tile *child);

    QList<Tile *> m_children;
    QList<Window *> m_windows;
    Tile *m_parentTile;

    TileManager *m_tiling;
    QRectF m_relativeGeometry;
    static QSizeF s_minimumSize;
    QuickTileMode m_quickTileMode = QuickTileFlag::None;
    qreal m_padding = 4.0;
};

} // namespace KWin
