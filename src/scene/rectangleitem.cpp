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

} // namespace KWin

#include "moc_rectangleitem.cpp"
