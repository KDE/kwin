/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2017 Roman Gilg <subdiff@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "nightcolormanager.h"
#include "clockskewnotifier.h"
#include "colordevice.h"
#include "colormanager.h"
#include "nightcolordbusinterface.h"
#include "nightcolorlogging.h"
#include "nightcolorsettings.h"
#include "suncalc.h"

#include <main.h>
#include <platform.h>
#include <session.h>
#include <workspace.h>

#include <KGlobalAccel>
#include <KLocalizedString>

#include <QAction>
#include <QDBusConnection>
#include <QTimer>

namespace KWin {

static const int QUICK_ADJUST_DURATION = 2000;
static const int TEMPERATURE_STEP = 50;
static NightColorManager *s_instance = nullptr;

static bool checkLocation(double lat, double lng)
{
    return -90 <= lat && lat <= 90 && -180 <= lng && lng <= 180;
}

NightColorManager *NightColorManager::self()
{
    return s_instance;
}

NightColorManager::NightColorManager(QObject *parent)
    : Plugin(parent)
{
    s_instance = this;

    m_iface = new NightColorDBusInterface(this);
    m_skewNotifier = new ClockSkewNotifier(this);

    // Display a message when Night Color is (un)inhibited.
    connect(this, &NightColorManager::inhibitedChanged, this, [this] {
        // TODO: Maybe use different icons?
        const QString iconName = isInhibited()
            ? QStringLiteral("preferences-desktop-display-nightcolor-off")
            : QStringLiteral("preferences-desktop-display-nightcolor-on");

        const QString text = isInhibited()
            ? i18nc("Night Color was disabled", "Night Color Off")
            : i18nc("Night Color was enabled", "Night Color On");

        QDBusMessage message = QDBusMessage::createMethodCall(
            QStringLiteral("org.kde.plasmashell"),
            QStringLiteral("/org/kde/osdService"),
            QStringLiteral("org.kde.osdService"),
            QStringLiteral("showText"));
        message.setArguments({ iconName, text });

        QDBusConnection::sessionBus().asyncCall(message);
    });

    if (workspace()) {
        init();
    } else {
        connect(kwinApp(), &Application::workspaceCreated, this, &NightColorManager::init);
    }
}

NightColorManager::~NightColorManager()
{
    s_instance = nullptr;
}

void NightColorManager::init()
{
    NightColorSettings::instance(kwinApp()->config());

    m_configWatcher = KConfigWatcher::create(kwinApp()->config());
    connect(m_configWatcher.data(), &KConfigWatcher::configChanged, this, &NightColorManager::reconfigure);

    // we may always read in the current config
    readConfig();

    if (!isAvailable()) {
        return;
    }

    // legacy shortcut with localized key (to avoid breaking existing config)
    if (i18n("Toggle Night Color") != QStringLiteral("Toggle Night Color")) {
        QAction toggleActionLegacy;
        toggleActionLegacy.setProperty("componentName", QStringLiteral(KWIN_NAME));
        toggleActionLegacy.setObjectName(i18n("Toggle Night Color"));
        KGlobalAccel::self()->removeAllShortcuts(&toggleActionLegacy);
    }

    QAction *toggleAction = new QAction(this);
    toggleAction->setProperty("componentName", QStringLiteral(KWIN_NAME));
    toggleAction->setObjectName(QStringLiteral("Toggle Night Color"));
    toggleAction->setText(i18n("Toggle Night Color"));
    KGlobalAccel::setGlobalShortcut(toggleAction, QList<QKeySequence>());
    input()->registerShortcut(QKeySequence(), toggleAction, this, &NightColorManager::toggle);

    connect(ColorManager::self(), &ColorManager::deviceAdded, this, &NightColorManager::hardReset);

    connect(kwinApp()->platform()->session(), &Session::activeChanged, this, [this](bool active) {
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
            qCDebug(KWIN_NIGHTCOLOR) << "Failed to get PreparingForSleep Property of logind session:" << reply.error().message();
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

void NightColorManager::hardReset()
{
    cancelAllTimers();

    updateTransitionTimings(true);
    updateTargetTemperature();

    if (isAvailable() && isEnabled() && !isInhibited()) {
        setRunning(true);
        commitGammaRamps(currentTargetTemp());
    }
    resetAllTimers();
}

void NightColorManager::reconfigure()
{
    cancelAllTimers();
    readConfig();
    resetAllTimers();
}

void NightColorManager::toggle()
{
    m_isGloballyInhibited = !m_isGloballyInhibited;
    m_isGloballyInhibited ? inhibit() : uninhibit();
}

bool NightColorManager::isInhibited() const
{
    return m_inhibitReferenceCount;
}

void NightColorManager::inhibit()
{
    m_inhibitReferenceCount++;

    if (m_inhibitReferenceCount == 1) {
        resetAllTimers();
        Q_EMIT inhibitedChanged();
    }
}

void NightColorManager::uninhibit()
{
    m_inhibitReferenceCount--;

    if (!m_inhibitReferenceCount) {
        resetAllTimers();
        Q_EMIT inhibitedChanged();
    }
}

bool NightColorManager::isEnabled() const
{
    return m_active;
}

bool NightColorManager::isRunning() const
{
    return m_running;
}

bool NightColorManager::isAvailable() const
{
    return kwinApp()->platform()->supportsGammaControl();
}

int NightColorManager::currentTemperature() const
{
    return m_currentTemp;
}

int NightColorManager::targetTemperature() const
{
    return m_targetTemperature;
}

NightColorMode NightColorManager::mode() const
{
    return m_mode;
}

QDateTime NightColorManager::previousTransitionDateTime() const
{
    return m_prev.first;
}

qint64 NightColorManager::previousTransitionDuration() const
{
    return m_prev.first.msecsTo(m_prev.second);
}

QDateTime NightColorManager::scheduledTransitionDateTime() const
{
    return m_next.first;
}

qint64 NightColorManager::scheduledTransitionDuration() const
{
    return m_next.first.msecsTo(m_next.second);
}

void NightColorManager::readConfig()
{
    NightColorSettings *s = NightColorSettings::self();
    s->load();

    setEnabled(s->active());

    const NightColorMode mode = s->mode();
    switch (s->mode()) {
    case NightColorMode::Automatic:
    case NightColorMode::Location:
    case NightColorMode::Timings:
    case NightColorMode::Constant:
        setMode(mode);
        break;
    default:
        // Fallback for invalid setting values.
        setMode(NightColorMode::Automatic);
        break;
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

void NightColorManager::resetAllTimers()
{
    cancelAllTimers();
    if (isAvailable()) {
        setRunning(isEnabled() && !isInhibited());
        // we do this also for active being false in order to reset the temperature back to the day value
        resetQuickAdjustTimer();
    } else {
        setRunning(false);
    }
}

void NightColorManager::cancelAllTimers()
{
    delete m_slowUpdateStartTimer;
    delete m_slowUpdateTimer;
    delete m_quickAdjustTimer;

    m_slowUpdateStartTimer = nullptr;
    m_slowUpdateTimer = nullptr;
    m_quickAdjustTimer = nullptr;
}

void NightColorManager::resetQuickAdjustTimer()
{
    updateTransitionTimings(false);
    updateTargetTemperature();

    int tempDiff = qAbs(currentTargetTemp() - m_currentTemp);
    // allow tolerance of one TEMPERATURE_STEP to compensate if a slow update is coincidental
    if (tempDiff > TEMPERATURE_STEP) {
        cancelAllTimers();
        m_quickAdjustTimer = new QTimer(this);
        m_quickAdjustTimer->setSingleShot(false);
        connect(m_quickAdjustTimer, &QTimer::timeout, this, &NightColorManager::quickAdjust);

        int interval = QUICK_ADJUST_DURATION / (tempDiff / TEMPERATURE_STEP);
        if (interval == 0) {
            interval = 1;
        }
        m_quickAdjustTimer->start(interval);
    } else {
        resetSlowUpdateStartTimer();
    }
}

void NightColorManager::quickAdjust()
{
    if (!m_quickAdjustTimer) {
        return;
    }

    int nextTemp;
    const int targetTemp = currentTargetTemp();

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

void NightColorManager::resetSlowUpdateStartTimer()
{
    delete m_slowUpdateStartTimer;
    m_slowUpdateStartTimer = nullptr;

    if (!m_running || m_quickAdjustTimer) {
        // only reenable the slow update start timer when quick adjust is not active anymore
        return;
    }

    // There is no need for starting the slow update timer. Screen color temperature
    // will be constant all the time now.
    if (m_mode == NightColorMode::Constant) {
        return;
    }

    // set up the next slow update
    m_slowUpdateStartTimer = new QTimer(this);
    m_slowUpdateStartTimer->setSingleShot(true);
    connect(m_slowUpdateStartTimer, &QTimer::timeout, this, &NightColorManager::resetSlowUpdateStartTimer);

    updateTransitionTimings(false);
    updateTargetTemperature();

    const int diff = QDateTime::currentDateTime().msecsTo(m_next.first);
    if (diff <= 0) {
        qCCritical(KWIN_NIGHTCOLOR) << "Error in time calculation. Deactivating Night Color.";
        return;
    }
    m_slowUpdateStartTimer->start(diff);

    // start the current slow update
    resetSlowUpdateTimer();
}

void NightColorManager::resetSlowUpdateTimer()
{
    delete m_slowUpdateTimer;
    m_slowUpdateTimer = nullptr;

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
        m_slowUpdateTimer = new QTimer(this);
        m_slowUpdateTimer->setSingleShot(false);
        if (isDay) {
            connect(m_slowUpdateTimer, &QTimer::timeout, this, [this]() {slowUpdate(m_dayTargetTemp);});
        } else {
            connect(m_slowUpdateTimer, &QTimer::timeout, this, [this]() {slowUpdate(m_nightTargetTemp);});
        }

        // calculate interval such as temperature is changed by TEMPERATURE_STEP K per timer timeout
        int interval = availTime * TEMPERATURE_STEP / qAbs(targetTemp - m_currentTemp);
        if (interval == 0) {
            interval = 1;
        }
        m_slowUpdateTimer->start(interval);
    }
}

void NightColorManager::slowUpdate(int targetTemp)
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

void NightColorManager::updateTargetTemperature()
{
    const int targetTemperature = mode() != NightColorMode::Constant && daylight() ? m_dayTargetTemp : m_nightTargetTemp;

    if (m_targetTemperature == targetTemperature) {
        return;
    }

    m_targetTemperature = targetTemperature;

    Q_EMIT targetTemperatureChanged();
}

void NightColorManager::updateTransitionTimings(bool force)
{
    if (m_mode == NightColorMode::Constant) {
        m_next = DateTimes();
        m_prev = DateTimes();
        Q_EMIT previousTransitionTimingsChanged();
        Q_EMIT scheduledTransitionTimingsChanged();
        return;
    }

    const QDateTime todayNow = QDateTime::currentDateTime();

    if (m_mode == NightColorMode::Timings) {
        const QDateTime morB = QDateTime(todayNow.date(), m_morning);
        const QDateTime morE = morB.addSecs(m_trTime * 60);
        const QDateTime eveB = QDateTime(todayNow.date(), m_evening);
        const QDateTime eveE = eveB.addSecs(m_trTime * 60);

        if (morB <= todayNow && todayNow < eveB) {
            m_next = DateTimes(eveB, eveE);
            m_prev = DateTimes(morB, morE);
        } else if (todayNow < morB) {
            m_next = DateTimes(morB, morE);
            m_prev = DateTimes(eveB.addDays(-1), eveE.addDays(-1));
        } else {
            m_next = DateTimes(morB.addDays(1), morE.addDays(1));
            m_prev = DateTimes(eveB, eveE);
        }
        Q_EMIT previousTransitionTimingsChanged();
        Q_EMIT scheduledTransitionTimingsChanged();
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
            m_next = getSunTimings(todayNow.addDays(1), lat, lng, true);
        } else {
            // next is evening
            m_prev = m_next;
            m_next = getSunTimings(todayNow, lat, lng, false);
        }
    }

    if (force || !checkAutomaticSunTimings()) {
        // in case this fails, reset them
        DateTimes morning = getSunTimings(todayNow, lat, lng, true);
        if (todayNow < morning.first) {
            m_prev = getSunTimings(todayNow.addDays(-1), lat, lng, false);
            m_next = morning;
        } else {
            DateTimes evening = getSunTimings(todayNow, lat, lng, false);
            if (todayNow < evening.first) {
                m_prev = morning;
                m_next = evening;
            } else {
                m_prev = evening;
                m_next = getSunTimings(todayNow.addDays(1), lat, lng, true);
            }
        }
    }

    Q_EMIT previousTransitionTimingsChanged();
    Q_EMIT scheduledTransitionTimingsChanged();
}

DateTimes NightColorManager::getSunTimings(const QDateTime &dateTime, double latitude, double longitude, bool morning) const
{
    DateTimes dateTimes = calculateSunTimings(dateTime, latitude, longitude, morning);
    // At locations near the poles it is possible, that we can't
    // calculate some or all sun timings (midnight sun).
    // In this case try to fallback to sensible default values.
    const bool beginDefined = !dateTimes.first.isNull();
    const bool endDefined = !dateTimes.second.isNull();
    if (!beginDefined || !endDefined) {
        if (beginDefined) {
            dateTimes.second = dateTimes.first.addMSecs( FALLBACK_SLOW_UPDATE_TIME );
        } else if (endDefined) {
            dateTimes.first = dateTimes.second.addMSecs( - FALLBACK_SLOW_UPDATE_TIME );
        } else {
            // Just use default values for morning and evening, but the user
            // will probably deactivate Night Color anyway if he is living
            // in a region without clear sun rise and set.
            const QTime referenceTime = morning ? QTime(6, 0) : QTime(18, 0);
            dateTimes.first = QDateTime(dateTime.date(), referenceTime);
            dateTimes.second = dateTimes.first.addMSecs( FALLBACK_SLOW_UPDATE_TIME );
        }
    }
    return dateTimes;
}

bool NightColorManager::checkAutomaticSunTimings() const
{
    if (m_prev.first.isValid() && m_prev.second.isValid() &&
            m_next.first.isValid() && m_next.second.isValid()) {
        const QDateTime todayNow = QDateTime::currentDateTime();
        return m_prev.first <= todayNow && todayNow < m_next.first &&
                m_prev.first.msecsTo(m_next.first) < MSC_DAY * 23./24;
    }
    return false;
}

bool NightColorManager::daylight() const
{
    return m_prev.first.date() == m_next.first.date();
}

int NightColorManager::currentTargetTemp() const
{
    if (!m_running) {
        return NEUTRAL_TEMPERATURE;
    }

    if (m_mode == NightColorMode::Constant) {
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

void NightColorManager::commitGammaRamps(int temperature)
{
    const QVector<ColorDevice *> devices = ColorManager::self()->devices();
    for (ColorDevice *device : devices) {
        device->setTemperature(temperature);
    }

    setCurrentTemperature(temperature);
}

void NightColorManager::autoLocationUpdate(double latitude, double longitude)
{
    qCDebug(KWIN_NIGHTCOLOR, "Received new location (lat: %f, lng: %f)", latitude, longitude);

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

    NightColorSettings *s = NightColorSettings::self();
    s->setLatitudeAuto(latitude);
    s->setLongitudeAuto(longitude);
    s->save();

    resetAllTimers();
}

void NightColorManager::setEnabled(bool enabled)
{
    if (m_active == enabled) {
        return;
    }
    m_active = enabled;
    m_skewNotifier->setActive(enabled);
    Q_EMIT enabledChanged();
}

void NightColorManager::setRunning(bool running)
{
    if (m_running == running) {
        return;
    }
    m_running = running;
    Q_EMIT runningChanged();
}

void NightColorManager::setCurrentTemperature(int temperature)
{
    if (m_currentTemp == temperature) {
        return;
    }
    m_currentTemp = temperature;
    Q_EMIT currentTemperatureChanged();
}

void NightColorManager::setMode(NightColorMode mode)
{
    if (m_mode == mode) {
        return;
    }
    m_mode = mode;
    Q_EMIT modeChanged();
}

} // namespace KWin
