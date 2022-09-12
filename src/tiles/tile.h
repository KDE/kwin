/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2022 Marco Martin <mart@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef TILE_H
#define TILE_H

#include <kwin_export.h>
#include <kwinglobals.h>

#include <QObject>
#include <QRectF>

namespace KWin
{

class TileManager;
class Window;

// This data entry looks and behaves like a tree model node, even though will live on a flat QAbstractItemModel to be represented by a single QML Repeater
class KWIN_EXPORT Tile : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QRectF relativeGeometry READ relativeGeometry NOTIFY relativeGeometryChanged)
    Q_PROPERTY(QRectF absoluteGeometry READ absoluteGeometry NOTIFY absoluteGeometryChanged)
    Q_PROPERTY(qreal padding READ padding WRITE setPadding NOTIFY paddingChanged)
    Q_PROPERTY(int positionInLayout READ row NOTIFY rowChanged)
    Q_PROPERTY(Tile *parent READ parentTile CONSTANT)
    Q_PROPERTY(QList<Tile *> tiles READ childTiles NOTIFY childTilesChanged)
    Q_PROPERTY(bool isLayout READ isLayout NOTIFY isLayoutChanged)
    Q_PROPERTY(bool canBeRemoved READ canBeRemoved CONSTANT)

public:
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
     * Absolute geometry minus the padding and reserved areas such as panels
     */
    QRectF windowGeometry() const;

    /**
     * Absolute geometry minus the padding and reserved areas such as panels
     */
    QRectF maximizedWindowGeometry() const;

    bool isLayout() const;
    bool canBeRemoved() const;

    qreal padding() const;
    void setPadding(qreal padding);

    /**
     * All tiles directly children of this tile
     */
    QList<Tile *> childTiles() const;

    /**
     * All tiles descendant of this tile, recursive
     */
    QList<Tile *> descendants() const;

    Q_INVOKABLE void resizeByPixels(qreal delta, Qt::Edge edge);

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
    T *createChild(const QRectF &relativeGeometry, int position)
    {
        T *t = new T(m_tiling, static_cast<T *>(this));
        t->setRelativeGeometry(relativeGeometry);
        insertChild(position, t);
        return t;
    }

Q_SIGNALS:
    void relativeGeometryChanged(const QRectF &relativeGeometry);
    void absoluteGeometryChanged();
    void windowGeometryChanged();
    void paddingChanged(qreal padding);
    void rowChanged(int row);
    void isLayoutChanged(bool isLayout);
    void childTilesChanged();

protected:
    explicit Tile(TileManager *tiling, Tile *parentItem = nullptr);
    void insertChild(int position, Tile *item);
    void removeChild(Tile *child);

private:
    QVector<Tile *> m_children;
    Tile *m_parentTile;

    TileManager *m_tiling;
    QRectF m_relativeGeometry;
    static QSizeF s_minimumSize;
    qreal m_padding = 4.0;
    bool m_canBeRemoved = true;
};

} // namespace KWin

#endif // TILE_H
