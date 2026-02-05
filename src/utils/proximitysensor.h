/*
    SPDX-FileCopyrightText: 2026 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QObject>

class QDBusServiceWatcher;

namespace KWin
{

class ProximitySensorSubscription;

class ProximitySensor : public QObject
{
    Q_OBJECT

public:
    ProximitySensor();
    ~ProximitySensor() override;

    std::optional<bool> reading() const;

    bool isEnabled() const;
    void setEnabled(bool enabled);

Q_SIGNALS:
    void availableChanged();
    void readingReceived();

private:
    void subscribe();
    void unsubscribe();

    void onServiceRegistered();
    void onServiceUnregistered();

    void updateReading(std::optional<bool> near);

    bool m_enabled = false;

    std::unique_ptr<QDBusServiceWatcher> m_watcher;
    std::unique_ptr<ProximitySensorSubscription> m_subscription;
    std::optional<bool> m_reading;
};

} // namespace KWin
