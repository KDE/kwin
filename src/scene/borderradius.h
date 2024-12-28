/*
    SPDX-FileCopyrightText: 2024 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#pragma once

#include "kwin_export.h"

#include <QRegion>
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

    QRegion clip(const QRegion &region, const QRectF &bounds) const;

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
