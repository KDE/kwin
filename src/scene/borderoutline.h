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
    BorderOutline(qreal thickness = 0, const QColor &color = QColor(), const BorderRadius &radius = BorderRadius());

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

inline BorderOutline::BorderOutline(qreal thickness, const QColor &color, const BorderRadius &radius)
    : m_thickness(thickness)
    , m_color(color)
    , m_radius(radius)
{
}

inline bool BorderOutline::isNull() const
{
    return qFuzzyIsNull(m_thickness);
}

inline qreal BorderOutline::thickness() const
{
    return m_thickness;
}

inline QColor BorderOutline::color() const
{
    return m_color;
}

inline BorderRadius BorderOutline::radius() const
{
    return m_radius;
}

inline BorderOutline BorderOutline::scaled(qreal scale) const
{
    return BorderOutline(m_thickness * scale, m_color, m_radius.scaled(scale));
}

inline BorderOutline BorderOutline::rounded() const
{
    return BorderOutline(std::round(m_thickness), m_color, m_radius.rounded());
}

inline QRectF BorderOutline::inflate(const QRectF &rect) const
{
    return rect.adjusted(-m_thickness, -m_thickness, m_thickness, m_thickness);
}

inline QRectF BorderOutline::deflate(const QRectF &rect) const
{
    return rect.adjusted(m_thickness, m_thickness, -m_thickness, -m_thickness);
}

} // namespace KWin
