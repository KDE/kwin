/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2017 Roman Gilg <subdiff@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef KWIN_NIGHTCOLOR_DBUS_INTERFACE_H
#define KWIN_NIGHTCOLOR_DBUS_INTERFACE_H

#include <QObject>
#include <QtDBus>

namespace KWin
{

namespace ColorCorrect
{

class Manager;

class ColorCorrectDBusInterface : public QObject, public QDBusContext
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.kde.kwin.ColorCorrect")
    Q_PROPERTY(bool inhibited READ isInhibited)
    Q_PROPERTY(bool enabled READ isEnabled)
    Q_PROPERTY(bool running READ isRunning)
    Q_PROPERTY(bool available READ isAvailable)
    Q_PROPERTY(int currentTemperature READ currentTemperature)
    Q_PROPERTY(int targetTemperature READ targetTemperature)
    Q_PROPERTY(int mode READ mode)
    Q_PROPERTY(quint64 previousTransitionDateTime READ previousTransitionDateTime)
    Q_PROPERTY(quint32 previousTransitionDuration READ previousTransitionDuration)
    Q_PROPERTY(quint64 scheduledTransitionDateTime READ scheduledTransitionDateTime)
    Q_PROPERTY(quint32 scheduledTransitionDuration READ scheduledTransitionDuration)

public:
    explicit ColorCorrectDBusInterface(Manager *parent);
    ~ColorCorrectDBusInterface() override = default;

    bool isInhibited() const;
    bool isEnabled() const;
    bool isRunning() const;
    bool isAvailable() const;
    int currentTemperature() const;
    int targetTemperature() const;
    int mode() const;
    quint64 previousTransitionDateTime() const;
    quint32 previousTransitionDuration() const;
    quint64 scheduledTransitionDateTime() const;
    quint32 scheduledTransitionDuration() const;

public Q_SLOTS:
    /**
     * @brief Gives information about the current state of Night Color.
     *
     * The returned variant hash has always the fields:
     * - ActiveEnabled
     * - Active
     * - Mode
     * - NightTemperatureEnabled
     * - NightTemperature
     * - Running
     * - CurrentColorTemperature
     * - LatitudeAuto
     * - LongitudeAuto
     * - LocationEnabled
     * - LatitudeFixed
     * - LongitudeFixed
     * - TimingsEnabled
     * - MorningBeginFixed
     * - EveningBeginFixed
     * - TransitionTime
     *
     * @return QHash<QString, QVariant>
     * @see nightColorConfigChange
     * @see signalNightColorConfigChange
     * @since 5.12
     */
    QHash<QString, QVariant> nightColorInfo();
    /**
     * @brief Allows changing the Night Color configuration.
     *
     * The provided variant hash can have the following fields:
     * - Active
     * - Mode
     * - NightTemperature
     * - LatitudeAuto
     * - LongitudeAuto
     * - LatitudeFixed
     * - LongitudeFixed
     * - MorningBeginFixed
     * - EveningBeginFixed
     * - TransitionTime
     *
     * It returns true if the configuration change was successful, otherwise false.
     * A change request for the location or timings needs to provide all relevant fields at the same time
     * to be successful. Otherwise the whole change request will get ignored. A change request will be ignored
     * as a whole as well, if one of the provided information has been sent in a wrong format.
     *
     * @return bool
     * @see nightColorInfo
     * @see signalNightColorConfigChange
     * @since 5.12
     */
    bool setNightColorConfig(QHash<QString, QVariant> data);
    /**
     * @brief For receiving auto location updates, primarily through the KDE Daemon
     * @return void
     * @since 5.12
     */
    void nightColorAutoLocationUpdate(double latitude, double longitude);
    /**
     * @brief Temporarily blocks Night Color.
     * @since 5.18
     */
    uint inhibit();
    /**
     * @brief Cancels the previous call to inhibit().
     * @since 5.18
     */
    void uninhibit(uint cookie);

Q_SIGNALS:
    /**
     * @brief Emits that the Night Color configuration has been changed.
     *
     * The provided variant hash provides the same fields as nightColorInfo
     *
     * @return void
     * @see nightColorInfo
     * @see nightColorConfigChange
     * @since 5.12
     */
    void nightColorConfigChanged(QHash<QString, QVariant> data);

private Q_SLOTS:
    void removeInhibitorService(const QString &serviceName);

private:
    void uninhibit(const QString &serviceName, uint cookie);

    Manager *m_manager;
    QDBusServiceWatcher *m_inhibitorWatcher;
    QMultiHash<QString, uint> m_inhibitors;
    uint m_lastInhibitionCookie = 0;
};

}

}

#endif // KWIN_NIGHTCOLOR_DBUS_INTERFACE_H
