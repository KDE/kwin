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

    Q_EMIT childRemoved(item);
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
        for (auto it = m_deviceRepaints.constBegin(); it != m_deviceRepaints.constEnd(); ++it) {
            RenderView *view = it.key();
            const Region &dirty = it.value();
            if (!dirty.isEmpty()) {
                m_scene->addDeviceRepaint(view, dirty);
            }
        }
        m_deviceRepaints.clear();
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
        scheduleMoveRepaint(this);
        m_position = point;
        updateItemToSceneTransform();
        if (m_parentItem) {
            m_parentItem->updateBoundingRect();
        }
        scheduleMoveRepaint(this);
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

void Item::setGeometry(const RectF &rect)
{
    setPosition(rect.topLeft());
    setSize(rect.size());
}

RectF Item::rect() const
{
    return RectF(QPoint(0, 0), size());
}

RectF Item::boundingRect() const
{
    return m_boundingRect;
}

void Item::updateBoundingRect()
{
    RectF boundingRect = rect();
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

QList<RectF> Item::shape() const
{
    return QList<RectF>();
}

Region Item::opaque() const
{
    return Region();
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

Region Item::mapToView(const Region &region, const RenderView *view) const
{
    Region ret;
    for (RectF rect : region.rects()) {
        ret |= mapToView(rect, view).toAlignedRect();
    }
    return ret;
}

RectF Item::mapToView(const RectF &rect, const RenderView *view) const
{
    const auto snappedPosition = snapToPixels(m_position, view->scale());
    const RectF ret = rect.translated(snappedPosition);
    if (m_parentItem) {
        return m_parentItem->mapToView(ret, view);
    } else {
        return ret;
    }
}

Region Item::mapToScene(const Region &region) const
{
    if (region.isEmpty()) {
        return Region();
    }
    Region ret;
    for (const Rect &rect : region.rects()) {
        ret |= m_itemToSceneTransform.mapRect(rect);
    }
    return ret;
}

RectF Item::mapToScene(const RectF &rect) const
{
    if (rect.isEmpty()) {
        return Rect();
    }
    return m_itemToSceneTransform.mapRect(rect);
}

RectF Item::mapFromScene(const RectF &rect) const
{
    if (rect.isEmpty()) {
        return Rect();
    }
    return m_sceneToItemTransform.mapRect(rect);
}

Rect Item::paintedDeviceArea(RenderView *view, const RectF &rect) const
{
    const qreal scale = view->scale();

    RectF snapped = rect.scaled(scale).rounded();
    for (const Item *item = this; item; item = item->parentItem()) {
        if (!item->m_transform.isIdentity()) {
            snapped = (QTransform::fromScale(1 / scale, 1 / scale) * item->m_transform * QTransform::fromScale(scale, scale))
                          .mapRect(snapped);
        }

        snapped.translate(snapToPixelGridF(item->position() * scale));
    }
    return view->mapToDeviceCoordinatesAligned(scaledRect(snapped, 1.0 / scale)) & view->deviceRect();
}

Region Item::paintedDeviceArea(RenderView *view, const Region &region) const
{
    Region ret;
    for (RectF part : region.rects()) {
        ret |= paintedDeviceArea(view, part);
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

void Item::scheduleRepaint(const Region &region)
{
    if (isVisible()) {
        scheduleRepaintInternal(region);
    }
}

void Item::scheduleRepaint(RenderView *view, const Region &region)
{
    if (isVisible()) {
        scheduleRepaintInternal(view, region);
    }
}

void Item::scheduleRepaintInternal(const Region &region)
{
    if (Q_UNLIKELY(!m_scene)) {
        return;
    }
    const QList<RenderView *> views = m_scene->views();
    for (RenderView *view : views) {
        if (!view->shouldRenderItem(this)) {
            continue;
        }
        const Region dirtyRegion = paintedDeviceArea(view, region);
        if (!dirtyRegion.isEmpty()) {
            m_deviceRepaints[view] += dirtyRegion;
            view->scheduleRepaint(this);
        }
    }
}

void Item::scheduleMoveRepaint(Item *originallyMovedItem)
{
    if (Q_UNLIKELY(!m_scene) || !isVisible()) {
        return;
    }
    const QList<RenderView *> views = m_scene->views();
    for (RenderView *view : views) {
        if (!view->shouldRenderItem(this)) {
            continue;
        }
        const Region dirtyRegion = paintedDeviceArea(view, rect());
        if (!dirtyRegion.isEmpty()) {
            // we can skip the move repaint if the parent item was moved
            // and this item was just implicitly moved as a consequence
            if (!view->canSkipMoveRepaint(originallyMovedItem)) {
                m_deviceRepaints[view] += dirtyRegion;
            }
            view->scheduleRepaint(this);
        }
    }
    for (Item *child : std::as_const(m_childItems)) {
        child->scheduleMoveRepaint(originallyMovedItem);
    }
}

void Item::scheduleRepaintInternal(RenderView *view, const Region &region)
{
    if (Q_UNLIKELY(!m_scene) || !view->shouldRenderItem(this)) {
        return;
    }
    const Region dirtyRegion = paintedDeviceArea(view, region);
    if (!dirtyRegion.isEmpty()) {
        m_deviceRepaints[view] += dirtyRegion;
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
        const Rect geometry = paintedDeviceArea(view, rect());
        if (!geometry.isEmpty()) {
            view->scheduleRepaint(this);
        }
    }
}

void Item::scheduleSceneRepaintInternal(const Region &region)
{
    if (Q_UNLIKELY(!m_scene)) {
        return;
    }
    const QList<RenderView *> views = m_scene->views();
    for (RenderView *view : views) {
        if (!view->shouldRenderItem(this) && !view->shouldRenderHole(this)) {
            continue;
        }
        const Region dirtyRegion = paintedDeviceArea(view, region);
        if (!dirtyRegion.isEmpty()) {
            m_scene->addDeviceRepaint(view, dirtyRegion);
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
    const auto it = m_deviceRepaints.find(view);
    return it != m_deviceRepaints.end() && !it->isEmpty();
}

Region Item::takeDeviceRepaints(RenderView *view)
{
    auto &repaints = m_deviceRepaints[view];
    Region reg;
    std::swap(reg, repaints);
    return reg;
}

void Item::resetRepaints(RenderView *view)
{
    m_deviceRepaints.insert(view, Region());
}

void Item::removeRepaints(RenderView *view)
{
    m_deviceRepaints.remove(view);
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

BorderRadius Item::borderRadius() const
{
    return m_borderRadius;
}

void Item::setBorderRadius(const BorderRadius &radius)
{
    if (m_borderRadius != radius) {
        m_borderRadius = radius;
        scheduleRepaint(rect());
    }
}

void Item::scheduleRepaint(const RectF &region)
{
    scheduleRepaint(Region(region.roundedOut()));
}

void Item::scheduleSceneRepaint(const RectF &region)
{
    scheduleSceneRepaint(Region(region.roundedOut()));
}

void Item::scheduleSceneRepaint(const Region &region)
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
    Q_EMIT visibleChanged();
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

const std::shared_ptr<ColorDescription> &Item::colorDescription() const
{
    return m_colorDescription;
}

RenderingIntent Item::renderingIntent() const
{
    return m_renderingIntent;
}

void Item::setColorDescription(const std::shared_ptr<ColorDescription> &description)
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

void Item::framePainted(RenderView *view, LogicalOutput *output, OutputFrame *frame, std::chrono::milliseconds timestamp)
{
    // The visibility of the item itself is not checked here to be able to paint hidden items for
    // things like screncasts or thumbnails
    handleFramePainted(output, frame, timestamp);
    for (const auto child : std::as_const(m_childItems)) {
        if (child->explicitVisible() && workspace()->outputAt(child->mapToScene(child->boundingRect()).center()) == output) {
            child->framePainted(view, output, frame, timestamp);
        }
    }
}

bool Item::isAncestorOf(const Item *item) const
{
    return std::ranges::any_of(m_childItems, [item](const Item *child) {
        return child == item || child->isAncestorOf(item);
    });
}

void Item::handleFramePainted(LogicalOutput *output, OutputFrame *frame, std::chrono::milliseconds timestamp)
{
}

void Item::releaseResources()
{
}

bool Item::hasVisibleContents() const
{
    if (!isVisible()) {
        return false;
    }
    return !m_size.isEmpty() || std::ranges::any_of(m_childItems, [](Item *item) {
        return item->hasVisibleContents();
    });
}

} // namespace KWin

#include "moc_item.cpp"
