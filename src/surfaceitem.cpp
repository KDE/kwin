/*
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "surfaceitem.h"
#include "deleted.h"

namespace KWin
{

SurfaceItem::SurfaceItem(Toplevel *window, Item *parent)
    : Item(parent)
    , m_window(window)
{
    connect(window, &Toplevel::windowClosed, this, &SurfaceItem::handleWindowClosed);
}

Toplevel *SurfaceItem::window() const
{
    return m_window;
}

void SurfaceItem::handleWindowClosed(Toplevel *original, Deleted *deleted)
{
    Q_UNUSED(original)
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

QRegion SurfaceItem::shape() const
{
    return QRegion();
}

QRegion SurfaceItem::opaque() const
{
    return QRegion();
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
        return m_pixmap.data();
    }
    if (m_previousPixmap && m_previousPixmap->isValid()) {
        return m_previousPixmap.data();
    }
    return nullptr;
}

SurfacePixmap *SurfaceItem::previousPixmap() const
{
    return m_previousPixmap.data();
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
    if (m_pixmap.isNull()) {
        m_pixmap.reset(createPixmap());
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
    if (!m_pixmap.isNull()) {
        if (m_pixmap->isValid()) {
            m_previousPixmap.reset(m_pixmap.take());
            m_previousPixmap->markAsDiscarded();
            referencePreviousPixmap();
        } else {
            m_pixmap.reset();
        }
    }
    addDamage(rect());
}

void SurfaceItem::preprocess()
{
    updatePixmap();
}

WindowQuadList SurfaceItem::buildQuads() const
{
    const QRegion region = shape();

    WindowQuadList quads;
    quads.reserve(region.rectCount());

    for (const QRectF rect : region) {
        WindowQuad quad;

        const QPointF bufferTopLeft = m_surfaceToBufferMatrix.map(rect.topLeft());
        const QPointF bufferTopRight = m_surfaceToBufferMatrix.map(rect.topRight());
        const QPointF bufferBottomRight = m_surfaceToBufferMatrix.map(rect.bottomRight());
        const QPointF bufferBottomLeft = m_surfaceToBufferMatrix.map(rect.bottomLeft());

        quad[0] = WindowVertex(rect.topLeft(), bufferTopLeft);
        quad[1] = WindowVertex(rect.topRight(), bufferTopRight);
        quad[2] = WindowVertex(rect.bottomRight(), bufferBottomRight);
        quad[3] = WindowVertex(rect.bottomLeft(), bufferBottomLeft);

        quads << quad;
    }

    return quads;
}

SurfaceTexture::~SurfaceTexture()
{
}

SurfacePixmap::SurfacePixmap(SurfaceTexture *texture, QObject *parent)
    : QObject(parent)
    , m_texture(texture)
{
}

void SurfacePixmap::update()
{
}

SurfaceTexture *SurfacePixmap::texture() const
{
    return m_texture.data();
}

bool SurfacePixmap::hasAlphaChannel() const
{
    return m_hasAlphaChannel;
}

QSize SurfacePixmap::size() const
{
    return m_size;
}

QRect SurfacePixmap::contentsRect() const
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
