/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2017 Roman Gilg <subdiff@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
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

class ClockSkewNotifier;
class Workspace;

namespace ColorCorrect
{

typedef QPair<QDateTime,QDateTime> DateTimes;
typedef QPair<QTime,QTime> Times;

class ColorCorrectDBusInterface;

/**
 * This enum type is used to specify operation mode of the night color manager.
 */
enum NightColorMode {
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
};

/**
 * The night color manager is a blue light filter similar to Redshift.
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
     */
    QHash<QString, QVariant> info() const;
    /**
     * Change configuration
     * @see info
     * @since 5.12
     */
    bool changeConfiguration(QHash<QString, QVariant> data);
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
     * Returns @c true if the night color manager is blocked; otherwise @c false.
     */
    bool isInhibited() const;

    /**
     * Temporarily blocks the night color manager.
     *
     * After calling this method, the screen color temperature will be reverted
     * back to 6500C. When you're done, call uninhibit() method.
     */
    void inhibit();

    /**
     * Attempts to unblock the night color manager.
     */
    void uninhibit();

    /**
     * Returns @c true if Night Color is enabled; otherwise @c false.
     */
    bool isEnabled() const;

    /**
     * Returns @c true if Night Color is currently running; otherwise @c false.
     */
    bool isRunning() const;

    /**
     * Returns @c true if Night Color is supported by platform; otherwise @c false.
     */
    bool isAvailable() const;

    /**
     * Returns the current screen color temperature.
     */
    int currentTemperature() const;

    /**
     * Returns the target screen color temperature.
     */
    int targetTemperature() const;

    /**
     * Returns the mode in which Night Color is operating.
     */
    NightColorMode mode() const;

    /**
     * Returns the datetime that specifies when the previous screen color temperature transition
     * had started. Notice that when Night Color operates in the Constant mode, the returned date
     * time object is not valid.
     */
    QDateTime previousTransitionDateTime() const;

    /**
     * Returns the duration of the previous screen color temperature transition, in milliseconds.
     */
    qint64 previousTransitionDuration() const;

    /**
     * Returns the datetime that specifies when the next screen color temperature transition will
     * start. Notice that when Night Color operates in the Constant mode, the returned date time
     * object is not valid.
     */
    QDateTime scheduledTransitionDateTime() const;

    /**
     * Returns the duration of the next screen color temperature transition, in milliseconds.
     */
    qint64 scheduledTransitionDuration() const;

    // for auto tests
    void reparseConfigAndReset();

public Q_SLOTS:
    void resetSlowUpdateStartTimer();
    void quickAdjust();

Q_SIGNALS:
    void configChange(QHash<QString, QVariant> data);

    /**
     * Emitted whenever the night color manager is blocked or unblocked.
     */
    void inhibitedChanged();

    /**
     * Emitted whenever the night color manager is enabled or disabled.
     */
    void enabledChanged();

    /**
     * Emitted whenever the night color manager starts or stops running.
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
     * Emitted whenever the timings of the previous color temperature transition have changed.
     */
    void previousTransitionTimingsChanged();

    /**
     * Emitted whenever the timings of the next color temperature transition have changed.
     */
    void scheduledTransitionTimingsChanged();

private:
    void initShortcuts();
    void readConfig();
    void hardReset();
    void slowUpdate(int targetTemp);
    void resetAllTimers();
    int currentTargetTemp() const;
    void cancelAllTimers();
    /**
     * Quick shift on manual change to current target Temperature
     */
    void resetQuickAdjustTimer();
    /**
     * Slow shift to daytime target Temperature
     */
    void resetSlowUpdateTimer();

    void updateTargetTemperature();
    void updateTransitionTimings(bool force);
    DateTimes getSunTimings(const QDateTime &dateTime, double latitude, double longitude, bool morning) const;
    bool checkAutomaticSunTimings() const;
    bool daylight() const;

    void commitGammaRamps(int temperature);

    void setEnabled(bool enabled);
    void setRunning(bool running);
    void setCurrentTemperature(int temperature);
    void setMode(NightColorMode mode);

    ColorCorrectDBusInterface *m_iface;
    ClockSkewNotifier *m_skewNotifier;

    // Specifies whether Night Color is enabled.
    bool m_active = false;

    // Specifies whether Night Color is currently running.
    bool m_running = false;

    // Specifies whether Night Color is inhibited globally.
    bool m_isGloballyInhibited = false;

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
    int m_targetTemperature = NEUTRAL_TEMPERATURE;
    int m_dayTargetTemp = NEUTRAL_TEMPERATURE;
    int m_nightTargetTemp = DEFAULT_NIGHT_TEMPERATURE;

    int m_failedCommitAttempts = 0;
    int m_inhibitReferenceCount = 0;

    // The Workspace class needs to call initShortcuts during initialization.
    friend class KWin::Workspace;
};

}
}

#endif // KWIN_COLORCORRECT_MANAGER_H
