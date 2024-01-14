/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2024 Joshua Goins <joshua.goins@kdab.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "cubic_curve.h"

#include <QStringList>

namespace KWin
{

constexpr qreal EPSILON = 1e-7;
constexpr int NUM_ITERATIONS = 4;

CubicCurve::CubicCurve()
    : m_controlPoint1(0.0, 0.0)
    , m_controlPoint2(1.0, 1.0)
{
    calculateCoefficients();
}

CubicCurve::CubicCurve(const QPointF &controlPoint1, const QPointF &controlPoint2)
    : m_controlPoint1(controlPoint1)
    , m_controlPoint2(controlPoint2)
{
    calculateCoefficients();
}

bool CubicCurve::operator==(const CubicCurve &curve) const
{
    return m_controlPoint1 == curve.m_controlPoint1 && m_controlPoint2 == curve.m_controlPoint2;
}

bool CubicCurve::isDefaultCurve() const
{
    return m_controlPoint1 == QPointF(0.0, 0.0) && m_controlPoint2 == QPointF(1.0, 1.0);
}

qreal CubicCurve::value(const qreal t) const
{
    Q_ASSERT(t >= 0.0);
    Q_ASSERT(t <= 1.0);

    // If we are a simple curve (0,0) -> (1,1) where X always equals Y, let's make this easy.
    if (isDefaultCurve()) {
        return t;
    }

    return qBound(0.0, sampleCurveY(findSolution(t)), 1.0);
}

QString CubicCurve::toString() const
{
    QString sCurve;
    for (const QPointF &pair : {m_controlPoint1, m_controlPoint2}) {
        sCurve += QString::number(pair.x());
        sCurve += ',';
        sCurve += QString::number(pair.y());
        sCurve += ';';
    }

    return sCurve;
}

CubicCurve CubicCurve::fromString(const QString &curveString)
{
    if (curveString.isEmpty()) {
        return CubicCurve();
    }

    const QStringList data = curveString.split(';');

    QList<QPointF> points;
    for (const QString &pair : data) {
        if (pair.indexOf(',') > -1) {
            points.append({pair.section(',', 0, 0).toDouble(),
                           pair.section(',', 1, 1).toDouble()});
        }
    }

    // We only support 2 points
    if (points.size() == 2) {
        return CubicCurve(points[0], points[1]);
    }

    return CubicCurve();
}

void CubicCurve::calculateCoefficients()
{
    m_cX = 3.0 * m_controlPoint1.x();
    m_bX = 3.0 * (m_controlPoint2.x() - m_controlPoint1.x()) - m_cX;
    m_aX = 1.0 - m_cX - m_bX;

    m_cY = 3.0 * m_controlPoint1.y();
    m_bY = 3.0 * (m_controlPoint2.y() - m_controlPoint1.y()) - m_cY;
    m_aY = 1.0 - m_cY - m_bY;
}

qreal CubicCurve::sampleCurveX(const qreal t) const
{
    return ((m_aX * t + m_bX) * t + m_cX) * t;
}

qreal CubicCurve::sampleCurveDerivativeX(const qreal t) const
{
    return (3.0 * m_aX * t + 2.0 * m_bX) * t + m_cX;
}

qreal CubicCurve::sampleCurveY(const qreal t) const
{
    return ((m_aY * t + m_bY) * t + m_cY) * t;
}

qreal CubicCurve::findSolution(const qreal t) const
{
    // Solving based on WebKit's UnitBezier: https://github.com/WebKit/WebKit/blob/main/Source/WebCore/platform/graphics/UnitBezier.h
    qreal t2 = t;

    // Use Newton's method first, which works for most of the cruve
    // Normally very quick, depending on the number of iterations.
    for (int i = 0; i < NUM_ITERATIONS; i++) {
        const qreal x2 = sampleCurveX(t2) - t;
        if (abs(x2) < EPSILON) {
            return t2;
        }
        const qreal d2 = sampleCurveDerivativeX(t2);
        if (abs(d2) < EPSILON) {
            return t2;
        }
        t2 = t2 - x2 / d2;
    }

    qreal min = 0.0;
    qreal max = 1.0;
    t2 = t;

    // Bi-sect as a fallback, this typically happens with high slopes.
    while (min < max) {
        const qreal x2 = sampleCurveX(t2);
        if (abs(x2 - t) < EPSILON) {
            return t2;
        }

        if (t > x2) {
            min = t2;
        } else {
            max = t2;
        }

        t2 = (max - min) * 0.5 + min;
    }

    return t2;
}

} // namespace KWin