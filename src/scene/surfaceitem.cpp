/*
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "scene/surfaceitem.h"
#include "core/pixelgrid.h"
#include "scene/scene.h"

using namespace std::chrono_literals;

namespace KWin
{

SurfaceItem::SurfaceItem(Item *parent)
    : Item(parent)
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
        setBufferSize(buffer->size());
    } else {
        m_bufferRef = nullptr;
        setBufferSize(QSize(0, 0));
    }
}

QRectF SurfaceItem::bufferSourceBox() const
{
    return m_bufferSourceBox;
}

void SurfaceItem::setBufferSourceBox(const QRectF &box)
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
        discardPixmap();
        discardQuads();
    }
}

QRegion SurfaceItem::mapFromBuffer(const QRegion &region) const
{
    const QRectF sourceBox = m_bufferToSurfaceTransform.map(m_bufferSourceBox, m_bufferSize);
    const qreal xScale = m_destinationSize.width() / sourceBox.width();
    const qreal yScale = m_destinationSize.height() / sourceBox.height();

    QRegion result;
    for (QRectF rect : region) {
        const QRectF r = m_bufferToSurfaceTransform.map(rect, m_bufferSize).translated(-sourceBox.topLeft());
        result += QRectF(r.x() * xScale, r.y() * yScale, r.width() * xScale, r.height() * yScale).toAlignedRect();
    }
    return result;
}

static QRegion expandRegion(const QRegion &region, const QMargins &padding)
{
    if (region.isEmpty()) {
        return QRegion();
    }

    QRegion ret;
    for (const QRect &rect : region) {
        ret += rect.marginsAdded(padding);
    }

    return ret;
}

void SurfaceItem::addDamage(const QRegion &region)
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

    const QRectF sourceBox = m_bufferToSurfaceTransform.map(m_bufferSourceBox, m_bufferSize);
    const qreal xScale = sourceBox.width() / m_destinationSize.width();
    const qreal yScale = sourceBox.height() / m_destinationSize.height();
    const QRegion logicalDamage = mapFromBuffer(region);

    const auto delegates = scene()->delegates();
    for (SceneDelegate *delegate : delegates) {
        QRegion delegateDamage = logicalDamage;
        const qreal delegateScale = delegate->scale();
        if (xScale != delegateScale || yScale != delegateScale) {
            // Simplified version of ceil(ceil(0.5 * output_scale / surface_scale) / output_scale)
            const int xPadding = std::ceil(0.5 / xScale);
            const int yPadding = std::ceil(0.5 / yScale);
            delegateDamage = expandRegion(delegateDamage, QMargins(xPadding, yPadding, xPadding, yPadding));
        }
        scheduleRepaint(delegate, delegateDamage);
    }

    Q_EMIT damaged();
}

void SurfaceItem::resetDamage()
{
    m_damage = QRegion();
}

QRegion SurfaceItem::damage() const
{
    return m_damage;
}

SurfacePixmap *SurfaceItem::pixmap() const
{
    if (m_pixmap && m_pixmap->isValid()) {
        return m_pixmap.get();
    }
    if (m_previousPixmap && m_previousPixmap->isValid()) {
        return m_previousPixmap.get();
    }
    return nullptr;
}

SurfacePixmap *SurfaceItem::previousPixmap() const
{
    return m_previousPixmap.get();
}

void SurfaceItem::referencePreviousPixmap()
{
    if (m_previousPixmap && m_previousPixmap->isDiscarded()) {
        m_referencePixmapCounter++;
    }
}

void SurfaceItem::unreferencePreviousPixmap()
{
    if (!m_previousPixmap || !m_previousPixmap->isDiscarded()) {
        return;
    }
    m_referencePixmapCounter--;
    if (m_referencePixmapCounter == 0) {
        m_previousPixmap.reset();
    }
}

void SurfaceItem::updatePixmap()
{
    if (!m_pixmap) {
        m_pixmap = createPixmap();
    }
    if (m_pixmap->isValid()) {
        m_pixmap->update();
    } else {
        m_pixmap->create();
        if (m_pixmap->isValid()) {
            unreferencePreviousPixmap();
            discardQuads();
        }
    }
}

void SurfaceItem::discardPixmap()
{
    if (m_pixmap) {
        if (m_pixmap->isValid()) {
            m_previousPixmap = std::move(m_pixmap);
            m_previousPixmap->markAsDiscarded();
            referencePreviousPixmap();
        } else {
            m_pixmap.reset();
        }
    }
}

void SurfaceItem::destroyPixmap()
{
    m_pixmap.reset();
}

void SurfaceItem::preprocess()
{
    updatePixmap();

    if (SurfacePixmap *surfacePixmap = pixmap(); surfacePixmap && !surfacePixmap->isDiscarded()) {
        SurfaceTexture *surfaceTexture = surfacePixmap->texture();
        if (surfaceTexture->isValid()) {
            const QRegion region = damage();
            if (!region.isEmpty()) {
                surfaceTexture->update(region);
                resetDamage();
            }
        } else if (surfacePixmap->isValid()) {
            if (surfaceTexture->create()) {
                resetDamage();
            }
        }
    }
}

WindowQuadList SurfaceItem::buildQuads() const
{
    if (!pixmap()) {
        return {};
    }

    const QList<QRectF> region = shape();
    WindowQuadList quads;
    quads.reserve(region.count());

    const QRectF sourceBox = m_bufferToSurfaceTransform.map(m_bufferSourceBox, m_bufferSize);
    const qreal xScale = sourceBox.width() / m_destinationSize.width();
    const qreal yScale = sourceBox.height() / m_destinationSize.height();

    for (const QRectF rect : region) {
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

std::chrono::nanoseconds SurfaceItem::frameTimeEstimation() const
{
    if (m_lastDamage) {
        const auto diff = std::chrono::steady_clock::now() - *m_lastDamage;
        return std::max(m_frameTimeEstimation, diff);
    } else {
        return m_frameTimeEstimation;
    }
}

std::shared_ptr<SyncReleasePoint> SurfaceItem::bufferReleasePoint() const
{
    return m_bufferReleasePoint;
}

SurfaceTexture::~SurfaceTexture()
{
}

SurfacePixmap::SurfacePixmap(std::unique_ptr<SurfaceTexture> &&texture, SurfaceItem *item)
    : m_item(item)
    , m_texture(std::move(texture))
{
}

void SurfacePixmap::update()
{
}

SurfaceItem *SurfacePixmap::item() const
{
    return m_item;
}

SurfaceTexture *SurfacePixmap::texture() const
{
    return m_texture.get();
}

bool SurfacePixmap::hasAlphaChannel() const
{
    return m_hasAlphaChannel;
}

QSize SurfacePixmap::size() const
{
    return m_size;
}

bool SurfacePixmap::isDiscarded() const
{
    return m_isDiscarded;
}

void SurfacePixmap::markAsDiscarded()
{
    m_isDiscarded = true;
}

} // namespace KWin

#include "moc_surfaceitem.cpp"
