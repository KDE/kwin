/*
    SPDX-FileCopyrightText: 2024 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#include "scene/borderoutline.h"

#include <KDecoration3/Decoration>

namespace KWin
{

BorderOutline::BorderOutline(qreal thickness, const QColor &color, const BorderRadius &radius)
    : m_thickness(thickness)
    , m_color(color)
    , m_radius(radius)
{
}

bool BorderOutline::isNull() const
{
    return qFuzzyIsNull(m_thickness);
}

qreal BorderOutline::thickness() const
{
    return m_thickness;
}

QColor BorderOutline::color() const
{
    return m_color;
}

BorderRadius BorderOutline::radius() const
{
    return m_radius;
}

BorderOutline BorderOutline::scaled(qreal scale) const
{
    return BorderOutline(m_thickness * scale, m_color, m_radius.scaled(scale));
}

BorderOutline BorderOutline::rounded() const
{
    return BorderOutline(std::round(m_thickness), m_color, m_radius.rounded());
}

QRectF BorderOutline::inflate(const QRectF &rect) const
{
    return rect.adjusted(-m_thickness, -m_thickness, m_thickness, m_thickness);
}

QRectF BorderOutline::deflate(const QRectF &rect) const
{
    return rect.adjusted(m_thickness, m_thickness, -m_thickness, -m_thickness);
}

BorderOutline BorderOutline::from(const KDecoration3::BorderOutline &outline)
{
    return BorderOutline(outline.thickness(), outline.color(), BorderRadius::from(outline.radius()));
}

} // namespace KWin
