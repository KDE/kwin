/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2017 Roman Gilg <subdiff@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "nightlightmanager.h"
#include "clockskewnotifier.h"
#include "colors/colordevice.h"
#include "colors/colormanager.h"
#include "nightlightdbusinterface.h"
#include "nightlightlogging.h"
#include "nightlightsettings.h"
#include "suncalc.h"

#include <core/outputbackend.h>
#include <core/session.h>
#include <input.h>
#include <main.h>
#include <workspace.h>

#include <KGlobalAccel>
#include <KLocalizedString>

#include <QAction>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusPendingReply>
#include <QDBusReply>
#include <QTimer>

namespace KWin
{

static const int QUICK_ADJUST_DURATION = 2000;
static const int TEMPERATURE_STEP = 50;
static NightLightManager *s_instance = nullptr;

static bool checkLocation(double lat, double lng)
{
    return -90 <= lat && lat <= 90 && -180 <= lng && lng <= 180;
}

NightLightManager *NightLightManager::self()
{
    return s_instance;
}

NightLightManager::NightLightManager()
{
    NightLightSettings::instance(kwinApp()->config());
    s_instance = this;

    m_iface = new NightLightDBusInterface(this);
    m_skewNotifier = new ClockSkewNotifier(this);

    // Display a message when Night Light is (un)inhibited.
    connect(this, &NightLightManager::inhibitedChanged, this, [this] {
        const QString iconName = isInhibited()
            ? QStringLiteral("redshift-status-off")
            : m_daylight && m_targetTemperature != DEFAULT_DAY_TEMPERATURE ? QStringLiteral("redshift-status-day")
                                                                           : QStringLiteral("redshift-status-on");

        const QString text = isInhibited()
            ? i18nc("Night Light was disabled", "Night Light Off")
            : i18nc("Night Light was enabled", "Night Light On");

        QDBusMessage message = QDBusMessage::createMethodCall(
            QStringLiteral("org.kde.plasmashell"),
            QStringLiteral("/org/kde/osdService"),
            QStringLiteral("org.kde.osdService"),
            QStringLiteral("showText"));
        message.setArguments({iconName, text});

        QDBusConnection::sessionBus().asyncCall(message);
    });

    m_configWatcher = KConfigWatcher::create(kwinApp()->config());
    connect(m_configWatcher.data(), &KConfigWatcher::configChanged, this, &NightLightManager::reconfigure);

    // we may always read in the current config
    readConfig();

    QAction *toggleAction = new QAction(this);
    toggleAction->setProperty("componentName", QStringLiteral("kwin"));
    toggleAction->setObjectName(QStringLiteral("Toggle Night Color"));
    toggleAction->setText(i18n("Toggle Night Light"));
    KGlobalAccel::setGlobalShortcut(toggleAction, QList<QKeySequence>());
    connect(toggleAction, &QAction::triggered, this, &NightLightManager::toggle);

    connect(kwinApp()->colorManager(), &ColorManager::deviceAdded, this, &NightLightManager::hardReset);

    connect(kwinApp()->session(), &Session::activeChanged, this, [this](bool active) {
        if (active) {
            hardReset();
        } else {
            cancelAllTimers();
        }
    });

    connect(m_skewNotifier, &ClockSkewNotifier::clockSkewed, this, [this]() {
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
            qCDebug(KWIN_NIGHTLIGHT) << "Failed to get PreparingForSleep Property of logind session:" << reply.error().message();
            // Always do a hard reset in case we have no further information.
            comingFromSuspend = true;
        }

        if (comingFromSuspend) {
            hardReset();
        } else {
            resetAllTimers();
        }
    });

    hardReset();
}

NightLightManager::~NightLightManager()
{
    s_instance = nullptr;
}

void NightLightManager::hardReset()
{
    cancelAllTimers();

    updateTransitionTimings(true);
    updateTargetTemperature();

    if (isEnabled() && !isInhibited()) {
        setRunning(true);
        commitGammaRamps(currentTargetTemp());
    }
    resetAllTimers();
}

void NightLightManager::reconfigure()
{
    cancelAllTimers();
    readConfig();
    resetAllTimers();
}

void NightLightManager::toggle()
{
    m_isGloballyInhibited = !m_isGloballyInhibited;
    m_isGloballyInhibited ? inhibit() : uninhibit();
}

bool NightLightManager::isInhibited() const
{
    return m_inhibitReferenceCount;
}

void NightLightManager::inhibit()
{
    m_inhibitReferenceCount++;

    if (m_inhibitReferenceCount == 1) {
        resetAllTimers();
        Q_EMIT inhibitedChanged();
    }
}

void NightLightManager::uninhibit()
{
    m_inhibitReferenceCount--;

    if (!m_inhibitReferenceCount) {
        resetAllTimers();
        Q_EMIT inhibitedChanged();
    }
}

bool NightLightManager::isEnabled() const
{
    return m_active;
}

bool NightLightManager::isRunning() const
{
    return m_running;
}

int NightLightManager::currentTemperature() const
{
    return m_currentTemp;
}

int NightLightManager::targetTemperature() const
{
    return m_targetTemperature;
}

NightLightMode NightLightManager::mode() const
{
    return m_mode;
}

QDateTime NightLightManager::previousTransitionDateTime() const
{
    return m_prev.first;
}

qint64 NightLightManager::previousTransitionDuration() const
{
    return m_prev.first.msecsTo(m_prev.second);
}

QDateTime NightLightManager::scheduledTransitionDateTime() const
{
    return m_next.first;
}

qint64 NightLightManager::scheduledTransitionDuration() const
{
    return m_next.first.msecsTo(m_next.second);
}

void NightLightManager::readConfig()
{
    NightLightSettings *s = NightLightSettings::self();
    s->load();

    setEnabled(s->active());

    const NightLightMode mode = s->mode();
    switch (s->mode()) {
    case NightLightMode::Automatic:
    case NightLightMode::Location:
    case NightLightMode::Timings:
    case NightLightMode::Constant:
        setMode(mode);
        break;
    default:
        // Fallback for invalid setting values.
        setMode(NightLightMode::Automatic);
        break;
    }

    m_dayTargetTemp = std::clamp(s->dayTemperature(), MIN_TEMPERATURE, DEFAULT_DAY_TEMPERATURE);
    m_nightTargetTemp = std::clamp(s->nightTemperature(), MIN_TEMPERATURE, DEFAULT_DAY_TEMPERATURE);

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

    int diffME = evB > mrB ? mrB.msecsTo(evB) : evB.msecsTo(mrB);
    int diffMin = std::min(diffME, MSC_DAY - diffME);

    int trTime = s->transitionTime() * 1000 * 60;
    if (trTime < 0 || diffMin <= trTime) {
        // transition time too long - use defaults
        mrB = QTime(6, 0);
        evB = QTime(18, 0);
        trTime = FALLBACK_SLOW_UPDATE_TIME;
    }
    m_morning = mrB;
    m_evening = evB;
    m_trTime = std::max(trTime / 1000 / 60, 1);
}

void NightLightManager::resetAllTimers()
{
    cancelAllTimers();
    setRunning(isEnabled() && !isInhibited());
    // we do this also for active being false in order to reset the temperature back to the day value
    updateTransitionTimings(false);
    updateTargetTemperature();
    resetQuickAdjustTimer(currentTargetTemp());
}

void NightLightManager::cancelAllTimers()
{
    m_slowUpdateStartTimer.reset();
    m_slowUpdateTimer.reset();
    m_quickAdjustTimer.reset();
}

void NightLightManager::resetQuickAdjustTimer(int targetTemp)
{
    int tempDiff = std::abs(targetTemp - m_currentTemp);
    // allow tolerance of one TEMPERATURE_STEP to compensate if a slow update is coincidental
    if (tempDiff > TEMPERATURE_STEP) {
        cancelAllTimers();
        m_quickAdjustTimer = std::make_unique<QTimer>();
        m_quickAdjustTimer->setSingleShot(false);
        connect(m_quickAdjustTimer.get(), &QTimer::timeout, this, [this, targetTemp]() {
            quickAdjust(targetTemp);
        });

        int interval = (QUICK_ADJUST_DURATION / (m_previewTimer && m_previewTimer->isActive() ? 8 : 1)) / (tempDiff / TEMPERATURE_STEP);
        if (interval == 0) {
            interval = 1;
        }
        m_quickAdjustTimer->start(interval);
    } else {
        resetSlowUpdateStartTimer();
    }
}

void NightLightManager::quickAdjust(int targetTemp)
{
    if (!m_quickAdjustTimer) {
        return;
    }

    int nextTemp;

    if (m_currentTemp < targetTemp) {
        nextTemp = std::min(m_currentTemp + TEMPERATURE_STEP, targetTemp);
    } else {
        nextTemp = std::max(m_currentTemp - TEMPERATURE_STEP, targetTemp);
    }
    commitGammaRamps(nextTemp);

    if (nextTemp == targetTemp) {
        // stop timer, we reached the target temp
        m_quickAdjustTimer.reset();
        resetSlowUpdateStartTimer();
    }
}

void NightLightManager::resetSlowUpdateStartTimer()
{
    m_slowUpdateStartTimer.reset();

    if (!m_running || m_quickAdjustTimer) {
        // only reenable the slow update start timer when quick adjust is not active anymore
        return;
    }

    // There is no need for starting the slow update timer. Screen color temperature
    // will be constant all the time now.
    if (m_mode == NightLightMode::Constant) {
        return;
    }

    // set up the next slow update
    m_slowUpdateStartTimer = std::make_unique<QTimer>();
    m_slowUpdateStartTimer->setSingleShot(true);
    connect(m_slowUpdateStartTimer.get(), &QTimer::timeout, this, &NightLightManager::resetSlowUpdateStartTimer);

    updateTransitionTimings(false);
    updateTargetTemperature();

    const int diff = QDateTime::currentDateTime().msecsTo(m_next.first);
    if (diff <= 0) {
        qCCritical(KWIN_NIGHTLIGHT) << "Error in time calculation. Deactivating Night Light.";
        return;
    }
    m_slowUpdateStartTimer->start(diff);

    // start the current slow update
    resetSlowUpdateTimer();
}

void NightLightManager::resetSlowUpdateTimer()
{
    m_slowUpdateTimer.reset();

    const QDateTime now = QDateTime::currentDateTime();
    const bool isDay = daylight();
    const int targetTemp = isDay ? m_dayTargetTemp : m_nightTargetTemp;

    // We've reached the target color temperature or the transition time is zero.
    if (m_prev.first == m_prev.second || m_currentTemp == targetTemp) {
        commitGammaRamps(targetTemp);
        return;
    }

    if (m_prev.first <= now && now <= m_prev.second) {
        int availTime = now.msecsTo(m_prev.second);
        m_slowUpdateTimer = std::make_unique<QTimer>();
        m_slowUpdateTimer->setSingleShot(false);
        if (isDay) {
            connect(m_slowUpdateTimer.get(), &QTimer::timeout, this, [this]() {
                slowUpdate(m_dayTargetTemp);
            });
        } else {
            connect(m_slowUpdateTimer.get(), &QTimer::timeout, this, [this]() {
                slowUpdate(m_nightTargetTemp);
            });
        }

        // calculate interval such as temperature is changed by TEMPERATURE_STEP K per timer timeout
        int interval = availTime * TEMPERATURE_STEP / std::abs(targetTemp - m_currentTemp);
        if (interval == 0) {
            interval = 1;
        }
        m_slowUpdateTimer->start(interval);
    }
}

void NightLightManager::slowUpdate(int targetTemp)
{
    if (!m_slowUpdateTimer) {
        return;
    }
    int nextTemp;
    if (m_currentTemp < targetTemp) {
        nextTemp = std::min(m_currentTemp + TEMPERATURE_STEP, targetTemp);
    } else {
        nextTemp = std::max(m_currentTemp - TEMPERATURE_STEP, targetTemp);
    }
    commitGammaRamps(nextTemp);
    if (nextTemp == targetTemp) {
        // stop timer, we reached the target temp
        m_slowUpdateTimer.reset();
    }
}

void NightLightManager::preview(uint previewTemp)
{
    previewTemp = std::clamp<uint>(previewTemp, MIN_TEMPERATURE, DEFAULT_DAY_TEMPERATURE);
    resetQuickAdjustTimer((int)previewTemp);
    if (m_previewTimer) {
        m_previewTimer.reset();
    }
    m_previewTimer = std::make_unique<QTimer>();
    m_previewTimer->setSingleShot(true);
    connect(m_previewTimer.get(), &QTimer::timeout, this, &NightLightManager::stopPreview);
    m_previewTimer->start(15000);

    QDBusMessage message = QDBusMessage::createMethodCall(
        QStringLiteral("org.kde.plasmashell"),
        QStringLiteral("/org/kde/osdService"),
        QStringLiteral("org.kde.osdService"),
        QStringLiteral("showText"));
    message.setArguments(
        {QStringLiteral("redshift-status-on"),
         i18n("Color Temperature Preview")});
    QDBusConnection::sessionBus().asyncCall(message);
}

void NightLightManager::stopPreview()
{
    if (m_previewTimer && m_previewTimer->isActive()) {
        updateTransitionTimings(false);
        updateTargetTemperature();
        resetQuickAdjustTimer(currentTargetTemp());
    }
}

void NightLightManager::updateTargetTemperature()
{
    const int targetTemperature = mode() != NightLightMode::Constant && daylight() ? m_dayTargetTemp : m_nightTargetTemp;

    if (m_targetTemperature == targetTemperature) {
        return;
    }

    m_targetTemperature = targetTemperature;

    Q_EMIT targetTemperatureChanged();
}

void NightLightManager::updateTransitionTimings(bool force)
{
    const auto oldPrev = m_prev;
    const auto oldNext = m_next;

    if (m_mode == NightLightMode::Constant) {
        setDaylight(false);
        m_next = DateTimes();
        m_prev = DateTimes();
    } else if (m_mode == NightLightMode::Timings) {
        const QDateTime todayNow = QDateTime::currentDateTime();

        const QDateTime nextMorB = QDateTime(todayNow.date().addDays(m_morning < todayNow.time()), m_morning);
        const QDateTime nextMorE = nextMorB.addSecs(m_trTime * 60);
        const QDateTime nextEveB = QDateTime(todayNow.date().addDays(m_evening < todayNow.time()), m_evening);
        const QDateTime nextEveE = nextEveB.addSecs(m_trTime * 60);

        if (nextEveB < nextMorB) {
            setDaylight(true);
            m_next = DateTimes(nextEveB, nextEveE);
            m_prev = DateTimes(nextMorB.addDays(-1), nextMorE.addDays(-1));
        } else {
            setDaylight(false);
            m_next = DateTimes(nextMorB, nextMorE);
            m_prev = DateTimes(nextEveB.addDays(-1), nextEveE.addDays(-1));
        }
    } else {
        const QDateTime todayNow = QDateTime::currentDateTime();

        double lat, lng;
        if (m_mode == NightLightMode::Automatic) {
            lat = m_latAuto;
            lng = m_lngAuto;
        } else {
            lat = m_latFixed;
            lng = m_lngFixed;
        }

        if (!force) {
            // first try by only switching the timings
            if (m_prev.first.date() == m_next.first.date()) {
                // next is evening
                setDaylight(true);
                m_prev = m_next;
                m_next = getSunTimings(todayNow, lat, lng, false);
            } else {
                // next is morning
                setDaylight(false);
                m_prev = m_next;
                m_next = getSunTimings(todayNow.addDays(1), lat, lng, true);
            }
        }

        if (force || !checkAutomaticSunTimings()) {
            // in case this fails, reset them
            DateTimes morning = getSunTimings(todayNow, lat, lng, true);
            if (todayNow < morning.first) {
                setDaylight(false);
                m_prev = getSunTimings(todayNow.addDays(-1), lat, lng, false);
                m_next = morning;
            } else {
                DateTimes evening = getSunTimings(todayNow, lat, lng, false);
                if (todayNow < evening.first) {
                    setDaylight(true);
                    m_prev = morning;
                    m_next = evening;
                } else {
                    setDaylight(false);
                    m_prev = evening;
                    m_next = getSunTimings(todayNow.addDays(1), lat, lng, true);
                }
            }
        }
    }

    if (oldPrev != m_prev) {
        Q_EMIT previousTransitionTimingsChanged();
    }
    if (oldNext != m_next) {
        Q_EMIT scheduledTransitionTimingsChanged();
    }
}

DateTimes NightLightManager::getSunTimings(const QDateTime &dateTime, double latitude, double longitude, bool morning) const
{
    DateTimes dateTimes = calculateSunTimings(dateTime, latitude, longitude, morning);
    // At locations near the poles it is possible, that we can't
    // calculate some or all sun timings (midnight sun).
    // In this case try to fallback to sensible default values.
    const bool beginDefined = !dateTimes.first.isNull();
    const bool endDefined = !dateTimes.second.isNull();
    if (!beginDefined || !endDefined) {
        if (beginDefined) {
            dateTimes.second = dateTimes.first.addMSecs(FALLBACK_SLOW_UPDATE_TIME);
        } else if (endDefined) {
            dateTimes.first = dateTimes.second.addMSecs(-FALLBACK_SLOW_UPDATE_TIME);
        } else {
            // Just use default values for morning and evening, but the user
            // will probably deactivate Night Light anyway if he is living
            // in a region without clear sun rise and set.
            const QTime referenceTime = morning ? QTime(6, 0) : QTime(18, 0);
            dateTimes.first = QDateTime(dateTime.date(), referenceTime);
            dateTimes.second = dateTimes.first.addMSecs(FALLBACK_SLOW_UPDATE_TIME);
        }
    }
    return dateTimes;
}

bool NightLightManager::checkAutomaticSunTimings() const
{
    if (m_prev.first.isValid() && m_prev.second.isValid() && m_next.first.isValid() && m_next.second.isValid()) {
        const QDateTime todayNow = QDateTime::currentDateTime();
        return m_prev.first <= todayNow && todayNow < m_next.first && m_prev.first.msecsTo(m_next.first) < MSC_DAY * 23. / 24;
    }
    return false;
}

bool NightLightManager::daylight() const
{
    return m_daylight;
}

int NightLightManager::currentTargetTemp() const
{
    if (!m_running) {
        return DEFAULT_DAY_TEMPERATURE;
    }

    if (m_mode == NightLightMode::Constant) {
        return m_nightTargetTemp;
    }

    const QDateTime todayNow = QDateTime::currentDateTime();

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

void NightLightManager::commitGammaRamps(int temperature)
{
    const QList<ColorDevice *> devices = kwinApp()->colorManager()->devices();
    for (ColorDevice *device : devices) {
        device->setTemperature(temperature);
    }

    setCurrentTemperature(temperature);
}

void NightLightManager::autoLocationUpdate(double latitude, double longitude)
{
    qCDebug(KWIN_NIGHTLIGHT, "Received new location (lat: %f, lng: %f)", latitude, longitude);

    if (!checkLocation(latitude, longitude)) {
        return;
    }

    // we tolerate small deviations with minimal impact on sun timings
    if (std::abs(m_latAuto - latitude) < 2 && std::abs(m_lngAuto - longitude) < 1) {
        return;
    }
    cancelAllTimers();
    m_latAuto = latitude;
    m_lngAuto = longitude;

    NightLightSettings *s = NightLightSettings::self();
    s->setLatitudeAuto(latitude);
    s->setLongitudeAuto(longitude);
    s->save();

    resetAllTimers();
}

void NightLightManager::setEnabled(bool enabled)
{
    if (m_active == enabled) {
        return;
    }
    m_active = enabled;
    m_skewNotifier->setActive(enabled);
    Q_EMIT enabledChanged();
}

void NightLightManager::setRunning(bool running)
{
    if (m_running == running) {
        return;
    }
    m_running = running;
    Q_EMIT runningChanged();
}

void NightLightManager::setCurrentTemperature(int temperature)
{
    if (m_currentTemp == temperature) {
        return;
    }
    m_currentTemp = temperature;
    Q_EMIT currentTemperatureChanged();
}

void NightLightManager::setMode(NightLightMode mode)
{
    if (m_mode == mode) {
        return;
    }
    m_mode = mode;
    Q_EMIT modeChanged();
}

void NightLightManager::setDaylight(bool daylight)
{
    if (m_daylight == daylight) {
        return;
    }
    m_daylight = daylight;
    Q_EMIT daylightChanged();
}

} // namespace KWin

#include "moc_nightlightmanager.cpp"
