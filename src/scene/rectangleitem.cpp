/*
    SPDX-FileCopyrightText: 2024 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "scene/rectangleitem.h"

namespace KWin
{

RectangleItem::RectangleItem(Item *parent)
    : Item(parent)
{
}

QColor RectangleItem::color() const
{
    return m_color;
}

void RectangleItem::setColor(const QColor &color)
{
    if (m_color != color) {
        m_color = color;
        scheduleRepaint(boundingRect());
    }
}

WindowQuadList RectangleItem::buildQuads() const
{
    const QRectF geometry = rect();
    if (geometry.isEmpty()) {
        return WindowQuadList();
    }

    WindowQuad quad;
    quad[0] = WindowVertex(geometry.topLeft(), QPointF(0, 0));
    quad[1] = WindowVertex(geometry.topRight(), QPointF(1, 0));
    quad[2] = WindowVertex(geometry.bottomRight(), QPointF(1, 1));
    quad[3] = WindowVertex(geometry.bottomLeft(), QPointF(0, 1));

    WindowQuadList list;
    list.append(quad);
    return list;
}

} // namespace KWin

#include "moc_rectangleitem.cpp"
