/*
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "surfaceitem.h"
#include "deleted.h"

namespace KWin
{

SurfaceItem::SurfaceItem(Window *window, Item *parent)
    : Item(parent)
    , m_window(window)
{
    connect(window, &Window::windowClosed, this, &SurfaceItem::handleWindowClosed);
}

Window *SurfaceItem::window() const
{
    return m_window;
}

void SurfaceItem::handleWindowClosed(Window *original, Deleted *deleted)
{
    m_window = deleted;
}

QMatrix4x4 SurfaceItem::surfaceToBufferMatrix() const
{
    return m_surfaceToBufferMatrix;
}

void SurfaceItem::setSurfaceToBufferMatrix(const QMatrix4x4 &matrix)
{
    m_surfaceToBufferMatrix = matrix;
}

void SurfaceItem::addDamage(const QRegion &region)
{
    m_damage += region;
    scheduleRepaint(region);

    Q_EMIT m_window->damaged(m_window, region);
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
    addDamage(rect().toAlignedRect());
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

QRectF SurfacePixmap::contentsRect() const
{
    return m_contentsRect;
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
