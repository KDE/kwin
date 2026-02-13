/*
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "scene/surfaceitem.h"
#include "core/pixelgrid.h"
#include "scene/itemrenderer.h"
#include "scene/scene.h"
#include "scene/texture.h"

using namespace std::chrono_literals;

namespace KWin
{

SurfaceItem::SurfaceItem(Item *parent)
    : Item(parent)
{
}

SurfaceItem::~SurfaceItem()
{
}

QSizeF SurfaceItem::destinationSize() const
{
    return m_destinationSize;
}

void SurfaceItem::setDestinationSize(const QSizeF &size)
{
    if (m_destinationSize != size) {
        m_destinationSize = size;
        setSize(size);
        discardQuads();
    }
}

GraphicsBuffer *SurfaceItem::buffer() const
{
    return m_bufferRef.buffer();
}

void SurfaceItem::setBuffer(GraphicsBuffer *buffer)
{
    if (buffer) {
        m_bufferRef = buffer;
        m_hasAlphaChannel = buffer->hasAlphaChannel();
        setBufferSize(buffer->size());
    } else {
        m_bufferRef = nullptr;
        m_hasAlphaChannel = false;
        setBufferSize(QSize(0, 0));
    }
}

RectF SurfaceItem::bufferSourceBox() const
{
    return m_bufferSourceBox;
}

void SurfaceItem::setBufferSourceBox(const RectF &box)
{
    if (m_bufferSourceBox != box) {
        m_bufferSourceBox = box;
        discardQuads();
    }
}

OutputTransform SurfaceItem::bufferTransform() const
{
    return m_surfaceToBufferTransform;
}

void SurfaceItem::setBufferTransform(OutputTransform transform)
{
    if (m_surfaceToBufferTransform != transform) {
        m_surfaceToBufferTransform = transform;
        m_bufferToSurfaceTransform = transform.inverted();
        discardQuads();
    }
}

QSize SurfaceItem::bufferSize() const
{
    return m_bufferSize;
}

void SurfaceItem::setBufferSize(const QSize &size)
{
    if (m_bufferSize != size) {
        m_bufferSize = size;
        discardQuads();
    }
}

Region SurfaceItem::mapFromBuffer(const Region &region) const
{
    const RectF sourceBox = m_bufferToSurfaceTransform.map(m_bufferSourceBox, m_bufferSize);
    const qreal xScale = m_destinationSize.width() / sourceBox.width();
    const qreal yScale = m_destinationSize.height() / sourceBox.height();

    Region result;
    for (RectF rect : region.rects()) {
        const RectF r = m_bufferToSurfaceTransform.map(rect, m_bufferSize).translated(-sourceBox.topLeft());
        result += RectF(r.x() * xScale, r.y() * yScale, r.width() * xScale, r.height() * yScale).toAlignedRect();
    }
    return result;
}

static Region expandRegion(const Region &region, const QMargins &padding)
{
    if (region.isEmpty()) {
        return Region();
    }

    Region ret;
    for (const Rect &rect : region.rects()) {
        ret += rect.marginsAdded(padding);
    }

    return ret;
}

void SurfaceItem::addDamage(const Region &region)
{
    if (m_lastDamage) {
        const auto diff = std::chrono::steady_clock::now() - *m_lastDamage;
        m_lastDamageTimeDiffs.push_back(diff);
        if (m_lastDamageTimeDiffs.size() > 100) {
            m_lastDamageTimeDiffs.pop_front();
        }
        m_frameTimeEstimation = std::accumulate(m_lastDamageTimeDiffs.begin(), m_lastDamageTimeDiffs.end(), 0ns) / m_lastDamageTimeDiffs.size();
    }
    m_lastDamage = std::chrono::steady_clock::now();
    m_damage += region;

    const RectF sourceBox = m_bufferToSurfaceTransform.map(m_bufferSourceBox, m_bufferSize);
    const qreal xScale = sourceBox.width() / m_destinationSize.width();
    const qreal yScale = sourceBox.height() / m_destinationSize.height();
    const Region logicalDamage = mapFromBuffer(region);

    const auto views = scene()->views();
    for (RenderView *view : views) {
        Region viewDamage = logicalDamage;
        const qreal viewScale = view->scale();
        if (xScale != viewScale || yScale != viewScale) {
            // Simplified version of ceil(ceil(0.5 * output_scale / surface_scale) / output_scale)
            const int xPadding = std::ceil(0.5 / xScale);
            const int yPadding = std::ceil(0.5 / yScale);
            viewDamage = expandRegion(viewDamage, QMargins(xPadding, yPadding, xPadding, yPadding));
        }
        scheduleRepaint(view, viewDamage);
    }

    Q_EMIT damaged();
}

void SurfaceItem::resetDamage()
{
    m_damage = Region();
}

Region SurfaceItem::damage() const
{
    return m_damage;
}

Texture *SurfaceItem::texture() const
{
    return m_texture.get();
}

void SurfaceItem::destroyTexture()
{
    m_texture.reset();
}

void SurfaceItem::preprocess()
{
    ItemRenderer *itemRenderer = scene()->renderer();

    if (!m_texture || m_texture->size() != m_bufferSize) {
        m_texture = itemRenderer->createTexture(buffer());
        if (m_texture) {
            resetDamage();
        }
        return;
    }

    const Region region = damage();
    if (!region.isEmpty()) {
        m_texture->attach(buffer(), region);
        resetDamage();
    }
}

WindowQuadList SurfaceItem::buildQuads() const
{
    const QList<RectF> region = shape();
    WindowQuadList quads;
    quads.reserve(region.count());

    const RectF sourceBox = m_bufferToSurfaceTransform.map(m_bufferSourceBox, m_bufferSize);
    const qreal xScale = sourceBox.width() / m_destinationSize.width();
    const qreal yScale = sourceBox.height() / m_destinationSize.height();

    for (const RectF rect : region) {
        WindowQuad quad;

        const QPointF bufferTopLeft = snapToPixelGridF(m_bufferSourceBox.topLeft() + m_surfaceToBufferTransform.map(QPointF(rect.left() * xScale, rect.top() * yScale), sourceBox.size()));
        const QPointF bufferTopRight = snapToPixelGridF(m_bufferSourceBox.topLeft() + m_surfaceToBufferTransform.map(QPointF(rect.right() * xScale, rect.top() * yScale), sourceBox.size()));
        const QPointF bufferBottomRight = snapToPixelGridF(m_bufferSourceBox.topLeft() + m_surfaceToBufferTransform.map(QPointF(rect.right() * xScale, rect.bottom() * yScale), sourceBox.size()));
        const QPointF bufferBottomLeft = snapToPixelGridF(m_bufferSourceBox.topLeft() + m_surfaceToBufferTransform.map(QPointF(rect.left() * xScale, rect.bottom() * yScale), sourceBox.size()));

        quad[0] = WindowVertex(rect.topLeft(), bufferTopLeft);
        quad[1] = WindowVertex(rect.topRight(), bufferTopRight);
        quad[2] = WindowVertex(rect.bottomRight(), bufferBottomRight);
        quad[3] = WindowVertex(rect.bottomLeft(), bufferBottomLeft);

        quads << quad;
    }

    return quads;
}

void SurfaceItem::releaseResources()
{
    m_texture.reset();
}

ContentType SurfaceItem::contentType() const
{
    return ContentType::None;
}

void SurfaceItem::setScanoutHint(DrmDevice *device, const QHash<uint32_t, QList<uint64_t>> &drmFormats)
{
}

void SurfaceItem::freeze()
{
}

std::optional<std::chrono::nanoseconds> SurfaceItem::recursiveFrameTimeEstimation() const
{
    std::optional<std::chrono::nanoseconds> ret = frameTimeEstimation();
    const auto children = childItems();
    for (Item *child : children) {
        const auto other = static_cast<SurfaceItem *>(child)->recursiveFrameTimeEstimation();
        if (!other.has_value()) {
            continue;
        }
        if (ret.has_value()) {
            ret = std::min(*ret, *other);
        } else {
            ret = other;
        }
    }
    return ret;
}

std::optional<std::chrono::nanoseconds> SurfaceItem::frameTimeEstimation() const
{
    if (m_lastDamage && std::chrono::steady_clock::now() - *m_lastDamage > std::chrono::milliseconds(100)) {
        // the surface seems to have stopped rendering entirely
        return std::nullopt;
    } else {
        return m_frameTimeEstimation;
    }
}

std::shared_ptr<SyncReleasePoint> SurfaceItem::bufferReleasePoint() const
{
    return m_bufferReleasePoint;
}

bool SurfaceItem::hasAlphaChannel() const
{
    return m_hasAlphaChannel;
}

} // namespace KWin

#include "moc_surfaceitem.cpp"
