/*
    SPDX-FileCopyrightText: 2024 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#pragma once

#include "core/rect.h"
#include "core/region.h"

#include <QVector4D>

namespace KDecoration3
{
class BorderRadius;
}

namespace KWin
{

class KWIN_EXPORT BorderRadius
{
public:
    BorderRadius();
    explicit BorderRadius(qreal radius);
    explicit BorderRadius(qreal topLeft, qreal topRight, qreal bottomRight, qreal bottomLeft);

    bool operator<=>(const BorderRadius &other) const = default;

    bool isNull() const;

    qreal topLeft() const;
    qreal bottomLeft() const;
    qreal topRight() const;
    qreal bottomRight() const;

    bool clips(const RectF &rect, const RectF &bounds) const;
    Region clip(const Region &region, const RectF &bounds) const;

    BorderRadius scaled(qreal scale) const;
    BorderRadius rounded() const;

    QVector4D toVector() const;

    static BorderRadius from(const KDecoration3::BorderRadius &radius);

private:
    qreal m_topLeft = 0;
    qreal m_topRight = 0;
    qreal m_bottomRight = 0;
    qreal m_bottomLeft = 0;
};

} // namespace KWin
