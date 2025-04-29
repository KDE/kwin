/*
    SPDX-FileCopyrightText: 2025 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#include "plugins/nightlight/suntransit.h"

namespace KWin
{

// Formulas are taken from https://gml.noaa.gov/grad/solcalc/calcdetails.html

static const qreal secondsPerDay = 86400.0;
static const qreal j2000 = 2451545.0;
static const qreal julianUnixEpoch = 2440587.5;
static const qreal julianYear = 365.25;
static const qreal julianCentury = julianYear * 100;

static QDateTime julianDateToDateTime(qreal jd)
{
    return QDateTime::fromSecsSinceEpoch(std::round((jd - julianUnixEpoch) * secondsPerDay));
}

static qreal dateTimeToJulianDate(const QDateTime &dateTime)
{
    return dateTime.toSecsSinceEpoch() / secondsPerDay + julianUnixEpoch;
}

static qreal julianDateToJulianCenturies(qreal jd)
{
    return (jd - j2000) / julianCentury;
}

static qreal sunGeometricMeanLongitude(qreal jcent)
{
    return qDegreesToRadians(fmod(280.46646 + jcent * (36000.76983 + jcent * 0.0003032), 360));
}

static qreal sunGeometricMeanAnomaly(qreal jcent)
{
    return qDegreesToRadians(357.52911 + jcent * (35999.05029 - 0.0001537 * jcent));
}

static qreal earthOrbitEccentricity(qreal jcent)
{
    return 0.016708634 - jcent * (0.000042037 + 0.0000001267 * jcent);
}

static qreal equationOfCenter(qreal jcent)
{
    const qreal anomaly = sunGeometricMeanAnomaly(jcent);
    return qDegreesToRadians(std::sin(anomaly) * (1.914602 - jcent * (0.004817 + 0.000014 * jcent))
                             + std::sin(2 * anomaly) * (0.019993 - 0.000101 * jcent) + std::sin(3 * anomaly) * 0.000289);
}

static qreal sunTrueLongitude(qreal jcent)
{
    return sunGeometricMeanLongitude(jcent) + equationOfCenter(jcent);
}

static qreal sunApparentLongitude(qreal jcent)
{
    return qDegreesToRadians(qRadiansToDegrees(sunTrueLongitude(jcent)) - 0.00569 - 0.00478 * std::sin(qDegreesToRadians(125.04 - 1934.136 * jcent)));
}

static qreal obliquityCorrection(qreal jcent)
{
    const qreal meanEclipticObliquity = 23 + (26 + ((21.448 - jcent * (46.815 + jcent * (0.00059 - jcent * 0.001813)))) / 60) / 60;
    return qDegreesToRadians(meanEclipticObliquity + 0.00256 * std::cos(qDegreesToRadians(125.04 - 1934.136 * jcent)));
}

static qreal sunDeclination(qreal jcent)
{
    return std::asin(std::sin(obliquityCorrection(jcent)) * std::sin(sunApparentLongitude(jcent)));
}

static qreal sunHourAngle(qreal jcent, qreal elevation, qreal latitude)
{
    const qreal declination = sunDeclination(jcent);
    const qreal angle = std::acos(std::cos(elevation) / (std::cos(latitude) * std::cos(declination)) - std::tan(latitude) * std::tan(declination));
    return std::copysign(angle, -elevation);
}

static qreal equationOfTime(qreal jcent)
{
    const qreal eccentricity = earthOrbitEccentricity(jcent);
    const qreal meanLongitude = sunGeometricMeanLongitude(jcent);
    const qreal meanAnomaly = sunGeometricMeanAnomaly(jcent);
    const qreal varY = std::pow(std::tan(obliquityCorrection(jcent) / 2), 2);

    return varY * std::sin(2 * meanLongitude)
        - 2 * eccentricity * std::sin(meanAnomaly)
        + 4 * eccentricity * varY * std::sin(meanAnomaly) * std::cos(2 * meanLongitude)
        - 0.5 * varY * varY * std::sin(4 * meanLongitude)
        - 1.25 * eccentricity * eccentricity * std::sin(2 * meanAnomaly);
}

static qreal timeOfNoon(qreal jd, qreal longitude)
{
    const qreal offset = longitude / (2 * M_PI);
    qreal noon = std::round(jd + offset) - offset;
    noon -= equationOfTime(julianDateToJulianCenturies(noon)) / (2 * M_PI);
    return noon;
}

SunTransit::SunTransit(const QDateTime &dateTime, qreal latitude, qreal longitude)
    : m_latitude(qDegreesToRadians(latitude))
    , m_longitude(qDegreesToRadians(longitude))
    , m_solarNoon(timeOfNoon(dateTimeToJulianDate(dateTime), m_longitude))
{
}

static qreal elevationAngleForEvent(SunTransit::Event event)
{
    switch (event) {
    case SunTransit::CivilDawn:
        return 90 + 6;
    case SunTransit::Sunrise:
        return 90 + 0.833;
    case SunTransit::Sunset:
        return -90 - 0.833;
    case SunTransit::CivilDusk:
        return -90 - 6;
    }

    Q_UNREACHABLE();
}

QDateTime SunTransit::dateTime(Event event) const
{
    const qreal elevationAngle = qDegreesToRadians(elevationAngleForEvent(event));
    const qreal hourAngle = sunHourAngle(julianDateToJulianCenturies(m_solarNoon), elevationAngle, m_latitude) / (2 * M_PI);
    if (std::isnan(hourAngle)) {
        return QDateTime();
    }

    // With extreme hour angles, the timings are less predictable.
    const qreal absoluteHourAngle = std::abs(hourAngle);
    if (absoluteHourAngle < 0.05 || absoluteHourAngle > 0.45) {
        return QDateTime();
    }

    return julianDateToDateTime(m_solarNoon + hourAngle);
}

} // namespace KWin
