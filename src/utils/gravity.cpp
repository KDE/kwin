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

RectF Gravity::apply(const RectF &rect, const RectF &bounds) const
{
    RectF geometry = rect;

    switch (m_kind) {
    case Gravity::TopLeft:
        geometry.moveRight(bounds.right());
        geometry.moveBottom(bounds.bottom());
        break;
    case Gravity::Top:
        geometry.moveBottom(bounds.bottom());
        geometry.moveHorizontalCenter(bounds.horizontalCenter());
        break;
    case Gravity::TopRight:
        geometry.moveLeft(bounds.left());
        geometry.moveBottom(bounds.bottom());
        break;
    case Gravity::Right:
        geometry.moveLeft(bounds.left());
        geometry.moveVerticalCenter(bounds.verticalCenter());
        break;
    case Gravity::None:
    case Gravity::BottomRight:
        geometry.moveLeft(bounds.left());
        geometry.moveTop(bounds.top());
        break;
    case Gravity::Bottom:
        geometry.moveHorizontalCenter(bounds.horizontalCenter());
        geometry.moveTop(bounds.top());
        break;
    case Gravity::BottomLeft:
        geometry.moveRight(bounds.right());
        geometry.moveTop(bounds.top());
        break;
    case Gravity::Left:
        geometry.moveRight(bounds.right());
        geometry.moveVerticalCenter(bounds.verticalCenter());
        break;
    }

    return geometry;
}

} // namespace KWin
