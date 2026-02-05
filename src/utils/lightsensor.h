/*
    SPDX-FileCopyrightText: 2025 Xaver Hugl <xaver.hugl@kde.org>
    SPDX-FileCopyrightText: 2026 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QObject>

class OrgFreedesktopDBusPropertiesInterface;
class QDBusServiceWatcher;

namespace KWin
{

class LightSensorSubscription;

class LightSensor : public QObject
{
    Q_OBJECT

public:
    LightSensor();
    ~LightSensor() override;

    bool isAvailable() const;

    std::optional<double> reading() const;

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
    void onPropertiesChanged(const QString &interfaceName, const QVariantMap &properties);

    void updateAvailable(bool available);
    void updateLevel(std::optional<double> value);
    void updateLevelUnit(std::optional<QString> unit);

    bool m_available = false;
    bool m_enabled = false;

    std::unique_ptr<QDBusServiceWatcher> m_watcher;
    std::unique_ptr<OrgFreedesktopDBusPropertiesInterface> m_properties;
    std::unique_ptr<LightSensorSubscription> m_subscription;
    std::optional<QString> m_unit;
    std::optional<double> m_reading;
};

} // namespace KWin
