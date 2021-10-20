/*
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "surfaceitem.h"
#include "toplevel.h"

namespace KWin
{

SurfaceItem::SurfaceItem(Surface *surface, Item *parent)
    : Item(parent)
    , SurfaceView(surface)
{
    connect(surface, &Surface::damaged, this, &SurfaceItem::addDamage);
    connect(surface, &Surface::positionChanged, this, &SurfaceItem::handlePositionChanged);
    connect(surface, &Surface::sizeChanged, this, &SurfaceItem::handleSizeChanged);
    connect(surface, &Surface::belowChanged, this, &SurfaceItem::handleBelowChanged);
    connect(surface, &Surface::aboveChanged, this, &SurfaceItem::handleAboveChanged);

    handlePositionChanged();
    handleSizeChanged();
    handleAboveChanged();
    handleBelowChanged();
}

void SurfaceItem::surfaceFrame()
{
    scheduleFrame();
}

void SurfaceItem::surfacePixmapInvalidated()
{
    discardPixmap();
}

void SurfaceItem::surfaceQuadsInvalidated()
{
    discardQuads();
}

void SurfaceItem::handlePositionChanged()
{
    setPosition(surface()->position());
}

void SurfaceItem::handleSizeChanged()
{
    setSize(surface()->size());
}

SurfaceItem *SurfaceItem::getOrCreateSubSurfaceItem(Surface *subsurface)
{
    SurfaceItem *&item = m_subsurfaces[subsurface];
    if (!item) {
        item = new SurfaceItem(subsurface, this);
        item->setParent(this);
        connect(subsurface, &QObject::destroyed, this, [this, subsurface]() {
            delete m_subsurfaces.take(subsurface);
        });
    }
    return item;
}

void SurfaceItem::handleBelowChanged()
{
    const QList<Surface *> below = surface()->below();
    for (int i = 0; i < below.size(); ++i) {
        getOrCreateSubSurfaceItem(below[i])->setZ(i - below.size());
    }
}

void SurfaceItem::handleAboveChanged()
{
    const QList<Surface *> above = surface()->above();
    for (int i = 0; i < above.size(); ++i) {
        getOrCreateSubSurfaceItem(above[i])->setZ(i);
    }
}

QRegion SurfaceItem::shape() const
{
    return surface()->shape();
}

QRegion SurfaceItem::opaque() const
{
    return surface()->opaque();
}

void SurfaceItem::addDamage(const QRegion &region)
{
    Toplevel *window = surface()->window();

    m_damage += region;
    scheduleRepaint(region);

    Q_EMIT window->damaged(window, region);
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
        m_pixmap.reset(surface()->createPixmap());
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
    const QMatrix4x4 surfaceToBufferMatrix = surface()->surfaceToBufferMatrix();

    WindowQuadList quads;
    quads.reserve(region.rectCount());

    for (const QRectF rect : region) {
        WindowQuad quad;

        const QPointF bufferTopLeft = surfaceToBufferMatrix.map(rect.topLeft());
        const QPointF bufferTopRight = surfaceToBufferMatrix.map(rect.topRight());
        const QPointF bufferBottomRight = surfaceToBufferMatrix.map(rect.bottomRight());
        const QPointF bufferBottomLeft = surfaceToBufferMatrix.map(rect.bottomLeft());

        quad[0] = WindowVertex(rect.topLeft(), bufferTopLeft);
        quad[1] = WindowVertex(rect.topRight(), bufferTopRight);
        quad[2] = WindowVertex(rect.bottomRight(), bufferBottomRight);
        quad[3] = WindowVertex(rect.bottomLeft(), bufferBottomLeft);

        quads << quad;
    }

    return quads;
}

} // namespace KWin
