/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2017 Roman Gilg <subdiff@gmail.com>
    SPDX-FileCopyrightText: 2024 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QDBusContext>
#include <QDBusServiceWatcher>
#include <QObject>

namespace KWin
{

class NightLightManager;

class NightLightDBusInterface : public QObject, public QDBusContext
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.kde.KWin.NightLight")
    Q_PROPERTY(bool inhibited READ isInhibited)
    Q_PROPERTY(bool enabled READ isEnabled)
    Q_PROPERTY(bool running READ isRunning)
    Q_PROPERTY(bool available READ isAvailable)
    Q_PROPERTY(quint32 currentTemperature READ currentTemperature)
    Q_PROPERTY(quint32 targetTemperature READ targetTemperature)
    Q_PROPERTY(quint32 mode READ mode)
    Q_PROPERTY(bool daylight READ daylight)
    Q_PROPERTY(quint64 previousTransitionDateTime READ previousTransitionDateTime)
    Q_PROPERTY(quint32 previousTransitionDuration READ previousTransitionDuration)
    Q_PROPERTY(quint64 scheduledTransitionDateTime READ scheduledTransitionDateTime)
    Q_PROPERTY(quint32 scheduledTransitionDuration READ scheduledTransitionDuration)

public:
    explicit NightLightDBusInterface(NightLightManager *parent);
    ~NightLightDBusInterface() override;

    bool isInhibited() const;
    bool isEnabled() const;
    bool isRunning() const;
    bool isAvailable() const;
    quint32 currentTemperature() const;
    quint32 targetTemperature() const;
    quint32 mode() const;
    bool daylight() const;
    quint64 previousTransitionDateTime() const;
    quint32 previousTransitionDuration() const;
    quint64 scheduledTransitionDateTime() const;
    quint32 scheduledTransitionDuration() const;

public Q_SLOTS:
    /**
     * @brief Temporarily blocks Night Light.
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

    NightLightManager *m_manager;
    QDBusServiceWatcher *m_inhibitorWatcher;
    QMultiHash<QString, uint> m_inhibitors;
    uint m_lastInhibitionCookie = 0;
};

}
