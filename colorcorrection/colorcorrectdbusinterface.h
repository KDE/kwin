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

public:
    explicit ColorCorrectDBusInterface(Manager *parent);
    ~ColorCorrectDBusInterface() override = default;

    bool isInhibited() const;

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
