/*
    SPDX-FileCopyrightText: 2025 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#include "scene/outlinedborderitem.h"

namespace KWin
{

OutlinedBorderItem::OutlinedBorderItem(const QRectF &innerRect, const BorderOutline &outline, Item *parent)
    : Item(parent)
    , m_innerRect(innerRect)
    , m_outline(outline)
{
    setGeometry(m_outline.inflate(m_innerRect));
}

QRectF OutlinedBorderItem::innerRect() const
{
    return m_innerRect;
}

void OutlinedBorderItem::setInnerRect(const QRectF &rect)
{
    if (m_innerRect != rect) {
        m_innerRect = rect;
        setGeometry(m_outline.inflate(m_innerRect));
        discardQuads();
    }
}

BorderOutline OutlinedBorderItem::outline() const
{
    return m_outline;
}

void OutlinedBorderItem::setOutline(const BorderOutline &outline)
{
    if (m_outline != outline) {
        m_outline = outline;
        setGeometry(m_outline.inflate(m_innerRect));
        discardQuads();
        scheduleSceneRepaint(boundingRect());
    }
}

WindowQuadList OutlinedBorderItem::buildQuads() const
{
    if (m_innerRect.isEmpty()) {
        return WindowQuadList();
    }

    const qreal thickness = m_outline.thickness();
    const BorderRadius radius = m_outline.radius();
    const QSizeF extents = size();

    WindowQuadList quads;
    quads.reserve(8);

    qreal topLeft = 0.0;
    if (radius.topLeft() > 0) {
        topLeft = thickness + radius.topLeft();
        quads << WindowQuad::fromRect(QRectF(0, 0, topLeft, topLeft));
    }

    qreal topRight = 0.0;
    if (radius.topRight() > 0) {
        topRight = thickness + radius.topRight();
        quads << WindowQuad::fromRect(QRectF(extents.width() - topRight, 0, topRight, topRight));
    }

    qreal bottomRight = 0.0;
    if (radius.bottomRight() > 0) {
        bottomRight = thickness + radius.bottomRight();
        quads << WindowQuad::fromRect(QRectF(extents.width() - bottomRight, extents.height() - bottomRight, bottomRight, bottomRight));
    }

    qreal bottomLeft = 0.0;
    if (radius.bottomLeft() > 0) {
        bottomLeft = thickness + radius.bottomLeft();
        quads << WindowQuad::fromRect(QRectF(0, extents.height() - bottomLeft, bottomLeft, bottomLeft));
    }

    quads << WindowQuad::fromRect(QRectF(topLeft, 0, extents.width() - topLeft - topRight, thickness));
    quads << WindowQuad::fromRect(QRectF(bottomLeft, extents.height() - thickness, extents.width() - bottomLeft - bottomRight, thickness));

    const qreal leftTopPadding = topLeft > 0 ? topLeft : thickness;
    const qreal leftBottomPadding = bottomLeft > 0 ? bottomLeft : thickness;
    quads << WindowQuad::fromRect(QRectF(0, leftTopPadding, thickness, extents.height() - leftTopPadding - leftBottomPadding));

    const qreal rightTopPadding = topRight > 0 ? topRight : thickness;
    const qreal rightBottomPadding = bottomRight > 0 ? bottomRight : thickness;
    quads << WindowQuad::fromRect(QRectF(extents.width() - thickness, rightTopPadding, thickness, extents.height() - rightTopPadding - rightBottomPadding));

    return quads;
}

} // namespace KWin

#include "moc_outlinedborderitem.cpp"
