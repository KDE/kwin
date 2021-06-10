/*
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "surfaceitem.h"

namespace KWin
{

SurfaceItem::SurfaceItem(Scene::Window *window, Item *parent)
    : Item(window, parent)
{
}

QPointF SurfaceItem::mapToWindow(const QPointF &point) const
{
    return rootPosition() + point - window()->pos();
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

    Toplevel *toplevel = window()->window();
    Q_EMIT toplevel->damaged(toplevel, region);
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
            m_previousPixmap.reset();
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
            m_referencePixmapCounter++;
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
        WindowQuad quad(const_cast<SurfaceItem *>(this));

        const QPointF windowTopLeft = mapToWindow(rect.topLeft());
        const QPointF windowTopRight = mapToWindow(rect.topRight());
        const QPointF windowBottomRight = mapToWindow(rect.bottomRight());
        const QPointF windowBottomLeft = mapToWindow(rect.bottomLeft());

        const QPointF bufferTopLeft = mapToBuffer(rect.topLeft());
        const QPointF bufferTopRight = mapToBuffer(rect.topRight());
        const QPointF bufferBottomRight = mapToBuffer(rect.bottomRight());
        const QPointF bufferBottomLeft = mapToBuffer(rect.bottomLeft());

        quad[0] = WindowVertex(windowTopLeft, bufferTopLeft);
        quad[1] = WindowVertex(windowTopRight, bufferTopRight);
        quad[2] = WindowVertex(windowBottomRight, bufferBottomRight);
        quad[3] = WindowVertex(windowBottomLeft, bufferBottomLeft);

        quads << quad;
    }

    return quads;
}

PlatformSurfaceTexture::~PlatformSurfaceTexture()
{
}

SurfacePixmap::SurfacePixmap(PlatformSurfaceTexture *platformTexture, QObject *parent)
    : QObject(parent)
    , m_platformTexture(platformTexture)
{
}

void SurfacePixmap::update()
{
}

PlatformSurfaceTexture *SurfacePixmap::platformTexture() const
{
    return m_platformTexture.data();
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
