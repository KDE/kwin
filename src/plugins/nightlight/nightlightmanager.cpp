/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2017 Roman Gilg <subdiff@gmail.com>
    SPDX-FileCopyrightText: 2024 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "nightlightmanager.h"
#include "colors/colordevice.h"
#include "colors/colormanager.h"
#include "core/outputbackend.h"
#include "core/session.h"
#include "main.h"
#include "nightlightdbusinterface.h"
#include "nightlightlogging.h"
#include "nightlightsettings.h"
#include "nightlightstate.h"

#include <KDarkLightScheduleProvider>
#include <KGlobalAccel>
#include <KLocalizedString>
#include <KSystemClockSkewNotifier>

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

NightLightManager::NightLightManager()
{
    NightLightSettings::instance(kwinApp()->config());

    m_iface = new NightLightDBusInterface(this);
    m_skewNotifier = new KSystemClockSkewNotifier(this);
    connect(m_skewNotifier, &KSystemClockSkewNotifier::skewed, this, &NightLightManager::resetAllTimers);

    // Display a message when Night Light is (un)inhibited.
    connect(this, &NightLightManager::inhibitedChanged, this, [this] {
        const QString iconName = isInhibited()
            ? QStringLiteral("redshift-status-off")
            : m_daylight && m_targetTemperature != DEFAULT_DAY_TEMPERATURE ? QStringLiteral("redshift-status-day")
                                                                           : QStringLiteral("redshift-status-on");

        const QString text = isInhibited()
            ? i18nc("Night Light was temporarily disabled", "Night Light Suspended")
            : i18nc("Night Light was reenabled from temporary suspension", "Night Light Resumed");

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
    toggleAction->setText(i18nc("Temporarily disable/reenable Night Light", "Suspend/Resume Night Light"));
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
    connect(kwinApp()->session(), &Session::awoke, this, &NightLightManager::hardReset);

    hardReset();
}

NightLightManager::~NightLightManager()
{
}

void NightLightManager::hardReset()
{
    cancelAllTimers();

    updateTransitionTimings(QDateTime::currentDateTime());
    updateTargetTemperature();

    if (isEnabled() && !isInhibited()) {
        setRunning(true);
        commitGammaRamps(currentTargetTemperature());
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
    return m_currentTemperature;
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
    NightLightSettings *settings = NightLightSettings::self();
    settings->load();

    setEnabled(settings->active());

    const NightLightMode mode = settings->mode();
    switch (settings->mode()) {
    case NightLightMode::Constant:
    case NightLightMode::DarkLight:
        setMode(mode);
        break;
    default:
        // Fallback for invalid setting values.
        setMode(NightLightMode::DarkLight);
        break;
    }

    if (m_active && m_mode == NightLightMode::DarkLight) {
        if (!m_stateConfig) {
            m_stateConfig = std::make_unique<NightLightState>();
        }
        if (!m_darkLightScheduler) {
            m_darkLightScheduler = std::make_unique<KDarkLightScheduleProvider>(m_stateConfig->state());
            connect(m_darkLightScheduler.get(), &KDarkLightScheduleProvider::scheduleChanged, this, [this]() {
                m_stateConfig->setState(m_darkLightScheduler->state());
                m_stateConfig->save();
                resetAllTimers();
            });
        }
    } else {
        m_stateConfig.reset();
        m_darkLightScheduler.reset();
    }

    m_dayTargetTemperature = std::clamp(settings->dayTemperature(), MIN_TEMPERATURE, DEFAULT_DAY_TEMPERATURE);
    m_nightTargetTemperature = std::clamp(settings->nightTemperature(), MIN_TEMPERATURE, DEFAULT_DAY_TEMPERATURE);
}

void NightLightManager::resetAllTimers()
{
    cancelAllTimers();
    setRunning(isEnabled() && !isInhibited());
    // we do this also for active being false in order to reset the temperature back to the day value
    updateTransitionTimings(QDateTime::currentDateTime());
    updateTargetTemperature();
    resetQuickAdjustTimer(currentTargetTemperature());
}

void NightLightManager::cancelAllTimers()
{
    m_slowUpdateStartTimer.reset();
    m_slowUpdateTimer.reset();
    m_quickAdjustTimer.reset();
}

void NightLightManager::resetQuickAdjustTimer(int targetTemperature)
{
    int tempDiff = std::abs(targetTemperature - m_currentTemperature);
    // allow tolerance of one TEMPERATURE_STEP to compensate if a slow update is coincidental
    if (tempDiff > TEMPERATURE_STEP) {
        cancelAllTimers();
        m_quickAdjustTimer = std::make_unique<QTimer>();
        m_quickAdjustTimer->setSingleShot(false);
        connect(m_quickAdjustTimer.get(), &QTimer::timeout, this, [this, targetTemperature]() {
            quickAdjust(targetTemperature);
        });

        int interval = (QUICK_ADJUST_DURATION / (m_previewTimer ? 8 : 1)) / (tempDiff / TEMPERATURE_STEP);
        if (interval == 0) {
            interval = 1;
        }
        m_quickAdjustTimer->start(interval);
    } else {
        resetSlowUpdateTimers();
    }
}

void NightLightManager::quickAdjust(int targetTemperature)
{
    if (!m_quickAdjustTimer) {
        return;
    }

    int nextTemperature;
    if (m_currentTemperature < targetTemperature) {
        nextTemperature = std::min(m_currentTemperature + TEMPERATURE_STEP, targetTemperature);
    } else {
        nextTemperature = std::max(m_currentTemperature - TEMPERATURE_STEP, targetTemperature);
    }
    commitGammaRamps(nextTemperature);

    if (nextTemperature == targetTemperature) {
        // stop timer, we reached the target temp
        m_quickAdjustTimer.reset();
        resetSlowUpdateTimers();
    }
}

void NightLightManager::resetSlowUpdateTimers()
{
    m_slowUpdateStartTimer.reset();

    if (!m_running || m_quickAdjustTimer) {
        // only reenable the slow update start timer when quick adjust is not active anymore
        return;
    }

    // If a preview is currently running, do not reset the update timer as it
    // would change the temperature back to the normal value.
    if (m_previewTimer) {
        return;
    }

    // There is no need for starting the slow update timer. Screen color temperature
    // will be constant all the time now.
    if (m_mode == NightLightMode::Constant) {
        return;
    }

    const QDateTime dateTime = QDateTime::currentDateTime();
    updateTransitionTimings(dateTime);
    updateTargetTemperature();

    const int diff = dateTime.msecsTo(m_next.first);
    if (diff <= 0) {
        qCCritical(KWIN_NIGHTLIGHT) << "Error in time calculation. Deactivating Night Light.";
        return;
    }
    m_slowUpdateStartTimer = std::make_unique<QTimer>();
    m_slowUpdateStartTimer->setSingleShot(true);
    connect(m_slowUpdateStartTimer.get(), &QTimer::timeout, this, &NightLightManager::resetSlowUpdateTimers);
    m_slowUpdateStartTimer->start(diff);

    // start the current slow update
    m_slowUpdateTimer.reset();

    if (m_currentTemperature == m_targetTemperature) {
        return;
    }

    if (dateTime < m_prev.second) {
        m_slowUpdateTimer = std::make_unique<QTimer>();
        m_slowUpdateTimer->setSingleShot(false);
        connect(m_slowUpdateTimer.get(), &QTimer::timeout, this, [this]() {
            slowUpdate(m_targetTemperature);
        });

        // calculate interval such as temperature is changed by TEMPERATURE_STEP K per timer timeout
        int interval = dateTime.msecsTo(m_prev.second) * TEMPERATURE_STEP / std::abs(m_targetTemperature - m_currentTemperature);
        if (interval == 0) {
            interval = 1;
        }
        m_slowUpdateTimer->start(interval);
    } else {
        commitGammaRamps(m_targetTemperature);
    }
}

void NightLightManager::slowUpdate(int targetTemperature)
{
    if (!m_slowUpdateTimer) {
        return;
    }
    int nextTemperature;
    if (m_currentTemperature < targetTemperature) {
        nextTemperature = std::min(m_currentTemperature + TEMPERATURE_STEP, targetTemperature);
    } else {
        nextTemperature = std::max(m_currentTemperature - TEMPERATURE_STEP, targetTemperature);
    }
    commitGammaRamps(nextTemperature);
    if (nextTemperature == targetTemperature) {
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
    if (m_previewTimer) {
        m_previewTimer.reset();
        updateTransitionTimings(QDateTime::currentDateTime());
        updateTargetTemperature();
        resetQuickAdjustTimer(currentTargetTemperature());
    }
}

void NightLightManager::updateTargetTemperature()
{
    const int targetTemperature = mode() != NightLightMode::Constant && daylight() ? m_dayTargetTemperature : m_nightTargetTemperature;

    if (m_targetTemperature == targetTemperature) {
        return;
    }

    m_targetTemperature = targetTemperature;

    Q_EMIT targetTemperatureChanged();
}

void NightLightManager::updateTransitionTimings(const QDateTime &dateTime)
{
    const auto oldPrev = m_prev;
    const auto oldNext = m_next;

    if (!m_active) {
        setDaylight(true);
        m_next = DateTimes();
        m_prev = DateTimes();
    } else {
        switch (m_mode) {
        case NightLightMode::Constant: {
            setDaylight(false);
            m_next = DateTimes();
            m_prev = DateTimes();
            break;
        }

        case NightLightMode::DarkLight: {
            const auto previousTransition = m_darkLightScheduler->schedule().previousTransition(dateTime);
            const auto nextTransition = m_darkLightScheduler->schedule().nextTransition(dateTime);

            bool passedMorning = false;
            bool passedEvening = false;

            switch (previousTransition->test(dateTime)) {
            case KDarkLightTransition::Upcoming:
                break;
            case KDarkLightTransition::InProgress:
            case KDarkLightTransition::Passed:
                if (previousTransition->type() == KDarkLightTransition::Morning) {
                    passedMorning = true;
                } else {
                    passedEvening = true;
                }
            }

            switch (nextTransition->test(dateTime)) {
            case KDarkLightTransition::Upcoming:
                break;
            case KDarkLightTransition::InProgress:
            case KDarkLightTransition::Passed:
                if (nextTransition->type() == KDarkLightTransition::Morning) {
                    passedMorning = true;
                } else {
                    passedEvening = true;
                }
            }

            setDaylight(passedMorning && !passedEvening);
            m_prev = DateTimes(previousTransition->startDateTime(), previousTransition->endDateTime());
            m_next = DateTimes(nextTransition->startDateTime(), nextTransition->endDateTime());
            break;
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

bool NightLightManager::daylight() const
{
    return m_daylight;
}

int NightLightManager::currentTargetTemperature() const
{
    if (!m_running) {
        return DEFAULT_DAY_TEMPERATURE;
    }

    if (m_mode == NightLightMode::Constant) {
        return m_nightTargetTemperature;
    }

    const QDateTime dateTime = QDateTime::currentDateTime();

    auto f = [this, dateTime](int target1, int target2) -> int {
        if (dateTime <= m_prev.first) {
            return target1;
        }
        if (dateTime >= m_prev.second) {
            return target2;
        }

        const double progress = double(m_prev.first.msecsTo(dateTime)) / m_prev.first.msecsTo(m_prev.second);
        return std::lerp(target1, target2, progress);
    };

    if (daylight()) {
        return f(m_nightTargetTemperature, m_dayTargetTemperature);
    } else {
        return f(m_dayTargetTemperature, m_nightTargetTemperature);
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
    if (m_currentTemperature == temperature) {
        return;
    }
    m_currentTemperature = temperature;
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
