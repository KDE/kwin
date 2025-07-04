/*
    SPDX-FileCopyrightText: 2024 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#pragma once

#include "scene/borderradius.h"

#include <QColor>

namespace KDecoration3
{
class BorderOutline;
}

namespace KWin
{

class KWIN_EXPORT BorderOutline
{
public:
    explicit BorderOutline(qreal thickness = 0, const QColor &color = QColor(), const BorderRadius &radius = BorderRadius());

    bool operator==(const BorderOutline &other) const = default;
    bool operator!=(const BorderOutline &other) const = default;

    bool isNull() const;

    qreal thickness() const;
    QColor color() const;
    BorderRadius radius() const;

    BorderOutline scaled(qreal scale) const;
    BorderOutline rounded() const;

    QRectF inflate(const QRectF &rect) const;
    QRectF deflate(const QRectF &rect) const;

    static BorderOutline from(const KDecoration3::BorderOutline &outline);

private:
    qreal m_thickness;
    QColor m_color;
    BorderRadius m_radius;
};

} // namespace KWin
