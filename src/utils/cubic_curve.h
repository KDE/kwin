/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2024 Joshua Goins <joshua.goins@kdab.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "kwin_export.h"

#include <QPointF>

namespace KWin
{

/**
 * @brief Calculates a Beizer cubic curve.
 *
 * This curve is defined with four control points, two of which are controllable.
 * The first and last points are at (0.0, 0.0) and (1.0, 1.0).
 */
class KWIN_EXPORT CubicCurve
{
public:
    /**
     * @brief Creates a curve with two default points, one at (0,0) and another at (1,1).
     */
    explicit CubicCurve();

    /**
     * @brief Creates a curve from @p controlPoint1 and @p controlPoint2.
     */
    explicit CubicCurve(const QPointF &controlPoint1, const QPointF &controlPoint2);

    bool operator==(const CubicCurve &curve) const;

    /**
     * @return Whether this is a default - or straight curve. This means X is always equal and linear to Y.
     */
    bool isDefaultCurve() const;

    /**
     * @brief Calculates the Y value of the point at time @p T.
     *
     * This function is very fast under most circumstances where @p T is sitting on a low
     * slope where it uses Newton's method to approximate the curve.
     *
     * @note T must be in the interval [0, 1].
     */
    qreal value(qreal t) const;

    /**
     * @brief Serializes the control points of this curve to a string.
     */
    QString toString() const;

    /**
     * @brief Creates a curve that's deserialized from @p curveString.
     * @note This serialized string can be created by CubicCurve::toString()
     */
    static CubicCurve fromString(const QString &curveString);

private:
    void calculateCoefficients();

    qreal sampleCurveX(qreal t) const;
    qreal sampleCurveDerivativeX(qreal t) const;
    qreal sampleCurveY(qreal t) const;
    qreal findSolution(qreal t) const;

    QPointF m_controlPoint1;
    QPointF m_controlPoint2;

    // Coefficients used in sampling
    qreal m_aX = 0.0;
    qreal m_bX = 0.0;
    qreal m_cX = 0.0;
    qreal m_aY = 0.0;
    qreal m_bY = 0.0;
    qreal m_cY = 0.0;
};

} // namespace KWin