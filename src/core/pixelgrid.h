/*
    SPDX-FileCopyrightText: 2023 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "core/rect.h"

namespace KWin
{

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

KWIN_EXPORT constexpr RectF snapToPixels(const RectF &logicalValue, double scale)
{
    const QPointF topLeft = snapToPixels(logicalValue.topLeft(), scale);
    const QPointF bottomRight = snapToPixels(logicalValue.bottomRight(), scale);
    return RectF(topLeft, bottomRight);
}

} // namespace KWin
