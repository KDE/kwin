/*
    SPDX-FileCopyrightText: 2023 Xaver Hugl <xaver.hugl@kde.org>
    SPDX-FileCopyrightText: 2026 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QObject>

class OrgFreedesktopDBusPropertiesInterface;
class QDBusServiceWatcher;

namespace KWin
{

enum class AccelerometerOrientation {
    Undefined,
    TopUp,
    TopDown,
    LeftUp,
    RightUp,
    FaceUp,
    FaceDown,
};

class OrientationSensorSubscription;

class OrientationSensor : public QObject
{
    Q_OBJECT

public:
    OrientationSensor();
    ~OrientationSensor() override;

    bool isAvailable() const;

    AccelerometerOrientation reading() const;

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
    void updateOrientation(AccelerometerOrientation orientation);

    bool m_available = false;
    bool m_enabled = false;

    std::unique_ptr<QDBusServiceWatcher> m_watcher;
    std::unique_ptr<OrgFreedesktopDBusPropertiesInterface> m_properties;
    std::unique_ptr<OrientationSensorSubscription> m_subscription;
    AccelerometerOrientation m_orientation = AccelerometerOrientation::Undefined;
};

} // namespace KWin
