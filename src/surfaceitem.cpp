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
    if (m_previousPixmap) {
        m_referencePixmapCounter++;
    }
}

void SurfaceItem::unreferencePreviousPixmap()
{
    if (!m_previousPixmap) {
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

    if (m_pixmap && m_pixmap->isValid()) {
        PlatformSurfaceTexture *platformSurfaceTexture = m_pixmap->platformTexture();
        if (platformSurfaceTexture->isValid()) {
            if (!damage().isEmpty()) {
                platformSurfaceTexture->update(damage());
                resetDamage();
            }
        } else {
            if (platformSurfaceTexture->create()) {
                resetDamage();
            }
        }
    }
}

WindowQuadList SurfaceItem::buildQuads() const
{
    const QRegion region = shape();

    WindowQuadList quads;
    quads.reserve(region.rectCount());

    for (const QRectF rect : region) {
        WindowQuad quad;

        const QPointF bufferTopLeft = mapToBuffer(rect.topLeft());
        const QPointF bufferTopRight = mapToBuffer(rect.topRight());
        const QPointF bufferBottomRight = mapToBuffer(rect.bottomRight());
        const QPointF bufferBottomLeft = mapToBuffer(rect.bottomLeft());

        quad[0] = WindowVertex(rect.topLeft(), bufferTopLeft);
        quad[1] = WindowVertex(rect.topRight(), bufferTopRight);
        quad[2] = WindowVertex(rect.bottomRight(), bufferBottomRight);
        quad[3] = WindowVertex(rect.bottomLeft(), bufferBottomLeft);

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

} // namespace KWin
