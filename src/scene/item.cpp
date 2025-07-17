/*
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "scene/item.h"
#include "core/outputlayer.h"
#include "core/pixelgrid.h"
#include "scene/scene.h"
#include "utils/common.h"
#include "workspace.h"

namespace KWin
{

ItemEffect::ItemEffect(Item *item)
    : m_item(item)
{
    item->addEffect();
}

ItemEffect::ItemEffect(ItemEffect &&move)
    : m_item(std::exchange(move.m_item, nullptr))
{
}

ItemEffect::ItemEffect()
{
}

ItemEffect::~ItemEffect()
{
    if (m_item) {
        m_item->removeEffect();
    }
}

ItemEffect &ItemEffect::operator=(ItemEffect &&move)
{
    std::swap(m_item, move.m_item);
    return *this;
}

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
    }
    scheduleSceneRepaint(boundingRect());
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
        for (auto it = m_repaints.constBegin(); it != m_repaints.constEnd(); ++it) {
            RenderView *view = it.key();
            const QRegion &dirty = it.value();
            if (!dirty.isEmpty()) {
                m_scene->addRepaint(view, dirty);
            }
        }
        m_repaints.clear();
        disconnect(m_scene, &Scene::viewRemoved, this, &Item::removeRepaints);
    }
    if (scene) {
        connect(scene, &Scene::viewRemoved, this, &Item::removeRepaints);
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
        scheduleMoveRepaint(boundingRect());
        m_position = point;
        updateItemToSceneTransform();
        if (m_parentItem) {
            m_parentItem->updateBoundingRect();
        }
        scheduleMoveRepaint(boundingRect());
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

void Item::setGeometry(const QRectF &rect)
{
    setPosition(rect.topLeft());
    setSize(rect.size());
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

QRect Item::paintedArea(RenderView *view, const QRectF &rect) const
{
    const qreal scale = view->scale();

    QRectF snapped = snapToPixelGridF(scaledRect(rect, scale));
    for (const Item *item = this; item; item = item->parentItem()) {
        snapped = item->transform()
                      .mapRect(snapped)
                      .translated(snapToPixelGridF(item->position() * scale));
    }

    return scaledRect(snapped, 1.0 / scale).toAlignedRect();
}

QRegion Item::paintedArea(RenderView *view, const QRegion &region) const
{
    if (region.isEmpty()) {
        return QRegion();
    }

    const qreal scale = view->scale();

    QList<QRectF> parts;
    parts.reserve(region.rectCount());
    for (const QRect &rect : region) {
        parts.append(snapToPixelGridF(scaledRect(rect, scale)));
    }

    for (const Item *item = this; item; item = item->parentItem()) {
        for (QRectF &part : parts) {
            part = item->transform()
                       .mapRect(part)
                       .translated(snapToPixelGridF(item->position() * scale));
        }
    }

    QRegion ret;
    for (const QRectF &part : parts) {
        ret |= scaledRect(part, 1.0 / scale).toAlignedRect();
    }
    return ret;
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
    m_parentItem->markSortedChildItemsDirty();

    scheduleSceneRepaint(boundingRect());
    sibling->scheduleSceneRepaint(sibling->boundingRect());
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
    m_parentItem->markSortedChildItemsDirty();

    scheduleSceneRepaint(boundingRect());
    sibling->scheduleSceneRepaint(sibling->boundingRect());
}

void Item::scheduleRepaint(const QRegion &region)
{
    if (isVisible()) {
        scheduleRepaintInternal(region);
    }
}

void Item::scheduleRepaint(RenderView *view, const QRegion &region)
{
    if (isVisible()) {
        scheduleRepaintInternal(view, region);
    }
}

void Item::scheduleRepaintInternal(const QRegion &region)
{
    if (Q_UNLIKELY(!m_scene)) {
        return;
    }
    const QList<RenderView *> views = m_scene->views();
    for (RenderView *view : views) {
        if (!view->shouldRenderItem(this)) {
            continue;
        }
        const QRegion dirtyRegion = paintedArea(view, region) & view->viewport().toAlignedRect();
        if (!dirtyRegion.isEmpty()) {
            m_repaints[view] += dirtyRegion;
            view->scheduleRepaint(this);
        }
    }
}

void Item::scheduleMoveRepaint(const QRectF &region)
{
    if (Q_UNLIKELY(!m_scene) || !isVisible()) {
        return;
    }
    const QList<RenderView *> views = m_scene->views();
    for (RenderView *view : views) {
        if (!view->shouldRenderItem(this)) {
            continue;
        }
        const QRegion dirtyRegion = paintedArea(view, region) & view->viewport().toAlignedRect();
        if (!dirtyRegion.isEmpty()) {
            if (!view->canSkipMoveRepaint(this)) {
                m_repaints[view] += dirtyRegion;
            }
            view->scheduleRepaint(this);
        }
    }
}

void Item::scheduleRepaintInternal(RenderView *view, const QRegion &region)
{
    if (Q_UNLIKELY(!m_scene) || !view->shouldRenderItem(this)) {
        return;
    }
    const QRegion dirtyRegion = paintedArea(view, region) & view->viewport().toAlignedRect();
    if (!dirtyRegion.isEmpty()) {
        m_repaints[view] += dirtyRegion;
        view->scheduleRepaint(this);
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
    const QList<RenderView *> views = m_scene->views();
    for (RenderView *view : views) {
        if (!view->shouldRenderItem(this)) {
            continue;
        }
        const QRect geometry = paintedArea(view, rect());
        if (view->viewport().intersects(geometry)) {
            view->scheduleRepaint(this);
        }
    }
}

void Item::scheduleSceneRepaintInternal(const QRegion &region)
{
    if (Q_UNLIKELY(!m_scene)) {
        return;
    }
    const QList<RenderView *> views = m_scene->views();
    for (RenderView *view : views) {
        if (!view->shouldRenderItem(this)) {
            continue;
        }
        const QRegion dirtyRegion = paintedArea(view, region) & view->viewport().toAlignedRect();
        if (!dirtyRegion.isEmpty()) {
            m_scene->addRepaint(view, dirtyRegion);
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

bool Item::hasRepaints(RenderView *view) const
{
    const auto it = m_repaints.find(view);
    return it != m_repaints.end() && !it->isEmpty();
}

QRegion Item::takeRepaints(RenderView *view)
{
    auto &repaints = m_repaints[view];
    QRegion reg;
    std::swap(reg, repaints);
    return reg;
}

void Item::resetRepaints(RenderView *view)
{
    m_repaints.insert(view, QRegion());
}

void Item::removeRepaints(RenderView *view)
{
    m_repaints.remove(view);
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

void Item::scheduleSceneRepaint(const QRectF &region)
{
    scheduleSceneRepaint(QRegion(region.toAlignedRect()));
}

void Item::scheduleSceneRepaint(const QRegion &region)
{
    if (isVisible()) {
        scheduleSceneRepaintInternal(region);
    }
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
        scheduleSceneRepaintInternal(boundingRect().toAlignedRect());
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

RenderingIntent Item::renderingIntent() const
{
    return m_renderingIntent;
}

void Item::setColorDescription(const ColorDescription &description)
{
    m_colorDescription = description;
}

void Item::setRenderingIntent(RenderingIntent intent)
{
    m_renderingIntent = intent;
}

PresentationModeHint Item::presentationHint() const
{
    return m_presentationHint;
}

void Item::setPresentationHint(PresentationModeHint hint)
{
    m_presentationHint = hint;
}

bool Item::hasEffects() const
{
    return m_effectCount != 0;
}

void Item::addEffect()
{
    m_effectCount++;
}

void Item::removeEffect()
{
    Q_ASSERT(m_effectCount > 0);
    m_effectCount--;
}

void Item::framePainted(Output *output, OutputFrame *frame, std::chrono::milliseconds timestamp)
{
    // The visibility of the item itself is not checked here to be able to paint hidden items for
    // things like screncasts or thumbnails
    handleFramePainted(output, frame, timestamp);
    for (const auto child : std::as_const(m_childItems)) {
        if (child->explicitVisible() && workspace()->outputAt(child->mapToScene(child->boundingRect()).center()) == output) {
            child->framePainted(output, frame, timestamp);
        }
    }
}

bool Item::isAncestorOf(const Item *item) const
{
    return std::ranges::any_of(m_childItems, [item](const Item *child) {
        return child == item || child->isAncestorOf(item);
    });
}

void Item::handleFramePainted(Output *output, OutputFrame *frame, std::chrono::milliseconds timestamp)
{
}

} // namespace KWin

#include "moc_item.cpp"
