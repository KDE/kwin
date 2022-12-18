/*
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "scene/item.h"
#include "core/renderlayer.h"
#include "core/renderloop.h"
#include "scene/scene.h"
#include "utils/common.h"

namespace KWin
{

Item::Item(Scene *scene, Item *parent)
    : m_scene(scene)
{
    setParentItem(parent);
    connect(m_scene, &Scene::delegateRemoved, this, &Item::removeRepaints);
}

Item::~Item()
{
    setParentItem(nullptr);
    for (const auto &dirty : std::as_const(m_repaints)) {
        if (!dirty.isEmpty()) {
            m_scene->addRepaint(dirty);
        }
    }
}

Scene *Item::scene() const
{
    return m_scene;
}

qreal Item::opacity() const
{
    return m_opacity;
}

void Item::setOpacity(qreal opacity)
{
    if (m_opacity != opacity) {
        m_opacity = opacity;
        scheduleRepaint(boundingRect());
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
        Q_ASSERT(m_parentItem->m_scene == m_scene);
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

    Q_EMIT childAdded(item);
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

QPointF Item::position() const
{
    return m_position;
}

void Item::setPosition(const QPointF &point)
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

QSizeF Item::size() const
{
    return m_size;
}

void Item::setSize(const QSizeF &size)
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

QRectF Item::rect() const
{
    return QRectF(QPoint(0, 0), size());
}

QRectF Item::boundingRect() const
{
    return m_boundingRect;
}

void Item::updateBoundingRect()
{
    QRectF boundingRect = rect();
    for (Item *item : std::as_const(m_childItems)) {
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

QVector<QRectF> Item::shape() const
{
    return QVector<QRectF>();
}

QRegion Item::opaque() const
{
    return QRegion();
}

QPointF Item::rootPosition() const
{
    QPointF ret = position();

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
    if (region.isEmpty()) {
        return QRegion();
    }
    return region.translated(rootPosition().toPoint());
}

QRectF Item::mapToGlobal(const QRectF &rect) const
{
    if (rect.isEmpty()) {
        return QRect();
    }
    return rect.translated(rootPosition());
}

QRectF Item::mapFromGlobal(const QRectF &rect) const
{
    if (rect.isEmpty()) {
        return QRect();
    }
    return rect.translated(-rootPosition());
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
    const QList<SceneDelegate *> delegates = m_scene->delegates();
    for (SceneDelegate *delegate : delegates) {
        const QRegion dirtyRegion = globalRegion & delegate->viewport();
        if (!dirtyRegion.isEmpty()) {
            m_repaints[delegate] += dirtyRegion;
            delegate->layer()->loop()->scheduleRepaint(this);
        }
    }
}

void Item::scheduleFrame()
{
    if (!isVisible()) {
        return;
    }
    const QRect geometry = mapToGlobal(rect()).toRect();
    const QList<SceneDelegate *> delegates = m_scene->delegates();
    for (SceneDelegate *delegate : delegates) {
        if (delegate->viewport().intersects(geometry)) {
            delegate->layer()->loop()->scheduleRepaint(this);
        }
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

QRegion Item::repaints(SceneDelegate *delegate) const
{
    return m_repaints.value(delegate);
}

void Item::resetRepaints(SceneDelegate *delegate)
{
    m_repaints.insert(delegate, QRegion());
}

void Item::removeRepaints(SceneDelegate *delegate)
{
    m_repaints.remove(delegate);
}

bool Item::explicitVisible() const
{
    return m_explicitVisible;
}

bool Item::isVisible() const
{
    return m_effectiveVisible;
}

void Item::setVisible(bool visible)
{
    if (m_explicitVisible != visible) {
        m_explicitVisible = visible;
        updateEffectiveVisibility();
    }
}

void Item::scheduleRepaint(const QRectF &region)
{
    scheduleRepaint(QRegion(region.toAlignedRect()));
}

bool Item::computeEffectiveVisibility() const
{
    return m_explicitVisible && (!m_parentItem || m_parentItem->isVisible());
}

void Item::updateEffectiveVisibility()
{
    const bool effectiveVisible = computeEffectiveVisibility();
    if (m_effectiveVisible == effectiveVisible) {
        return;
    }

    m_effectiveVisible = effectiveVisible;
    if (!m_effectiveVisible) {
        m_scene->addRepaint(mapToGlobal(boundingRect()).toAlignedRect());
    } else {
        scheduleRepaintInternal(boundingRect().toAlignedRect());
    }

    for (Item *childItem : std::as_const(m_childItems)) {
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
