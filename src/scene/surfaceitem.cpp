/*
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "scene/surfaceitem.h"
#include "scene/scene.h"

namespace KWin
{

SurfaceItem::SurfaceItem(Scene *scene, Item *parent)
    : Item(scene, parent)
{
}

QMatrix4x4 SurfaceItem::surfaceToBufferMatrix() const
{
    return m_surfaceToBufferMatrix;
}

void SurfaceItem::setSurfaceToBufferMatrix(const QMatrix4x4 &matrix)
{
    m_surfaceToBufferMatrix = matrix;
    m_bufferToSurfaceMatrix = matrix.inverted();
}

QRectF SurfaceItem::bufferSourceBox() const
{
    return m_bufferSourceBox;
}

void SurfaceItem::setBufferSourceBox(const QRectF &box)
{
    m_bufferSourceBox = box;
}

OutputTransform SurfaceItem::bufferTransform() const
{
    return m_bufferTransform;
}

void SurfaceItem::setBufferTransform(OutputTransform transform)
{
    m_bufferTransform = transform;
}

QSize SurfaceItem::bufferSize() const
{
    return m_bufferSize;
}

void SurfaceItem::setBufferSize(const QSize &size)
{
    m_bufferSize = size;
}

QRegion SurfaceItem::mapFromBuffer(const QRegion &region) const
{
    QRegion result;
    for (const QRect &rect : region) {
        result += m_bufferToSurfaceMatrix.mapRect(QRectF(rect)).toAlignedRect();
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
    m_damage += region;

    const QRectF sourceBox = m_bufferTransform.map(m_bufferSourceBox, m_bufferSize);
    const qreal xScale = sourceBox.width() / size().width();
    const qreal yScale = sourceBox.height() / size().height();
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
}

WindowQuadList SurfaceItem::buildQuads() const
{
    if (!pixmap()) {
        return {};
    }

    const QVector<QRectF> region = shape();
    const auto size = pixmap()->size();

    WindowQuadList quads;
    quads.reserve(region.count());

    for (const QRectF rect : region) {
        WindowQuad quad;

        // Use toPoint to round the device position to match what we eventually
        // do for the geometry, otherwise we end up with mismatched UV
        // coordinates as the texture size is going to be in (rounded) device
        // coordinates as well.
        const QPointF bufferTopLeft = m_surfaceToBufferMatrix.map(rect.topLeft()).toPoint();
        const QPointF bufferTopRight = m_surfaceToBufferMatrix.map(rect.topRight()).toPoint();
        const QPointF bufferBottomRight = m_surfaceToBufferMatrix.map(rect.bottomRight()).toPoint();
        const QPointF bufferBottomLeft = m_surfaceToBufferMatrix.map(rect.bottomLeft()).toPoint();

        quad[0] = WindowVertex(rect.topLeft(), QPointF{bufferTopLeft.x() / size.width(), bufferTopLeft.y() / size.height()});
        quad[1] = WindowVertex(rect.topRight(), QPointF{bufferTopRight.x() / size.width(), bufferTopRight.y() / size.height()});
        quad[2] = WindowVertex(rect.bottomRight(), QPointF{bufferBottomRight.x() / size.width(), bufferBottomRight.y() / size.height()});
        quad[3] = WindowVertex(rect.bottomLeft(), QPointF{bufferBottomLeft.x() / size.width(), bufferBottomLeft.y() / size.height()});

        quads << quad;
    }

    return quads;
}

ContentType SurfaceItem::contentType() const
{
    return ContentType::None;
}

SurfaceTexture::~SurfaceTexture()
{
}

SurfacePixmap::SurfacePixmap(std::unique_ptr<SurfaceTexture> &&texture, QObject *parent)
    : QObject(parent)
    , m_texture(std::move(texture))
{
}

GraphicsBuffer *SurfacePixmap::buffer() const
{
    return m_bufferRef.buffer();
}

void SurfacePixmap::setBuffer(GraphicsBuffer *buffer)
{
    if (m_bufferRef.buffer() == buffer) {
        return;
    }
    m_bufferRef = buffer;
    if (m_bufferRef) {
        m_hasAlphaChannel = m_bufferRef->hasAlphaChannel();
        m_size = m_bufferRef->size();
    }
}

GraphicsBufferOrigin SurfacePixmap::bufferOrigin() const
{
    return m_bufferOrigin;
}

void SurfacePixmap::setBufferOrigin(GraphicsBufferOrigin origin)
{
    m_bufferOrigin = origin;
}

void SurfacePixmap::update()
{
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
