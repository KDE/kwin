/*
    SPDX-FileCopyrightText: 2024 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#include "scene/borderradius.h"

#include <KDecoration3/Decoration>

namespace KWin
{

BorderRadius::BorderRadius()
{
}

BorderRadius::BorderRadius(qreal radius)
    : m_topLeft(radius)
    , m_topRight(radius)
    , m_bottomRight(radius)
    , m_bottomLeft(radius)
{
}

BorderRadius::BorderRadius(qreal topLeft, qreal topRight, qreal bottomRight, qreal bottomLeft)
    : m_topLeft(topLeft)
    , m_topRight(topRight)
    , m_bottomRight(bottomRight)
    , m_bottomLeft(bottomLeft)
{
}

bool BorderRadius::isNull() const
{
    return qFuzzyIsNull(m_topLeft) && qFuzzyIsNull(m_topRight) && qFuzzyIsNull(m_bottomRight) && qFuzzyIsNull(m_bottomLeft);
}

qreal BorderRadius::topLeft() const
{
    return m_topLeft;
}

qreal BorderRadius::bottomLeft() const
{
    return m_bottomLeft;
}

qreal BorderRadius::topRight() const
{
    return m_topRight;
}

qreal BorderRadius::bottomRight() const
{
    return m_bottomRight;
}

BorderRadius BorderRadius::scaled(qreal scale) const
{
    return BorderRadius(m_topLeft * scale,
                        m_topRight * scale,
                        m_bottomRight * scale,
                        m_bottomLeft * scale);
}

BorderRadius BorderRadius::rounded() const
{
    return BorderRadius(std::round(m_topLeft),
                        std::round(m_topRight),
                        std::round(m_bottomRight),
                        std::round(m_bottomLeft));
}

QVector4D BorderRadius::toVector() const
{
    return QVector4D(m_topLeft, m_topRight, m_bottomLeft, m_bottomRight);
}

static double pointLength(const QPointF &point)
{
    return std::hypot(point.x(), point.y());
}

bool BorderRadius::clips(const QRectF &rect, const QRectF &bounds) const
{
    if (rect.x() < bounds.x() || rect.y() < bounds.y()) {
        return true;
    }
    if (rect.right() > bounds.right() || rect.bottom() > bounds.bottom()) {
        return true;
    }
    // rect is somewhere within bounds, now check the corners
    return pointLength(rect.topLeft() - bounds.topLeft() - QPointF(m_topLeft, m_topLeft)) > m_topLeft
        || pointLength(rect.topRight() - bounds.topRight() - QPointF(m_topRight, m_topRight)) > m_topRight
        || pointLength(rect.bottomRight() - bounds.bottomRight() - QPointF(m_bottomRight, m_bottomRight)) > m_bottomRight
        || pointLength(rect.bottomLeft() - bounds.bottomLeft() - QPointF(m_bottomLeft, m_bottomLeft)) > m_bottomLeft;
}

QRegion BorderRadius::clip(const QRegion &region, const QRectF &bounds) const
{
    if (region.isEmpty()) {
        return QRegion();
    }

    QRegion clipped = region;

    if (m_topLeft > 0) {
        clipped = clipped.subtracted(QRectF(0, 0, m_topLeft, m_topLeft).toAlignedRect());
    }

    if (m_topRight > 0) {
        clipped = clipped.subtracted(QRectF(bounds.x() + bounds.width() - m_topRight, 0, m_topRight, m_topRight).toAlignedRect());
    }

    if (m_bottomRight > 0) {
        clipped = clipped.subtracted(QRectF(bounds.x() + bounds.width() - m_bottomRight, bounds.y() + bounds.height() - m_bottomRight, m_bottomRight, m_bottomRight).toAlignedRect());
    }

    if (m_bottomLeft > 0) {
        clipped = clipped.subtracted(QRectF(0, bounds.y() + bounds.height() - m_bottomLeft, m_bottomLeft, m_bottomLeft).toAlignedRect());
    }

    return clipped;
}

BorderRadius BorderRadius::from(const KDecoration3::BorderRadius &radius)
{
    return BorderRadius(radius.topLeft(), radius.topRight(), radius.bottomRight(), radius.bottomLeft());
}

} // namespace KWin
