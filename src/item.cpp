/*
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "item.h"
#include "abstract_output.h"
#include "composite.h"
#include "main.h"
#include "platform.h"
#include "renderloop.h"
#include "screens.h"

namespace KWin
{

Item::Item(Scene::Window *window, Item *parent)
    : m_window(window)
{
    setParentItem(parent);

    if (kwinApp()->platform()->isPerScreenRenderingEnabled()) {
        connect(kwinApp()->platform(), &Platform::outputEnabled, this, &Item::reallocRepaints);
        connect(kwinApp()->platform(), &Platform::outputDisabled, this, &Item::reallocRepaints);
    }
    reallocRepaints();
}

Item::~Item()
{
    setParentItem(nullptr);

    for (int i = 0; i < m_repaints.count(); ++i) {
        const QRegion dirty = repaints(i);
        if (!dirty.isEmpty()) {
            Compositor::self()->addRepaint(dirty);
        }
    }
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
    Q_EMIT xChanged();
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
    Q_EMIT yChanged();
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
    Q_EMIT widthChanged();
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
    Q_EMIT heightChanged();
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
    updateEffectiveVisibility();
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

        if (xDirty) {
            Q_EMIT xChanged();
        }
        if (yDirty) {
            Q_EMIT yChanged();
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
            Q_EMIT widthChanged();
        }
        if (heightDirty) {
            Q_EMIT heightChanged();
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
        Q_EMIT boundingRectChanged();
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
    if (isVisible()) {
        scheduleRepaintInternal(region);
    }
}

void Item::scheduleRepaintInternal(const QRegion &region)
{
    const QRegion globalRegion = mapToGlobal(region);
    if (kwinApp()->platform()->isPerScreenRenderingEnabled()) {
        const QVector<AbstractOutput *> outputs = kwinApp()->platform()->enabledOutputs();
        if (m_repaints.count() != outputs.count()) {
            return; // Repaints haven't been reallocated yet, do nothing.
        }
        for (int screenId = 0; screenId < m_repaints.count(); ++screenId) {
            AbstractOutput *output = outputs[screenId];
            const QRegion dirtyRegion = globalRegion & output->geometry();
            if (!dirtyRegion.isEmpty()) {
                m_repaints[screenId] += dirtyRegion;
                output->renderLoop()->scheduleRepaint();
            }
        }
    } else {
        m_repaints[0] += globalRegion;
        kwinApp()->platform()->renderLoop()->scheduleRepaint();
    }
}

void Item::scheduleFrame()
{
    if (!isVisible()) {
        return;
    }
    if (kwinApp()->platform()->isPerScreenRenderingEnabled()) {
        const QRect geometry = mapToGlobal(rect());
        const QVector<AbstractOutput *> outputs = kwinApp()->platform()->enabledOutputs();
        for (const AbstractOutput *output : outputs) {
            if (output->geometry().intersects(geometry)) {
                output->renderLoop()->scheduleRepaint();
            }
        }
    } else {
        kwinApp()->platform()->renderLoop()->scheduleRepaint();
    }
}

void Item::preprocess()
{
}

WindowQuadList Item::buildQuads() const
{
    return WindowQuadList();
}

void Item::discardQuads()
{
    m_quads.reset();
}

WindowQuadList Item::quads() const
{
    if (!m_quads.has_value()) {
        m_quads = buildQuads();
    }
    return m_quads.value();
}

QRegion Item::repaints(int screen) const
{
    Q_ASSERT(!m_repaints.isEmpty());
    const int index = screen != -1 ? screen : 0;
    if (m_repaints[index] == infiniteRegion()) {
        return QRect(QPoint(0, 0), screens()->size());
    }
    return m_repaints[index];
}

void Item::resetRepaints(int screen)
{
    Q_ASSERT(!m_repaints.isEmpty());
    const int index = screen != -1 ? screen : 0;
    m_repaints[index] = QRegion();
}

void Item::reallocRepaints()
{
    if (kwinApp()->platform()->isPerScreenRenderingEnabled()) {
        m_repaints.resize(kwinApp()->platform()->enabledOutputs().count());
    } else {
        m_repaints.resize(1);
    }

    m_repaints.fill(infiniteRegion());
}

bool Item::isVisible() const
{
    return m_effectiveVisible;
}

void Item::setVisible(bool visible)
{
    if (m_visible != visible) {
        m_visible = visible;
        updateEffectiveVisibility();
    }
}

bool Item::computeEffectiveVisibility() const
{
    return m_visible && (!m_parentItem || m_parentItem->isVisible());
}

void Item::updateEffectiveVisibility()
{
    const bool effectiveVisible = computeEffectiveVisibility();
    if (m_effectiveVisible == effectiveVisible) {
        return;
    }

    m_effectiveVisible = effectiveVisible;
    scheduleRepaintInternal(boundingRect());

    for (Item *childItem : qAsConst(m_childItems)) {
        childItem->updateEffectiveVisibility();
    }
}

} // namespace KWin
