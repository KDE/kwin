/*
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "item.h"

namespace KWin
{

Item::Item(Scene::Window *window, Item *parent)
    : m_window(window)
{
    setParentItem(parent);
}

Item::~Item()
{
    setParentItem(nullptr);
}

int Item::x() const
{
    return m_x;
}

void Item::setX(int x)
{
    if (m_x == x) {
        return;
    }
    scheduleRepaint(boundingRect());
    m_x = x;
    scheduleRepaint(boundingRect());
    discardQuads();
    emit xChanged();
}

int Item::y() const
{
    return m_y;
}

void Item::setY(int y)
{
    if (m_y == y) {
        return;
    }
    scheduleRepaint(boundingRect());
    m_y = y;
    scheduleRepaint(boundingRect());
    discardQuads();
    emit yChanged();
}

int Item::width() const
{
    return m_width;
}

void Item::setWidth(int width)
{
    if (m_width == width) {
        return;
    }
    scheduleRepaint(rect());
    m_width = width;
    updateBoundingRect();
    scheduleRepaint(rect());
    discardQuads();
    emit widthChanged();
}

int Item::height() const
{
    return m_height;
}

void Item::setHeight(int height)
{
    if (m_height == height) {
        return;
    }
    scheduleRepaint(rect());
    m_height = height;
    updateBoundingRect();
    scheduleRepaint(rect());
    discardQuads();
    emit heightChanged();
}

Item *Item::parentItem() const
{
    return m_parentItem;
}

void Item::setParentItem(Item *item)
{
    if (m_parentItem == item) {
        return;
    }
    if (m_parentItem) {
        m_parentItem->removeChild(this);
    }
    m_parentItem = item;
    if (m_parentItem) {
        m_parentItem->addChild(this);
    }
}

void Item::addChild(Item *item)
{
    Q_ASSERT(!m_childItems.contains(item));

    connect(item, &Item::xChanged, this, &Item::updateBoundingRect);
    connect(item, &Item::yChanged, this, &Item::updateBoundingRect);
    connect(item, &Item::boundingRectChanged, this, &Item::updateBoundingRect);

    m_childItems.append(item);
    updateBoundingRect();
    scheduleRepaint(item->boundingRect().translated(item->position()));
    discardQuads();
}

void Item::removeChild(Item *item)
{
    Q_ASSERT(m_childItems.contains(item));
    scheduleRepaint(item->boundingRect().translated(item->position()));

    m_childItems.removeOne(item);

    disconnect(item, &Item::xChanged, this, &Item::updateBoundingRect);
    disconnect(item, &Item::yChanged, this, &Item::updateBoundingRect);
    disconnect(item, &Item::boundingRectChanged, this, &Item::updateBoundingRect);

    updateBoundingRect();
    discardQuads();
}

QList<Item *> Item::childItems() const
{
    return m_childItems;
}

Scene::Window *Item::window() const
{
    return m_window;
}

QPoint Item::position() const
{
    return QPoint(x(), y());
}

void Item::setPosition(const QPoint &point)
{
    const bool xDirty = (x() != point.x());
    const bool yDirty = (y() != point.y());

    if (xDirty || yDirty) {
        scheduleRepaint(boundingRect());
        m_x = point.x();
        m_y = point.y();
        scheduleRepaint(boundingRect());

        discardQuads();

        if (xDirty) {
            emit xChanged();
        }
        if (yDirty) {
            emit yChanged();
        }
    }
}

QSize Item::size() const
{
    return QSize(width(), height());
}

void Item::setSize(const QSize &size)
{
    const bool widthDirty = (width() != size.width());
    const bool heightDirty = (height() != size.height());

    if (widthDirty || heightDirty) {
        scheduleRepaint(rect());
        m_width = size.width();
        m_height = size.height();
        updateBoundingRect();
        scheduleRepaint(rect());

        discardQuads();

        if (widthDirty) {
            emit widthChanged();
        }
        if (heightDirty) {
            emit heightChanged();
        }
    }
}

QRect Item::rect() const
{
    return QRect(QPoint(0, 0), size());
}

QRect Item::boundingRect() const
{
    return m_boundingRect;
}

void Item::updateBoundingRect()
{
    QRect boundingRect = rect();
    for (Item *item : qAsConst(m_childItems)) {
        boundingRect |= item->boundingRect().translated(item->position());
    }
    if (m_boundingRect != boundingRect) {
        m_boundingRect = boundingRect;
        emit boundingRectChanged();
    }
}

QPoint Item::rootPosition() const
{
    QPoint ret = position();

    Item *parent = parentItem();
    while (parent) {
        ret += parent->position();
        parent = parent->parentItem();
    }

    return ret;
}

QRegion Item::mapToGlobal(const QRegion &region) const
{
    return region.translated(rootPosition());
}

QRect Item::mapToGlobal(const QRect &rect) const
{
    return rect.translated(rootPosition());
}

void Item::stackBefore(Item *sibling)
{
    if (Q_UNLIKELY(!sibling)) {
        qCDebug(KWIN_CORE) << Q_FUNC_INFO << "requires a valid sibling";
        return;
    }
    if (Q_UNLIKELY(!sibling->parentItem() || sibling->parentItem() != parentItem())) {
        qCDebug(KWIN_CORE) << Q_FUNC_INFO << "requires items to be siblings";
        return;
    }
    if (Q_UNLIKELY(sibling == this)) {
        return;
    }

    const int selfIndex = m_parentItem->m_childItems.indexOf(this);
    const int siblingIndex = m_parentItem->m_childItems.indexOf(sibling);

    if (selfIndex == siblingIndex - 1) {
        return;
    }

    m_parentItem->m_childItems.move(selfIndex, selfIndex > siblingIndex ? siblingIndex : siblingIndex - 1);

    scheduleRepaint(boundingRect());
    sibling->scheduleRepaint(sibling->boundingRect());

    discardQuads();
}

void Item::stackAfter(Item *sibling)
{
    if (Q_UNLIKELY(!sibling)) {
        qCDebug(KWIN_CORE) << Q_FUNC_INFO << "requires a valid sibling";
        return;
    }
    if (Q_UNLIKELY(!sibling->parentItem() || sibling->parentItem() != parentItem())) {
        qCDebug(KWIN_CORE) << Q_FUNC_INFO << "requires items to be siblings";
        return;
    }
    if (Q_UNLIKELY(sibling == this)) {
        return;
    }

    const int selfIndex = m_parentItem->m_childItems.indexOf(this);
    const int siblingIndex = m_parentItem->m_childItems.indexOf(sibling);

    if (selfIndex == siblingIndex + 1) {
        return;
    }

    m_parentItem->m_childItems.move(selfIndex, selfIndex > siblingIndex ? siblingIndex + 1 : siblingIndex);

    scheduleRepaint(boundingRect());
    sibling->scheduleRepaint(sibling->boundingRect());

    discardQuads();
}

void Item::stackChildren(const QList<Item *> &children)
{
    if (m_childItems.count() != children.count()) {
        qCWarning(KWIN_CORE) << Q_FUNC_INFO << "invalid child list";
        return;
    }

#if !defined(QT_NO_DEBUG)
    for (const Item *item : children) {
        Q_ASSERT_X(item->parentItem() == this, Q_FUNC_INFO, "invalid parent");
    }
#endif

    m_childItems = children;
    discardQuads();
}

void Item::scheduleRepaint(const QRegion &region)
{
    window()->addLayerRepaint(mapToGlobal(region));
}

void Item::scheduleRepaint()
{
    window()->scheduleRepaint();
}

void Item::preprocess()
{
}

void Item::discardQuads()
{
    window()->discardQuads();
}

} // namespace KWin
