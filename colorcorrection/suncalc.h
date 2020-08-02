/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2017 Roman Gilg <subdiff@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#ifndef KWIN_SUNCALCULATOR_H
#define KWIN_SUNCALCULATOR_H

#include <QDate>
#include <QTime>
#include <QPair>

namespace KWin
{

namespace ColorCorrect
{

/**
 * Calculates for a given location and date two of the
 * following sun timings in their temporal order:
 * - Nautical dawn and sunrise for the morning
 * - Sunset and nautical dusk for the evening
 * @since 5.12
 */

QPair<QDateTime, QDateTime> calculateSunTimings(const QDateTime &dateTime, double latitude, double longitude, bool morning);


}
}

#endif // KWIN_SUNCALCULATOR_H
