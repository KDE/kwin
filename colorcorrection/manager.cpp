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
#include "manager.h"
#include "colorcorrectdbusinterface.h"
#include "suncalc.h"
#include "gammaramp.h"
#include <colorcorrect_logging.h>

#include <main.h>
#include <platform.h>
#include <screens.h>
#include <workspace.h>
#include <logind.h>

#include <colorcorrect_settings.h>

#include <QTimer>
#include <QDBusConnection>
#include <QSocketNotifier>

#ifdef Q_OS_LINUX
#include <sys/timerfd.h>
#endif
#include <unistd.h>
#include <fcntl.h>

namespace KWin {
namespace ColorCorrect {

static const int QUICK_ADJUST_DURATION = 2000;
static const int TEMPERATURE_STEP = 50;

static bool checkLocation(double lat, double lng)
{
    return -90 <= lat && lat <= 90 && -180 <= lng && lng <= 180;
}

Manager::Manager(QObject *parent)
    : QObject(parent)
{
    m_iface = new ColorCorrectDBusInterface(this);
    connect(kwinApp(), &Application::workspaceCreated, this, &Manager::init);
}

void Manager::init()
{
    Settings::instance(kwinApp()->config());
    // we may always read in the current config
    readConfig();

    if (!kwinApp()->platform()->supportsGammaControl()) {
        // at least update the sun timings to make the values accessible via dbus
        updateSunTimings(true);
        return;
    }

    connect(LogindIntegration::self(), &LogindIntegration::sessionActiveChanged, this,
            [this](bool active) {
                if (active) {
                    hardReset();
                } else {
                    cancelAllTimers();
                }
            }
    );

#ifdef Q_OS_LINUX
    // monitor for system clock changes - from the time dataengine
    auto timeChangedFd = ::timerfd_create(CLOCK_REALTIME, O_CLOEXEC | O_NONBLOCK);
    ::itimerspec timespec;
    //set all timers to 0, which creates a timer that won't do anything
    ::memset(&timespec, 0, sizeof(timespec));

    // Monitor for the time changing (flags == TFD_TIMER_ABSTIME | TFD_TIMER_CANCEL_ON_SET).
    // However these are not exposed in glibc so value is hardcoded:
    ::timerfd_settime(timeChangedFd, 3, &timespec, 0);

    connect(this, &QObject::destroyed, [timeChangedFd]() {
        ::close(timeChangedFd);
    });

    auto notifier = new QSocketNotifier(timeChangedFd, QSocketNotifier::Read, this);
    connect(notifier, &QSocketNotifier::activated, this, [this](int fd) {
        uint64_t c;
        ::read(fd, &c, 8);

        // check if we're resuming from suspend - in this case do a hard reset
        // Note: We're using the time clock to detect a suspend phase instead of connecting to the
        //       provided logind dbus signal, because this signal would be received way too late.
        QDBusMessage message = QDBusMessage::createMethodCall("org.freedesktop.login1",
                                                              "/org/freedesktop/login1",
                                                              "org.freedesktop.DBus.Properties",
                                                              QStringLiteral("Get"));
        message.setArguments(QVariantList({"org.freedesktop.login1.Manager", QStringLiteral("PreparingForSleep")}));
        QDBusReply<QVariant> reply = QDBusConnection::systemBus().call(message);
        bool comingFromSuspend;
        if (reply.isValid()) {
            comingFromSuspend = reply.value().toBool();
        } else {
            qCDebug(KWIN_COLORCORRECTION) << "Failed to get PreparingForSleep Property of logind session:" << reply.error().message();
            // Always do a hard reset in case we have no further information.
            comingFromSuspend = true;
        }

        if (comingFromSuspend) {
            hardReset();
        } else {
            resetAllTimers();
        }
    });
#else
    // TODO: Alternative method for BSD.
#endif

    hardReset();
}

void Manager::hardReset()
{
    cancelAllTimers();
    updateSunTimings(true);
    if (kwinApp()->platform()->supportsGammaControl() && m_active) {
        m_running = true;
        commitGammaRamps(currentTargetTemp());
    }
    resetAllTimers();
}

void Manager::reparseConfigAndReset()
{
    cancelAllTimers();
    readConfig();
    hardReset();
}

void Manager::readConfig()
{
    Settings *s = Settings::self();
    s->load();

    m_active = s->active();

    NightColorMode mode = s->mode();
    if (mode == NightColorMode::Location || mode == NightColorMode::Timings) {
        m_mode = mode;
    } else {
        // also fallback for invalid setting values
        m_mode = NightColorMode::Automatic;
    }

    m_nightTargetTemp = qBound(MIN_TEMPERATURE, s->nightTemperature(), NEUTRAL_TEMPERATURE);

    double lat, lng;
    auto correctReadin = [&lat, &lng]() {
        if (!checkLocation(lat, lng)) {
            // out of domain
            lat = 0;
            lng = 0;
        }
    };
    // automatic
    lat = s->latitudeAuto();
    lng = s->longitudeAuto();
    correctReadin();
    m_latAuto = lat;
    m_lngAuto = lng;
    // fixed location
    lat = s->latitudeFixed();
    lng = s->longitudeFixed();
    correctReadin();
    m_latFixed = lat;
    m_lngFixed = lng;

    // fixed timings
    QTime mrB = QTime::fromString(s->morningBeginFixed(), "hhmm");
    QTime evB = QTime::fromString(s->eveningBeginFixed(), "hhmm");

    int diffME = mrB.msecsTo(evB);
    if (diffME <= 0) {
        // morning not strictly before evening - use defaults
        mrB = QTime(6,0);
        evB = QTime(18,0);
        diffME = mrB.msecsTo(evB);
    }
    int diffMin = qMin(diffME, MSC_DAY - diffME);

    int trTime = s->transitionTime() * 1000 * 60;
    if (trTime < 0 || diffMin <= trTime) {
        // transition time too long - use defaults
        mrB = QTime(6,0);
        evB = QTime(18,0);
        trTime = FALLBACK_SLOW_UPDATE_TIME;
    }
    m_morning = mrB;
    m_evening = evB;
    m_trTime = qMax(trTime / 1000 / 60, 1);
}

void Manager::resetAllTimers()
{
    cancelAllTimers();
    if (kwinApp()->platform()->supportsGammaControl()) {
        if (m_active) {
            m_running = true;
        }
        // we do this also for active being false in order to reset the temperature back to the day value
        resetQuickAdjustTimer();
    } else {
        m_running = false;
    }
}

void Manager::cancelAllTimers()
{
    delete m_slowUpdateStartTimer;
    delete m_slowUpdateTimer;
    delete m_quickAdjustTimer;

    m_slowUpdateStartTimer = nullptr;
    m_slowUpdateTimer = nullptr;
    m_quickAdjustTimer = nullptr;
}

void Manager::resetQuickAdjustTimer()
{
    updateSunTimings(false);

    int tempDiff = qAbs(currentTargetTemp() - m_currentTemp);
    // allow tolerance of one TEMPERATURE_STEP to compensate if a slow update is coincidental
    if (tempDiff > TEMPERATURE_STEP) {
        cancelAllTimers();
        m_quickAdjustTimer = new QTimer(this);
        m_quickAdjustTimer->setSingleShot(false);
        connect(m_quickAdjustTimer, &QTimer::timeout, this, &Manager::quickAdjust);

        int interval = QUICK_ADJUST_DURATION / (tempDiff / TEMPERATURE_STEP);
        if (interval == 0) {
            interval = 1;
        }
        m_quickAdjustTimer->start(interval);
    } else {
        resetSlowUpdateStartTimer();
    }
}

void Manager::quickAdjust()
{
    if (!m_quickAdjustTimer) {
        return;
    }

    int nextTemp;
    int targetTemp = currentTargetTemp();

    if (m_currentTemp < targetTemp) {
        nextTemp = qMin(m_currentTemp + TEMPERATURE_STEP, targetTemp);
    } else {
        nextTemp = qMax(m_currentTemp - TEMPERATURE_STEP, targetTemp);
    }
    commitGammaRamps(nextTemp);

    if (nextTemp == targetTemp) {
        // stop timer, we reached the target temp
        delete m_quickAdjustTimer;
        m_quickAdjustTimer = nullptr;
        resetSlowUpdateStartTimer();
    }
}

void Manager::resetSlowUpdateStartTimer()
{
    delete m_slowUpdateStartTimer;
    m_slowUpdateStartTimer = nullptr;

    if (!m_running || m_quickAdjustTimer) {
        // only reenable the slow update start timer when quick adjust is not active anymore
        return;
    }

    // set up the next slow update
    m_slowUpdateStartTimer = new QTimer(this);
    m_slowUpdateStartTimer->setSingleShot(true);
    connect(m_slowUpdateStartTimer, &QTimer::timeout, this, &Manager::resetSlowUpdateStartTimer);

    updateSunTimings(false);
    int diff;
    if (m_mode == NightColorMode::Timings) {
        // Timings mode is in local time
        diff = QDateTime::currentDateTime().msecsTo(m_next.first);
    } else {
        diff = QDateTime::currentDateTimeUtc().msecsTo(m_next.first);
    }
    if (diff <= 0) {
        qCCritical(KWIN_COLORCORRECTION) << "Error in time calculation. Deactivating Night Color.";
        return;
    }
    m_slowUpdateStartTimer->start(diff);

    // start the current slow update
    resetSlowUpdateTimer();
}

void Manager::resetSlowUpdateTimer()
{
    delete m_slowUpdateTimer;
    m_slowUpdateTimer = nullptr;

    QDateTime now = QDateTime::currentDateTimeUtc();
    bool isDay = daylight();
    int targetTemp = isDay ? m_dayTargetTemp : m_nightTargetTemp;

    if (m_prev.first == m_prev.second) {
        // transition time is zero
        commitGammaRamps(isDay ? m_dayTargetTemp : m_nightTargetTemp);
        return;
    }

    if (m_prev.first <= now && now <= m_prev.second) {
        int availTime = now.msecsTo(m_prev.second);
        m_slowUpdateTimer = new QTimer(this);
        m_slowUpdateTimer->setSingleShot(false);
        if (isDay) {
            connect(m_slowUpdateTimer, &QTimer::timeout, this, [this]() {slowUpdate(m_dayTargetTemp);});
        } else {
            connect(m_slowUpdateTimer, &QTimer::timeout, this, [this]() {slowUpdate(m_nightTargetTemp);});
        }

        // calculate interval such as temperature is changed by TEMPERATURE_STEP K per timer timeout
        int interval = availTime / (qAbs(targetTemp - m_currentTemp) / TEMPERATURE_STEP);
        if (interval == 0) {
            interval = 1;
        }
        m_slowUpdateTimer->start(interval);
    }
}

void Manager::slowUpdate(int targetTemp)
{
    if (!m_slowUpdateTimer) {
        return;
    }
    int nextTemp;
    if (m_currentTemp < targetTemp) {
        nextTemp = qMin(m_currentTemp + TEMPERATURE_STEP, targetTemp);
    } else {
        nextTemp = qMax(m_currentTemp - TEMPERATURE_STEP, targetTemp);
    }
    commitGammaRamps(nextTemp);
    if (nextTemp == targetTemp) {
        // stop timer, we reached the target temp
        delete m_slowUpdateTimer;
        m_slowUpdateTimer = nullptr;
    }
}

void Manager::updateSunTimings(bool force)
{
    QDateTime todayNow = QDateTime::currentDateTimeUtc();

    if (m_mode == NightColorMode::Timings) {

        QDateTime todayNowLocal = QDateTime::currentDateTime();

        QDateTime morB = QDateTime(todayNowLocal.date(), m_morning);
        QDateTime morE = morB.addSecs(m_trTime * 60);
        QDateTime eveB = QDateTime(todayNowLocal.date(), m_evening);
        QDateTime eveE = eveB.addSecs(m_trTime * 60);

        if (morB <= todayNowLocal && todayNowLocal < eveB) {
            m_next = DateTimes(eveB, eveE);
            m_prev = DateTimes(morB, morE);
        } else if (todayNowLocal < morB) {
            m_next = DateTimes(morB, morE);
            m_prev = DateTimes(eveB.addDays(-1), eveE.addDays(-1));
        } else {
            m_next = DateTimes(morB.addDays(1), morE.addDays(1));
            m_prev = DateTimes(eveB, eveE);
        }
        return;
    }

    double lat, lng;
    if (m_mode == NightColorMode::Automatic) {
        lat = m_latAuto;
        lng = m_lngAuto;
    } else {
        lat = m_latFixed;
        lng = m_lngFixed;
    }

    if (!force) {
        // first try by only switching the timings
        if (daylight()) {
            // next is morning
            m_prev = m_next;
            m_next = getSunTimings(todayNow.date().addDays(1), lat, lng, true);
        } else {
            // next is evening
            m_prev = m_next;
            m_next = getSunTimings(todayNow.date(), lat, lng, false);
        }
    }

    if (force || !checkAutomaticSunTimings()) {
        // in case this fails, reset them
        DateTimes morning = getSunTimings(todayNow.date(), lat, lng, true);
        if (todayNow < morning.first) {
            m_prev = getSunTimings(todayNow.date().addDays(-1), lat, lng, false);
            m_next = morning;
        } else {
            DateTimes evening = getSunTimings(todayNow.date(), lat, lng, false);
            if (todayNow < evening.first) {
                m_prev = morning;
                m_next = evening;
            } else {
                m_prev = evening;
                m_next = getSunTimings(todayNow.date().addDays(1), lat, lng, true);
            }
        }
    }
}

DateTimes Manager::getSunTimings(QDate date, double latitude, double longitude, bool morning) const
{
    Times times = calculateSunTimings(date, latitude, longitude, morning);
    // At locations near the poles it is possible, that we can't
    // calculate some or all sun timings (midnight sun).
    // In this case try to fallback to sensible default values.
    bool beginDefined = !times.first.isNull();
    bool endDefined = !times.second.isNull();
    if (!beginDefined || !endDefined) {
        if (beginDefined) {
            times.second = times.first.addMSecs( FALLBACK_SLOW_UPDATE_TIME );
        } else if (endDefined) {
            times.first = times.second.addMSecs( - FALLBACK_SLOW_UPDATE_TIME);
        } else {
            // Just use default values for morning and evening, but the user
            // will probably deactivate Night Color anyway if he is living
            // in a region without clear sun rise and set.
            times.first = morning ? QTime(6,0,0) : QTime(18,0,0);
            times.second = times.first.addMSecs( FALLBACK_SLOW_UPDATE_TIME );
        }
    }
    return DateTimes(QDateTime(date, times.first, Qt::UTC), QDateTime(date, times.second, Qt::UTC));
}

bool Manager::checkAutomaticSunTimings() const
{
    if (m_prev.first.isValid() && m_prev.second.isValid() &&
            m_next.first.isValid() && m_next.second.isValid()) {
        QDateTime todayNow = QDateTime::currentDateTimeUtc();
        return m_prev.first <= todayNow && todayNow < m_next.first &&
                m_prev.first.msecsTo(m_next.first) < MSC_DAY * 23./24;
    }
    return false;
}

bool Manager::daylight() const
{
    return m_prev.first.date() == m_next.first.date();
}

int Manager::currentTargetTemp() const
{
    if (!m_active) {
        return NEUTRAL_TEMPERATURE;
    }

    QDateTime todayNow = QDateTime::currentDateTimeUtc();

    auto f = [this, todayNow](int target1, int target2) {
        if (todayNow <= m_prev.second) {
            double residueQuota = todayNow.msecsTo(m_prev.second) / (double)m_prev.first.msecsTo(m_prev.second);

            double ret = (int)((1. - residueQuota) * (double)target2 + residueQuota * (double)target1);
            // remove single digits
            ret = ((int)(0.1 * ret)) * 10;
            return (int)ret;
        } else {
            return target2;
        }
    };

    if (daylight()) {
        return f(m_nightTargetTemp, m_dayTargetTemp);
    } else {
        return f(m_dayTargetTemp, m_nightTargetTemp);
    }
}

void Manager::commitGammaRamps(int temperature)
{
    int nscreens = Screens::self()->count();

    for (int screen = 0; screen < nscreens; screen++) {
        int rampsize = kwinApp()->platform()->gammaRampSize(screen);
        GammaRamp ramp(rampsize);

        /*
         * The gamma calculation below is based on the Redshift app:
         * https://github.com/jonls/redshift
         */

        // linear default state
        for (int i = 0; i < rampsize; i++) {
                uint16_t value = (double)i / rampsize * (UINT16_MAX + 1);
                ramp.red[i] = value;
                ramp.green[i] = value;
                ramp.blue[i] = value;
        }

        // approximate white point
        float whitePoint[3];
        float alpha = (temperature % 100) / 100.;
        int bbCIndex = ((temperature - 1000) / 100) * 3;
        whitePoint[0] = (1. - alpha) * blackbodyColor[bbCIndex] + alpha * blackbodyColor[bbCIndex + 3];
        whitePoint[1] = (1. - alpha) * blackbodyColor[bbCIndex + 1] + alpha * blackbodyColor[bbCIndex + 4];
        whitePoint[2] = (1. - alpha) * blackbodyColor[bbCIndex + 2] + alpha * blackbodyColor[bbCIndex + 5];

        for (int i = 0; i < rampsize; i++) {
            ramp.red[i] = (double)ramp.red[i] / (UINT16_MAX+1) * whitePoint[0] * (UINT16_MAX+1);
            ramp.green[i] = (double)ramp.green[i] / (UINT16_MAX+1) * whitePoint[1] * (UINT16_MAX+1);
            ramp.blue[i] = (double)ramp.blue[i] / (UINT16_MAX+1) * whitePoint[2] * (UINT16_MAX+1);
        }

        if (kwinApp()->platform()->setGammaRamp(screen, ramp)) {
            m_currentTemp = temperature;
            m_failedCommitAttempts = 0;
        } else {
            m_failedCommitAttempts++;
            if (m_failedCommitAttempts < 10) {
                qCWarning(KWIN_COLORCORRECTION).nospace() << "Committing Gamma Ramp failed for screen " << screen <<
                         ". Trying " << (10 - m_failedCommitAttempts) << " times more.";
            } else {
                // TODO: On multi monitor setups we could try to rollback earlier changes for already commited outputs
                qCWarning(KWIN_COLORCORRECTION) << "Gamma Ramp commit failed too often. Deactivating color correction for now.";
                m_failedCommitAttempts = 0; // reset so we can try again later (i.e. after suspend phase or config change)
                m_running = false;
                cancelAllTimers();
            }
        }
    }
}

QHash<QString, QVariant> Manager::info() const
{
    return QHash<QString, QVariant> {
        { QStringLiteral("Available"), kwinApp()->platform()->supportsGammaControl() },

        { QStringLiteral("ActiveEnabled"), true},
        { QStringLiteral("Active"), m_active},

        { QStringLiteral("ModeEnabled"), true},
        { QStringLiteral("Mode"), (int)m_mode},

        { QStringLiteral("NightTemperatureEnabled"), true},
        { QStringLiteral("NightTemperature"), m_nightTargetTemp},

        { QStringLiteral("Running"), m_running},
        { QStringLiteral("CurrentColorTemperature"), m_currentTemp},

        { QStringLiteral("LatitudeAuto"), m_latAuto},
        { QStringLiteral("LongitudeAuto"), m_lngAuto},

        { QStringLiteral("LocationEnabled"), true},
        { QStringLiteral("LatitudeFixed"), m_latFixed},
        { QStringLiteral("LongitudeFixed"), m_lngFixed},

        { QStringLiteral("TimingsEnabled"), true},
        { QStringLiteral("MorningBeginFixed"), m_morning.toString(Qt::ISODate)},
        { QStringLiteral("EveningBeginFixed"), m_evening.toString(Qt::ISODate)},
        { QStringLiteral("TransitionTime"), m_trTime},
    };
}

bool Manager::changeConfiguration(QHash<QString, QVariant> data)
{
    bool activeUpdate, modeUpdate, tempUpdate, locUpdate, timeUpdate;
    activeUpdate = modeUpdate = tempUpdate = locUpdate = timeUpdate = false;

    bool active = m_active;
    NightColorMode mode = m_mode;
    int nightT = m_nightTargetTemp;

    double lat = m_latFixed;
    double lng = m_lngFixed;

    QTime mor = m_morning;
    QTime eve = m_evening;
    int trT = m_trTime;

    QHash<QString, QVariant>::const_iterator iter1, iter2, iter3;

    iter1 = data.constFind("Active");
    if (iter1 != data.constEnd()) {
        if (!iter1.value().canConvert<bool>()) {
            return false;
        }
        bool act = iter1.value().toBool();
        activeUpdate = m_active != act;
        active = act;
    }

    iter1 = data.constFind("Mode");
    if (iter1 != data.constEnd()) {
        if (!iter1.value().canConvert<int>()) {
            return false;
        }
        int mo = iter1.value().toInt();
        if (mo < 0 || 2 < mo) {
            return false;
        }
        NightColorMode moM;
        switch (mo) {
            case 0:
                moM = NightColorMode::Automatic;
                break;
            case 1:
                moM = NightColorMode::Location;
                break;
            case 2:
                moM = NightColorMode::Timings;
        }
        modeUpdate = m_mode != moM;
        mode = moM;
    }

    iter1 = data.constFind("NightTemperature");
    if (iter1 != data.constEnd()) {
        if (!iter1.value().canConvert<int>()) {
            return false;
        }
        int nT = iter1.value().toInt();
        if (nT < MIN_TEMPERATURE || NEUTRAL_TEMPERATURE < nT) {
            return false;
        }
        tempUpdate = m_nightTargetTemp != nT;
        nightT = nT;
    }

    iter1 = data.constFind("LatitudeFixed");
    iter2 = data.constFind("LongitudeFixed");
    if (iter1 != data.constEnd() && iter2 != data.constEnd()) {
        if (!iter1.value().canConvert<double>() || !iter2.value().canConvert<double>()) {
            return false;
        }
        double la = iter1.value().toDouble();
        double ln = iter2.value().toDouble();
        if (!checkLocation(la, ln)) {
            return false;
        }
        locUpdate = m_latFixed != la || m_lngFixed != ln;
        lat = la;
        lng = ln;
    }

    iter1 = data.constFind("MorningBeginFixed");
    iter2 = data.constFind("EveningBeginFixed");
    iter3 = data.constFind("TransitionTime");
    if (iter1 != data.constEnd() && iter2 != data.constEnd() && iter3 != data.constEnd()) {
        if (!iter1.value().canConvert<QString>() || !iter2.value().canConvert<QString>() || !iter3.value().canConvert<int>()) {
            return false;
        }
        QTime mo = QTime::fromString(iter1.value().toString(), Qt::ISODate);
        QTime ev = QTime::fromString(iter2.value().toString(), Qt::ISODate);
        if (!mo.isValid() || !ev.isValid()) {
            return false;
        }
        int tT = iter3.value().toInt();

        int diffME = mo.msecsTo(ev);
        if (diffME <= 0 || qMin(diffME, MSC_DAY - diffME) <= tT * 60 * 1000 || tT < 1) {
            // morning not strictly before evening, transition time too long or transition time out of bounds
            return false;
        }

        timeUpdate = m_morning != mo || m_evening != ev || m_trTime != tT;
        mor = mo;
        eve = ev;
        trT = tT;
    }

    if (!(activeUpdate || modeUpdate || tempUpdate || locUpdate || timeUpdate)) {
        return true;
    }

    bool resetNeeded = activeUpdate || modeUpdate || tempUpdate ||
            (locUpdate && mode == NightColorMode::Location) ||
            (timeUpdate && mode == NightColorMode::Timings);

    if (resetNeeded) {
        cancelAllTimers();
    }

    Settings *s = Settings::self();
    if (activeUpdate) {
        m_active = active;
        s->setActive(active);
    }

    if (modeUpdate) {
        m_mode = mode;
        s->setMode(mode);
    }

    if (tempUpdate) {
        m_nightTargetTemp = nightT;
        s->setNightTemperature(nightT);
    }

    if (locUpdate) {
        m_latFixed = lat;
        m_lngFixed = lng;
        s->setLatitudeFixed(lat);
        s->setLongitudeFixed(lng);
    }

    if (timeUpdate) {
        m_morning = mor;
        m_evening = eve;
        m_trTime = trT;
        s->setMorningBeginFixed(mor.toString("hhmm"));
        s->setEveningBeginFixed(eve.toString("hhmm"));
        s->setTransitionTime(trT);
    }
    s->save();

    if (resetNeeded) {
        resetAllTimers();
    }
    emit configChange(info());
    return true;
}

void Manager::autoLocationUpdate(double latitude, double longitude)
{
    if (!checkLocation(latitude, longitude)) {
        return;
    }

    // we tolerate small deviations with minimal impact on sun timings
    if (qAbs(m_latAuto - latitude) < 2 && qAbs(m_lngAuto - longitude) < 1) {
        return;
    }
    cancelAllTimers();
    m_latAuto = latitude;
    m_lngAuto = longitude;

    Settings *s = Settings::self();
    s->setLatitudeAuto(latitude);
    s->setLongitudeAuto(longitude);
    s->save();

    resetAllTimers();
    emit configChange(info());
}

}
}
