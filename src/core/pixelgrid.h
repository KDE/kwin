/*
    SPDX-FileCopyrightText: 2023 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "kwin_export.h"

#include <QPointF>
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

KWIN_EXPORT constexpr double snapToPixels(double logicalValue, double scale)
{
    return std::round(logicalValue * scale) / scale;
}

KWIN_EXPORT constexpr QPointF snapToPixels(const QPointF &logicalValue, double scale)
{
    return QPointF(snapToPixels(logicalValue.x(), scale), snapToPixels(logicalValue.y(), scale));
}

KWIN_EXPORT constexpr QSizeF snapToPixels(const QSizeF &logicalValue, double scale)
{
    return QSizeF(snapToPixels(logicalValue.width(), scale), snapToPixels(logicalValue.height(), scale));
}

KWIN_EXPORT constexpr QRectF snapToPixels(const QRectF &logicalValue, double scale)
{
    const QPointF topLeft = snapToPixels(logicalValue.topLeft(), scale);
    const QPointF bottomRight = snapToPixels(logicalValue.bottomRight(), scale);
    return QRectF(topLeft, bottomRight);
}

} // namespace KWin
