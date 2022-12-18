/*
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "kwineffects.h"
#include "kwinglobals.h"

#include <QMatrix4x4>
#include <QObject>
#include <QVector>

#include <optional>

namespace KWin
{

class SceneDelegate;
class Scene;

/**
 * The Item class is the base class for items in the scene.
 */
class KWIN_EXPORT Item : public QObject
{
    Q_OBJECT

public:
    explicit Item(Scene *scene, Item *parent = nullptr);
    ~Item() override;

    Scene *scene() const;

    qreal opacity() const;
    void setOpacity(qreal opacity);

    QPointF position() const;
    void setPosition(const QPointF &point);

    QSizeF size() const;
    void setSize(const QSizeF &size);

    int z() const;
    void setZ(int z);

    /**
     * Returns the enclosing rectangle of the item. The rect equals QRect(0, 0, width(), height()).
     */
    QRectF rect() const;
    /**
     * Returns the enclosing rectangle of the item and all of its descendants.
     */
    QRectF boundingRect() const;

    virtual QVector<QRectF> shape() const;
    virtual QRegion opaque() const;

    /**
     * Returns the visual parent of the item. Note that the visual parent differs from
     * the QObject parent.
     */
    Item *parentItem() const;
    void setParentItem(Item *parent);
    QList<Item *> childItems() const;
    QList<Item *> sortedChildItems() const;

    QPointF rootPosition() const;

    QMatrix4x4 transform() const;
    void setTransform(const QMatrix4x4 &transform);

    /**
     * Maps the given @a region from the item's coordinate system to the scene's coordinate
     * system.
     */
    QRegion mapToGlobal(const QRegion &region) const;
    /**
     * Maps the given @a rect from the item's coordinate system to the scene's coordinate
     * system.
     */
    QRectF mapToGlobal(const QRectF &rect) const;
    /**
     * Maps the given @a rect from the scene's coordinate system to the item's coordinate
     * system.
     */
    QRectF mapFromGlobal(const QRectF &rect) const;

    /**
     * Moves this item right before the specified @a sibling in the parent's children list.
     */
    void stackBefore(Item *sibling);
    /**
     * Moves this item right after the specified @a sibling in the parent's children list.
     */
    void stackAfter(Item *sibling);

    bool explicitVisible() const;
    bool isVisible() const;
    void setVisible(bool visible);

    void scheduleRepaint(const QRectF &region);
    void scheduleRepaint(const QRegion &region);
    void scheduleFrame();
    QRegion repaints(SceneDelegate *delegate) const;
    void resetRepaints(SceneDelegate *delegate);

    WindowQuadList quads() const;
    virtual void preprocess();

Q_SIGNALS:
    void childAdded(Item *item);
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
    void markSortedChildItemsDirty();

    bool computeEffectiveVisibility() const;
    void updateEffectiveVisibility();
    void removeRepaints(SceneDelegate *delegate);

    Scene *m_scene;
    QPointer<Item> m_parentItem;
    QList<Item *> m_childItems;
    QMatrix4x4 m_transform;
    QRectF m_boundingRect;
    QPointF m_position;
    QSizeF m_size = QSize(0, 0);
    qreal m_opacity = 1;
    int m_z = 0;
    bool m_explicitVisible = true;
    bool m_effectiveVisible = true;
    QMap<SceneDelegate *, QRegion> m_repaints;
    mutable std::optional<WindowQuadList> m_quads;
    mutable std::optional<QList<Item *>> m_sortedChildItems;
};

} // namespace KWin
