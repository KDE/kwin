/*
    SPDX-FileCopyrightText: 2025 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "utils/gravity.h"

namespace KWin
{

Gravity::Gravity()
    : m_kind(None)
{
}

Gravity::Gravity(Kind kind)
    : m_kind(kind)
{
}

Gravity::operator Gravity::Kind() const
{
    return m_kind;
}

QRectF Gravity::apply(const QRectF &rect, const QRectF &bounds) const
{
    QRectF geometry = rect;

    switch (m_kind) {
    case Gravity::TopLeft:
        geometry.moveRight(bounds.right());
        geometry.moveBottom(bounds.bottom());
        break;
    case Gravity::Top:
    case Gravity::TopRight:
        geometry.moveLeft(bounds.left());
        geometry.moveBottom(bounds.bottom());
        break;
    case Gravity::Right:
    case Gravity::BottomRight:
    case Gravity::Bottom:
    case Gravity::None:
        geometry.moveLeft(bounds.left());
        geometry.moveTop(bounds.top());
        break;
    case Gravity::BottomLeft:
    case Gravity::Left:
        geometry.moveRight(bounds.right());
        geometry.moveTop(bounds.top());
        break;
    }

    return geometry;
}

} // namespace KWin
