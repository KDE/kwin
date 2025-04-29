/*
    SPDX-FileCopyrightText: 2025 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#pragma once

#include <QDateTime>

namespace KWin
{

/**
 * The SunTransit type provides a way to compute the timings of sunset and sunrise. The event timings
 * are provided relative to the closest solar noon for the specified location and time.
 *
 * The computed transit timings are not suitable for the scientific purposes, but they are good enough
 * for other things where accuracy is not critical.
 */
class SunTransit
{
    qreal m_latitude;
    qreal m_longitude;
    qreal m_solarNoon;

public:
    /**
     * The Event type is used to describe solar transit periods.
     */
    enum Event {
        CivilDawn, /// Transitioning from nighttime to daylight, the Sun is 6 degrees below the horizon
        Sunrise, /// Transitioning from nighttime to daylight, the Sun is 0.833 degrees below the horizon
        Sunset, /// Transitioning from daylight to nighttime, the Sun is 0.833 degrees below the horizon
        CivilDusk, /// Transitioning from daylight to nighttime, the Sun is 6 degrees below the horizon
    };

    SunTransit(const QDateTime &dateTime, qreal latitude, qreal longitude);

    /**
     * Returns the date and the time when the Sun reaches the specified position. If the date and time
     * cannot be determined, an invalid QDateTime object will be returned.
     */
    QDateTime dateTime(Event event) const;
};

} // namespace KWin
