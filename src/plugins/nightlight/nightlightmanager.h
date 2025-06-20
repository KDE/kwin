/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2017 Roman Gilg <subdiff@gmail.com>
    SPDX-FileCopyrightText: 2024 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "constants.h"
#include "plugin.h"

#include <KConfigWatcher>

#include <QDateTime>
#include <QObject>
#include <QPair>

class KDarkLightScheduleProvider;
class KSystemClockSkewNotifier;
class NightLightState;
class QTimer;

namespace KWin
{

class NightLightDBusInterface;

typedef QPair<QDateTime, QDateTime> DateTimes;

/**
 * This enum type is used to specify operation mode of the night light manager.
 */
enum NightLightMode {
    /**
     * Color temperature is computed based on the current position of the Sun.
     *
     * Location of the user is provided by Plasma.
     */
    Automatic,
    /**
     * Color temperature is computed based on the current position of the Sun.
     *
     * Location of the user is provided by themselves.
     */
    Location,
    /**
     * Color temperature is computed based on the current time.
     *
     * Sunrise and sunset times have to be specified by the user.
     */
    Timings,
    /**
     * Color temperature is constant thoughout the day.
     */
    Constant,
    DarkLight,
};

/**
 * The night light manager is a blue light filter similar to Redshift.
 *
 * There are four modes this manager can operate in: Automatic, Location, Timings,
 * and Constant. Both Automatic and Location modes derive screen color temperature
 * from the current position of the Sun, the only difference between two is how
 * coordinates of the user are specified. If the user is located near the North or
 * South pole, we can't compute correct position of the Sun, that's why we need
 * Timings and Constant mode.
 *
 * With the Timings mode, screen color temperature is computed based on the clock
 * time. The user needs to specify timings of the sunset and sunrise as well the
 * transition time.
 *
 * With the Constant mode, screen color temperature is always constant.
 */
class KWIN_EXPORT NightLightManager : public Plugin
{
    Q_OBJECT

public:
    explicit NightLightManager();
    ~NightLightManager() override;

    void autoLocationUpdate(double latitude, double longitude);

    /**
     * Toggles the active state of the filter.
     *
     * A quick transition will be started if the difference between current screen
     * color temperature and target screen color temperature is too large. Target
     * temperature is defined in context of the new active state.
     *
     * If the filter becomes inactive after calling this method, the target color
     * temperature is 6500 K.
     *
     * If the filter becomes active after calling this method, the target screen
     * color temperature is defined by the current operation mode.
     *
     * Note that this method is a no-op if the underlying platform doesn't support
     * adjusting gamma ramps.
     */
    void toggle();

    /**
     * Returns @c true if the night light manager is blocked; otherwise @c false.
     */
    bool isInhibited() const;

    /**
     * Temporarily blocks the night light manager.
     *
     * After calling this method, the screen color temperature will be reverted
     * back to 6500C. When you're done, call uninhibit() method.
     */
    void inhibit();

    /**
     * Attempts to unblock the night light manager.
     */
    void uninhibit();

    /**
     * Returns @c true if Night Light is enabled; otherwise @c false.
     */
    bool isEnabled() const;

    /**
     * Returns @c true if Night Light is currently running; otherwise @c false.
     */
    bool isRunning() const;

    /**
     * Returns the current screen color temperature.
     */
    int currentTemperature() const;

    /**
     * Returns the target screen color temperature.
     */
    int targetTemperature() const;

    /**
     * Returns the mode in which Night Light is operating.
     */
    NightLightMode mode() const;

    /**
     * Returns whether Night Light is currently on day time.
     */
    bool daylight() const;

    /**
     * Returns the datetime that specifies when the previous screen color temperature transition
     * had started. Notice that when Night Light operates in the Constant mode, the returned date
     * time object is not valid.
     */
    QDateTime previousTransitionDateTime() const;

    /**
     * Returns the duration of the previous screen color temperature transition, in milliseconds.
     */
    qint64 previousTransitionDuration() const;

    /**
     * Returns the datetime that specifies when the next screen color temperature transition will
     * start. Notice that when Night Light operates in the Constant mode, the returned date time
     * object is not valid.
     */
    QDateTime scheduledTransitionDateTime() const;

    /**
     * Returns the duration of the next screen color temperature transition, in milliseconds.
     */
    qint64 scheduledTransitionDuration() const;

    /**
     * Applies new night light settings.
     */
    void reconfigure();

    /**
     * Previews a given temperature for a short time (15s).
     */
    void preview(uint previewTemperature);

    /**
     * Stops an ongoing preview.
     * Has no effect if there is currently no preview.
     */
    void stopPreview();

public Q_SLOTS:
    void quickAdjust(int targetTemperature);

Q_SIGNALS:
    /**
     * Emitted whenever the night light manager is blocked or unblocked.
     */
    void inhibitedChanged();

    /**
     * Emitted whenever the night light manager is enabled or disabled.
     */
    void enabledChanged();

    /**
     * Emitted whenever the night light manager starts or stops running.
     */
    void runningChanged();

    /**
     * Emitted whenever the current screen color temperature has changed.
     */
    void currentTemperatureChanged();

    /**
     * Emitted whenever the target screen color temperature has changed.
     */
    void targetTemperatureChanged();

    /**
     * Emitted whenver the operation mode has changed.
     */
    void modeChanged();

    /**
     * Emitted whenver night light has switched between day and night time.
     */
    void daylightChanged();

    /**
     * Emitted whenever the timings of the previous color temperature transition have changed.
     */
    void previousTransitionTimingsChanged();

    /**
     * Emitted whenever the timings of the next color temperature transition have changed.
     */
    void scheduledTransitionTimingsChanged();

private:
    void readConfig();
    void hardReset();
    void slowUpdate(int targetTemperature);
    void resetAllTimers();
    int currentTargetTemperature() const;
    void cancelAllTimers();
    /**
     * Quick shift on manual change to current target Temperature
     */
    void resetQuickAdjustTimer(int targetTemperature);
    /**
     * Slow shift to daytime target Temperature
     */
    void resetSlowUpdateTimers();

    void updateTargetTemperature();
    void updateTransitionTimings(const QDateTime &dateTime);

    void commitGammaRamps(int temperature);

    void setEnabled(bool enabled);
    void setRunning(bool running);
    void setCurrentTemperature(int temperature);
    void setMode(NightLightMode mode);
    void setDaylight(bool daylight);

    NightLightDBusInterface *m_iface;
    KSystemClockSkewNotifier *m_skewNotifier;

    std::unique_ptr<NightLightState> m_stateConfig;
    std::unique_ptr<KDarkLightScheduleProvider> m_darkLightScheduler;

    // Specifies whether Night Light is enabled.
    bool m_active = false;

    // Specifies whether Night Light is currently running.
    bool m_running = false;

    // Specifies whether Night Light is inhibited globally.
    bool m_isGloballyInhibited = false;

    NightLightMode m_mode = NightLightMode::Automatic;

    // the previous and next sunrise/sunset intervals - in UTC time
    DateTimes m_prev = DateTimes();
    DateTimes m_next = DateTimes();

    // whether it is currently day or night
    bool m_daylight = true;

    // manual times from config
    QTime m_morning = QTime(6, 0);
    QTime m_evening = QTime(18, 0);
    int m_transitionDuration = DEFAULT_TRANSITION_DURATION; // in milliseconds

    // auto location provided by work space
    double m_latitudeAuto;
    double m_longitudeAuto;
    // manual location from config
    double m_latitudeFixed;
    double m_longitudeFixed;

    std::unique_ptr<QTimer> m_slowUpdateStartTimer;
    std::unique_ptr<QTimer> m_slowUpdateTimer;
    std::unique_ptr<QTimer> m_quickAdjustTimer;
    std::unique_ptr<QTimer> m_previewTimer;

    int m_currentTemperature = DEFAULT_DAY_TEMPERATURE;
    int m_targetTemperature = DEFAULT_DAY_TEMPERATURE;
    int m_dayTargetTemperature = DEFAULT_DAY_TEMPERATURE;
    int m_nightTargetTemperature = DEFAULT_NIGHT_TEMPERATURE;

    int m_inhibitReferenceCount = 0;
    KConfigWatcher::Ptr m_configWatcher;
};

} // namespace KWin
