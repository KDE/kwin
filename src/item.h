/*
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "scene.h"

#include <optional>

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

    bool isVisible() const;
    void setVisible(bool visible);

    void scheduleRepaint(const QRegion &region);
    void scheduleFrame();
    QRegion repaints(int screen) const;
    void resetRepaints(int screen);

    WindowQuadList quads() const;
    virtual void preprocess();

Q_SIGNALS:
    /**
     * This signal is emitted when the position of this item has changed.
     */
    void positionChanged();
    /**
     * This signal is emitted when the size of this item has changed.
     */
    void sizeChanged();

    /**
     * This signal is emitted when the rectangle that encloses this item and all of its children
     * has changed.
     */
    void boundingRectChanged();

protected:
    virtual WindowQuadList buildQuads() const;
    void discardQuads();

private:
    void addChild(Item *item);
    void removeChild(Item *item);
    void updateBoundingRect();
    void scheduleRepaintInternal(const QRegion &region);
    void reallocRepaints();

    bool computeEffectiveVisibility() const;
    void updateEffectiveVisibility();

    Scene::Window *m_window;
    QPointer<Item> m_parentItem;
    QList<Item *> m_childItems;
    QRect m_boundingRect;
    QPoint m_position;
    QSize m_size = QSize(0, 0);
    bool m_visible = true;
    bool m_effectiveVisible = true;
    QVector<QRegion> m_repaints;
    mutable std::optional<WindowQuadList> m_quads;
};

} // namespace KWin
