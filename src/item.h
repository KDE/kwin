/*
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "scene.h"

namespace KWin
{

/**
 * The Item class is the base class for items in the scene.
 */
class KWIN_EXPORT Item : public QObject
{
    Q_OBJECT

public:
    explicit Item(Scene::Window *window, Item *parent = nullptr);
    ~Item() override;

    /**
     * Returns the x coordinate relative to the top left corner of the parent item.
     */
    int x() const;
    void setX(int x);

    /**
     * Returns the y coordinate relative to the top left corner of the parent item.
     */
    int y() const;
    void setY(int y);

    int width() const;
    void setWidth(int width);

    int height() const;
    void setHeight(int height);

    QPoint position() const;
    void setPosition(const QPoint &point);

    QSize size() const;
    void setSize(const QSize &size);

    /**
     * Returns the enclosing rectangle of the item. The rect equals QRect(0, 0, width(), height()).
     */
    QRect rect() const;
    /**
     * Returns the enclosing rectangle of the item and all of its descendants.
     */
    QRect boundingRect() const;

    /**
     * Returns the visual parent of the item. Note that the visual parent differs from
     * the QObject parent.
     */
    Item *parentItem() const;
    void setParentItem(Item *parent);
    QList<Item *> childItems() const;

    Scene::Window *window() const;
    QPoint rootPosition() const;

    /**
     * Maps the given @a region from the item's coordinate system to the scene's coordinate
     * system.
     */
    QRegion mapToGlobal(const QRegion &region) const;
    /**
     * Maps the given @a rect from the item's coordinate system to the scene's coordinate
     * system.
     */
    QRect mapToGlobal(const QRect &rect) const;

    /**
     * Moves this item right before the specified @a sibling in the parent's children list.
     */
    void stackBefore(Item *sibling);
    /**
     * Moves this item right after the specified @a sibling in the parent's children list.
     */
    void stackAfter(Item *sibling);
    /**
     * Restacks the child items in the specified order. Note that the specified stacking order
     * must be a permutation of childItems().
     */
    void stackChildren(const QList<Item *> &children);

    void scheduleRepaint(const QRegion &region);
    void scheduleRepaint();

Q_SIGNALS:
    /**
     * This signal is emitted when the x coordinate of this item has changed.
     */
    void xChanged();
    /**
     * This signal is emitted when the y coordinate of this item has changed.
     */
    void yChanged();
    /**
     * This signal is emitted when the width of this item has changed.
     */
    void widthChanged();
    /**
     * This signal is emitted when the height of this item has changed.
     */
    void heightChanged();

    /**
     * This signal is emitted when the rectangle that encloses this item and all of its children
     * has changed.
     */
    void boundingRectChanged();

protected:
    virtual void preprocess();
    void discardQuads();

private:
    void addChild(Item *item);
    void removeChild(Item *item);
    void updateBoundingRect();

    Scene::Window *m_window;
    QPointer<Item> m_parentItem;
    QList<Item *> m_childItems;
    QRect m_boundingRect;
    int m_x = 0;
    int m_y = 0;
    int m_width = 0;
    int m_height = 0;

    friend class Scene::Window;
};

} // namespace KWin
