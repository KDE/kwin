/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright 2017 Roman Gilg <subdiff@gmail.com>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*********************************************************************/
#ifndef KWIN_COLORCORRECT_MANAGER_H
#define KWIN_COLORCORRECT_MANAGER_H

#include "constants.h"
#include <kwin_export.h>

#include <QObject>
#include <QPair>
#include <QDateTime>

class QTimer;

namespace KWin
{

class Platform;

namespace ColorCorrect
{

typedef QPair<QDateTime,QDateTime> DateTimes;
typedef QPair<QTime,QTime> Times;

class ColorCorrectDBusInterface;


enum NightColorMode {
    // timings are based on provided location data
    Automatic = 0,
    // timings are based on fixed location data
    Location,
    // fixed timings
    Timings
};

class KWIN_EXPORT Manager : public QObject
{
    Q_OBJECT

public:
    Manager(QObject *parent);
    void init();

    /**
     * Get current configuration
     * @see changeConfiguration
     * @since 5.12
     **/
    QHash<QString, QVariant> info() const;
    /**
     * Change configuration
     * @see info
     * @since 5.12
     **/
    bool changeConfiguration(QHash<QString, QVariant> data);
    void autoLocationUpdate(double latitude, double longitude);

    // for auto tests
    void reparseConfigAndReset();

public Q_SLOTS:
    void resetSlowUpdateStartTimer();
    void quickAdjust();

Q_SIGNALS:
    void configChange(QHash<QString, QVariant> data);

private:
    void readConfig();
    void hardReset();
    void slowUpdate(int targetTemp);
    void resetAllTimers();
    int currentTargetTemp() const;
    void cancelAllTimers();
    /**
     * Quick shift on manual change to current target Temperature
     **/
    void resetQuickAdjustTimer();
    /**
     * Slow shift to daytime target Temperature
     **/
    void resetSlowUpdateTimer();

    void updateSunTimings(bool force);
    DateTimes getSunTimings(QDate date, double latitude, double longitude, bool morning) const;
    bool checkAutomaticSunTimings() const;
    bool daylight() const;

    void commitGammaRamps(int temperature);

    ColorCorrectDBusInterface *m_iface;

    bool m_active;
    bool m_running = false;

    NightColorMode m_mode = NightColorMode::Automatic;

    // the previous and next sunrise/sunset intervals - in UTC time
    DateTimes m_prev = DateTimes();
    DateTimes m_next = DateTimes();

    // manual times from config
    QTime m_morning = QTime(6,0);
    QTime m_evening = QTime(18,0);
    int m_trTime = 30; // saved in minutes > 1

    // auto location provided by work space
    double m_latAuto;
    double m_lngAuto;
    // manual location from config
    double m_latFixed;
    double m_lngFixed;

    QTimer *m_slowUpdateStartTimer = nullptr;
    QTimer *m_slowUpdateTimer = nullptr;
    QTimer *m_quickAdjustTimer = nullptr;

    int m_currentTemp = NEUTRAL_TEMPERATURE;
    int m_dayTargetTemp = NEUTRAL_TEMPERATURE;
    int m_nightTargetTemp = DEFAULT_NIGHT_TEMPERATURE;

    int m_failedCommitAttempts = 0;
};

}
}

#endif // KWIN_COLORCORRECT_MANAGER_H
