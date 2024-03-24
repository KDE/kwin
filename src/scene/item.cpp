/*
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "scene/item.h"
#include "core/renderlayer.h"
#include "scene/containernode.h"
#include "scene/scene.h"
#include "scene/transformnode.h"
#include "utils/common.h"

namespace KWin
{

Item::Item(Item *parent)
{
    setParentItem(parent);
}

Item::~Item()
{
    setParentItem(nullptr);
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
        m_parentItem->invalidateRenderNode();
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
    setScene(item ? item->scene() : nullptr);

    if (m_parentItem) {
        m_parentItem->addChild(this);
    }
    updateItemToSceneTransform();
    updateEffectiveVisibility();
}

void Item::addChild(Item *item)
{
    Q_ASSERT(!m_childItems.contains(item));

    m_childItems.append(item);
    markSortedChildItemsDirty();

    item->invalidateRenderNode();
    updateBoundingRect();
    scheduleRepaint(item->transform().mapRect(item->boundingRect()).translated(item->position()));

    Q_EMIT childAdded(item);
}

void Item::removeChild(Item *item)
{
    Q_ASSERT(m_childItems.contains(item));
    scheduleRepaint(item->transform().mapRect(item->boundingRect()).translated(item->position()));

    m_childItems.removeOne(item);
    markSortedChildItemsDirty();
    invalidateRenderNode();

    updateBoundingRect();
}

QList<Item *> Item::childItems() const
{
    return m_childItems;
}

void Item::setScene(Scene *scene)
{
    if (m_scene == scene) {
        return;
    }
    if (m_scene) {
        for (const auto &dirty : std::as_const(m_repaints)) {
            if (!dirty.isEmpty()) {
                m_scene->addRepaint(dirty);
            }
        }
        m_repaints.clear();
        disconnect(m_scene, &Scene::delegateRemoved, this, &Item::removeRepaints);
    }
    if (scene) {
        connect(scene, &Scene::delegateRemoved, this, &Item::removeRepaints);
    }

    m_scene = scene;

    for (Item *childItem : std::as_const(m_childItems)) {
        childItem->setScene(scene);
    }
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
        invalidateRenderNode();
        updateItemToSceneTransform();
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
        boundingRect |= item->transform().mapRect(item->boundingRect()).translated(item->position());
    }
    if (m_boundingRect != boundingRect) {
        m_boundingRect = boundingRect;
        Q_EMIT boundingRectChanged();
        if (m_parentItem) {
            m_parentItem->updateBoundingRect();
        }
    }
}

QList<QRectF> Item::shape() const
{
    return QList<QRectF>();
}

QRegion Item::opaque() const
{
    return QRegion();
}

QTransform Item::transform() const
{
    return m_transform;
}

void Item::setTransform(const QTransform &transform)
{
    if (m_transform == transform) {
        return;
    }
    scheduleRepaint(boundingRect());
    m_transform = transform;
    updateItemToSceneTransform();
    invalidateRenderNode();
    if (m_parentItem) {
        m_parentItem->updateBoundingRect();
    }
    scheduleRepaint(boundingRect());
}

void Item::updateItemToSceneTransform()
{
    m_itemToSceneTransform = m_transform;
    if (!m_position.isNull()) {
        m_itemToSceneTransform *= QTransform::fromTranslate(m_position.x(), m_position.y());
    }
    if (m_parentItem) {
        m_itemToSceneTransform *= m_parentItem->m_itemToSceneTransform;
    }
    m_sceneToItemTransform = m_itemToSceneTransform.inverted();

    for (Item *childItem : std::as_const(m_childItems)) {
        childItem->updateItemToSceneTransform();
    }
}

QRegion Item::mapToScene(const QRegion &region) const
{
    if (region.isEmpty()) {
        return QRegion();
    }
    return m_itemToSceneTransform.map(region);
}

QRectF Item::mapToScene(const QRectF &rect) const
{
    if (rect.isEmpty()) {
        return QRect();
    }
    return m_itemToSceneTransform.mapRect(rect);
}

QRectF Item::mapFromScene(const QRectF &rect) const
{
    if (rect.isEmpty()) {
        return QRect();
    }
    return m_sceneToItemTransform.mapRect(rect);
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
    m_parentItem->invalidateRenderNode();

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
    m_parentItem->invalidateRenderNode();

    scheduleRepaint(boundingRect());
    sibling->scheduleRepaint(sibling->boundingRect());
}

void Item::scheduleRepaint(const QRegion &region)
{
    if (isVisible()) {
        scheduleRepaintInternal(region);
    }
}

void Item::scheduleRepaint(SceneDelegate *delegate, const QRegion &region)
{
    if (isVisible()) {
        scheduleRepaintInternal(delegate, region);
    }
}

void Item::scheduleRepaintInternal(const QRegion &region)
{
    if (Q_UNLIKELY(!m_scene)) {
        return;
    }
    const QRegion globalRegion = mapToScene(region);
    const QList<SceneDelegate *> delegates = m_scene->delegates();
    for (SceneDelegate *delegate : delegates) {
        const QRegion dirtyRegion = globalRegion & delegate->viewport();
        if (!dirtyRegion.isEmpty()) {
            m_repaints[delegate] += dirtyRegion;
            delegate->layer()->scheduleRepaint(this);
        }
    }
}

void Item::scheduleRepaintInternal(SceneDelegate *delegate, const QRegion &region)
{
    if (Q_UNLIKELY(!m_scene)) {
        return;
    }
    const QRegion globalRegion = mapToScene(region);
    const QRegion dirtyRegion = globalRegion & delegate->viewport();
    if (!dirtyRegion.isEmpty()) {
        m_repaints[delegate] += dirtyRegion;
        delegate->layer()->scheduleRepaint(this);
    }
}

void Item::scheduleFrame()
{
    if (!isVisible()) {
        return;
    }
    if (Q_UNLIKELY(!m_scene)) {
        return;
    }
    const QRect geometry = mapToScene(rect()).toRect();
    const QList<SceneDelegate *> delegates = m_scene->delegates();
    for (SceneDelegate *delegate : delegates) {
        if (delegate->viewport().intersects(geometry)) {
            delegate->layer()->scheduleRepaint(this);
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
    invalidateRenderNode();

    if (!m_effectiveVisible) {
        if (m_scene) {
            m_scene->addRepaint(mapToScene(boundingRect()).toAlignedRect());
        }
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

const ColorDescription &Item::colorDescription() const
{
    return m_colorDescription;
}

void Item::setColorDescription(const ColorDescription &description)
{
    m_colorDescription = description;
}

PresentationModeHint Item::presentationHint() const
{
    return m_presentationHint;
}

void Item::setPresentationHint(PresentationModeHint hint)
{
    m_presentationHint = hint;
}

void Item::invalidateRenderNode()
{
    if (!m_renderNodeValid) {
        return;
    }

    m_renderNodeValid = false;

    if (m_parentItem) {
        m_parentItem->invalidateRenderNode();
    }
}

RenderNode *Item::updateContentNode()
{
    return nullptr;
}

RenderNode *Item::updateNode()
{
    if (!m_renderNodeValid) {
        m_renderNodeValid = true;
        m_renderNode = buildNode();
    }
    return m_renderNode;
}

RenderNode *Item::buildNode()
{
    if (!m_explicitVisible) {
        return nullptr;
    }

    QList<RenderNode *> childNodes;
    if (RenderNode *contentNode = updateContentNode()) {
        childNodes.append(contentNode);
    }

    const QList<Item *> childItems = sortedChildItems();
    for (Item *childItem : childItems) {
        if (RenderNode *childNode = childItem->updateNode()) {
            childNodes.append(childNode);
        }
    }

    RenderNode *containerNode = nullptr;
    if (!childNodes.isEmpty()) {
        if (childNodes.size() == 1) {
            containerNode = childNodes.first();
        } else {
            containerNode = new ContainerNode(childNodes);
        }
    } else {
        return nullptr;
    }

    if (!m_position.isNull() || !m_transform.isIdentity()) {
        return new TransformNode(containerNode, QTransform::fromTranslate(m_position.x(), m_position.y()) * m_transform);
    }

    return containerNode;
}

} // namespace KWin

#include "moc_item.cpp"
