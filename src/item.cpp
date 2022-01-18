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
#include "scene.h"
#include "screens.h"
#include "utils/common.h"

namespace KWin
{

Item::Item(Item *parent)
{
    setParentItem(parent);
    connect(kwinApp()->platform(), &Platform::outputDisabled, this, &Item::removeRepaints);
}

Item::~Item()
{
    setParentItem(nullptr);
    for (const auto &dirty : qAsConst(m_repaints)) {
        if (!dirty.isEmpty()) {
            Compositor::self()->scene()->addRepaint(dirty);
        }
    }
}

int Item::z() const
{
    return m_z;
}

void Item::setZ(int z)
{
    if (m_z == z) {
        return;
    }
    m_z = z;
    if (m_parentItem) {
        m_parentItem->markSortedChildItemsDirty();
    }
    scheduleRepaint(boundingRect());
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

    m_childItems.append(item);
    markSortedChildItemsDirty();

    updateBoundingRect();
    scheduleRepaint(item->boundingRect().translated(item->position()));
}

void Item::removeChild(Item *item)
{
    Q_ASSERT(m_childItems.contains(item));
    scheduleRepaint(item->boundingRect().translated(item->position()));

    m_childItems.removeOne(item);
    markSortedChildItemsDirty();

    updateBoundingRect();
}

QList<Item *> Item::childItems() const
{
    return m_childItems;
}

QPoint Item::position() const
{
    return m_position;
}

void Item::setPosition(const QPoint &point)
{
    if (m_position != point) {
        scheduleRepaint(boundingRect());
        m_position = point;
        if (m_parentItem) {
            m_parentItem->updateBoundingRect();
        }
        scheduleRepaint(boundingRect());
        Q_EMIT positionChanged();
    }
}

QSize Item::size() const
{
    return m_size;
}

void Item::setSize(const QSize &size)
{
    if (m_size != size) {
        scheduleRepaint(rect());
        m_size = size;
        updateBoundingRect();
        scheduleRepaint(rect());
        discardQuads();
        Q_EMIT sizeChanged();
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
        if (m_parentItem) {
            m_parentItem->updateBoundingRect();
        }
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

QMatrix4x4 Item::transform() const
{
    return m_transform;
}

void Item::setTransform(const QMatrix4x4 &transform)
{
    m_transform = transform;
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
    markSortedChildItemsDirty();

    scheduleRepaint(boundingRect());
    sibling->scheduleRepaint(sibling->boundingRect());
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
    markSortedChildItemsDirty();

    scheduleRepaint(boundingRect());
    sibling->scheduleRepaint(sibling->boundingRect());
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
        for (const auto &output : outputs) {
            const QRegion dirtyRegion = globalRegion & output->geometry();
            if (!dirtyRegion.isEmpty()) {
                m_repaints[output] += dirtyRegion;
                output->renderLoop()->scheduleRepaint(this);
            }
        }
    } else {
        m_repaints[nullptr] += globalRegion;
        kwinApp()->platform()->renderLoop()->scheduleRepaint(this);
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
                output->renderLoop()->scheduleRepaint(this);
            }
        }
    } else {
        kwinApp()->platform()->renderLoop()->scheduleRepaint(this);
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

QRegion Item::repaints(AbstractOutput *output) const
{
    return m_repaints.value(output, QRect(QPoint(0, 0), screens()->size()));
}

void Item::resetRepaints(AbstractOutput *output)
{
    m_repaints.insert(output, QRegion());
}

void Item::removeRepaints(AbstractOutput *output)
{
    m_repaints.remove(output);
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

static bool compareZ(const Item *a, const Item *b)
{
    return a->z() < b->z();
}

QList<Item *> Item::sortedChildItems() const
{
    if (!m_sortedChildItems.has_value()) {
        QList<Item *> items = m_childItems;
        std::stable_sort(items.begin(), items.end(), compareZ);
        m_sortedChildItems = items;
    }
    return m_sortedChildItems.value();
}

void Item::markSortedChildItemsDirty()
{
    m_sortedChildItems.reset();
}

} // namespace KWin
