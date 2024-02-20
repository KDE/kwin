/*
    SPDX-FileCopyrightText: 2023 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "kwin_export.h"

#include <QRect>
#include <QVector2D>

namespace KWin
{

KWIN_EXPORT inline int snapToPixelGrid(qreal value)
{
    return std::round(value);
}

KWIN_EXPORT inline qreal snapToPixelGridF(qreal value)
{
    return std::round(value);
}

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

KWIN_EXPORT inline QVector2D snapToPixelGrid(const QVector2D &vector)
{
    return QVector2D(snapToPixelGridF(vector.x()), snapToPixelGridF(vector.y()));
}

} // namespace KWin
