/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2017 Roman Gilg <subdiff@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QObject>
#include <QtDBus>

namespace KWin
{

class NightColorManager;

class NightColorDBusInterface : public QObject, public QDBusContext
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
    explicit NightColorDBusInterface(NightColorManager *parent);
    ~NightColorDBusInterface() override;

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
    /**
     * @brief Previews a given temperature for a short time (15s).
     * @since 5.25
     */
    void preview(uint temperature);
    /**
     * @brief Stops an ongoing preview.
     * @since 5.25
     */
    void stopPreview();

private Q_SLOTS:
    void removeInhibitorService(const QString &serviceName);

private:
    void uninhibit(const QString &serviceName, uint cookie);

    NightColorManager *m_manager;
    QDBusServiceWatcher *m_inhibitorWatcher;
    QMultiHash<QString, uint> m_inhibitors;
    uint m_lastInhibitionCookie = 0;
};

}
