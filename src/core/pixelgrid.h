/*
    SPDX-FileCopyrightText: 2023 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "kwin_export.h"

#include <QRect>

namespace KWin
{

KWIN_EXPORT inline QPoint snapToPixelGrid(const QPointF &point)
{
    return QPoint(std::round(point.x()), std::round(point.y()));
}

KWIN_EXPORT inline QPointF snapToPixelGridF(const QPointF &point)
{
    return QPointF(std::round(point.x()), std::round(point.y()));
}

KWIN_EXPORT inline QRect snapToPixelGrid(const QRectF &rect)
{
    const QPoint topLeft = snapToPixelGrid(rect.topLeft());
    const QPoint bottomRight = snapToPixelGrid(rect.bottomRight());
    return QRect(topLeft.x(), topLeft.y(), bottomRight.x() - topLeft.x(), bottomRight.y() - topLeft.y());
}

KWIN_EXPORT inline QRectF snapToPixelGridF(const QRectF &rect)
{
    return QRectF(snapToPixelGridF(rect.topLeft()), snapToPixelGridF(rect.bottomRight()));
}

} // namespace KWin
