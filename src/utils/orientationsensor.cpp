/*
    SPDX-FileCopyrightText: 2023 Xaver Hugl <xaver.hugl@kde.org>
    SPDX-FileCopyrightText: 2026 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "utils/orientationsensor.h"
#include "dbusproperties_interface.h"
#include "sensorproxy_interface.h"
#include "utils/common.h"
#include "utils/dbus.h"

#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusMessage>
#include <QDBusPendingCall>
#include <QDBusServiceWatcher>

namespace KWin
{

static inline QString serviceName()
{
    return QStringLiteral("net.hadess.SensorProxy");
}

static inline QString objectPath()
{
    return QStringLiteral("/net/hadess/SensorProxy");
}

static inline QString interfaceName()
{
    return QStringLiteral("net.hadess.SensorProxy");
}

class OrientationSensorSubscription : public QObject
{
    Q_OBJECT

public:
    OrientationSensorSubscription();
    ~OrientationSensorSubscription();

    AccelerometerOrientation reading() const;
    void setReading(const QString &reading);

Q_SIGNALS:
    void readingChanged();

private Q_SLOTS:
    void onPropertiesChanged(const QString &interfaceName, const QVariantMap &properties);

private:
    AccelerometerOrientation m_reading = AccelerometerOrientation::Undefined;

    std::unique_ptr<OrgFreedesktopDBusPropertiesInterface> m_sensorProperties;
    std::unique_ptr<NetHadessSensorProxyInterface> m_sensorInterface;
};

OrientationSensorSubscription::OrientationSensorSubscription()
    : m_sensorProperties(std::make_unique<OrgFreedesktopDBusPropertiesInterface>(serviceName(), objectPath(), QDBusConnection::systemBus()))
    , m_sensorInterface(std::make_unique<NetHadessSensorProxyInterface>(serviceName(), objectPath(), QDBusConnection::systemBus()))
{
    connect(m_sensorProperties.get(), &OrgFreedesktopDBusPropertiesInterface::PropertiesChanged, this, &OrientationSensorSubscription::onPropertiesChanged);

    intoFuture<void>(m_sensorInterface->ClaimAccelerometer()).then([](const std::expected<void, QDBusError> &result) {
        if (!result) {
            qCWarning(KWIN_CORE) << "net.hadess.SensorProxy.ClaimAccelerometer() failed:" << result.error();
        }
    });

    intoFuture<QVariant>(m_sensorProperties->Get(m_sensorInterface->interface(), QStringLiteral("AccelerometerOrientation"))).then(this, [this](const std::expected<QVariant, QDBusError> &result) {
        if (result.has_value()) {
            setReading(result->toString());
        } else {
            qCWarning(KWIN_CORE) << "Failed to fetch net.hadess.SensorProxy.AccelerometerOrientation property:" << result.error();
        }
    });
}

OrientationSensorSubscription::~OrientationSensorSubscription()
{
    m_sensorInterface->ReleaseAccelerometer();
}

void OrientationSensorSubscription::onPropertiesChanged(const QString &interfaceName, const QVariantMap &properties)
{
    if (auto it = properties.find(QStringLiteral("AccelerometerOrientation")); it != properties.end()) {
        setReading(it->toString());
    }
}

AccelerometerOrientation OrientationSensorSubscription::reading() const
{
    return m_reading;
}

void OrientationSensorSubscription::setReading(const QString &reading)
{
    AccelerometerOrientation orientation = AccelerometerOrientation::Undefined;
    if (reading == QLatin1StringView("normal")) {
        orientation = AccelerometerOrientation::TopUp;
    } else if (reading == QLatin1StringView("bottom-up")) {
        orientation = AccelerometerOrientation::TopDown;
    } else if (reading == QLatin1StringView("left-up")) {
        orientation = AccelerometerOrientation::LeftUp;
    } else if (reading == QLatin1StringView("right-up")) {
        orientation = AccelerometerOrientation::RightUp;
    } else {
        qCWarning(KWIN_CORE) << "Unknown orientation sensor reading:" << reading;
    }

    if (m_reading != orientation) {
        m_reading = orientation;
        Q_EMIT readingChanged();
    }
}

OrientationSensor::OrientationSensor()
{
    m_watcher = std::make_unique<QDBusServiceWatcher>(serviceName(), QDBusConnection::systemBus(), QDBusServiceWatcher::WatchForOwnerChange);
    connect(m_watcher.get(), &QDBusServiceWatcher::serviceRegistered, this, &OrientationSensor::onServiceRegistered);
    connect(m_watcher.get(), &QDBusServiceWatcher::serviceUnregistered, this, &OrientationSensor::onServiceUnregistered);

    m_properties = std::make_unique<OrgFreedesktopDBusPropertiesInterface>(serviceName(), objectPath(), QDBusConnection::systemBus());
    connect(m_properties.get(), &OrgFreedesktopDBusPropertiesInterface::PropertiesChanged, this, &OrientationSensor::onPropertiesChanged);

    if (QDBusConnection::systemBus().interface()->isServiceRegistered(serviceName())) {
        onServiceRegistered();
    }
}

OrientationSensor::~OrientationSensor()
{
}

void OrientationSensor::onServiceRegistered()
{
    intoFuture<QVariant>(m_properties->Get(interfaceName(), QStringLiteral("HasAccelerometer"))).then(this, [this](const std::expected<QVariant, QDBusError> &value) {
        if (value.has_value()) {
            updateAvailable(value->toBool());
        } else {
            qCWarning(KWIN_CORE) << "Failed to fetch net.hadess.SensorProxy.HasAccelerometer property:" << value.error();
        }
    });

    if (m_enabled) {
        subscribe();
    }
}

void OrientationSensor::onServiceUnregistered()
{
    unsubscribe();

    m_orientation = AccelerometerOrientation::Undefined;

    updateAvailable(false);
}

void OrientationSensor::onPropertiesChanged(const QString &interfaceName, const QVariantMap &properties)
{
    if (auto it = properties.find(QStringLiteral("HasAccelerometer")); it != properties.end()) {
        updateAvailable(it->toBool());
    }
}

bool OrientationSensor::isAvailable() const
{
    return m_available;
}

AccelerometerOrientation OrientationSensor::reading() const
{
    return m_orientation;
}

bool OrientationSensor::isEnabled() const
{
    return m_enabled;
}

void OrientationSensor::setEnabled(bool enabled)
{
    if (m_enabled == enabled) {
        return;
    }

    m_enabled = enabled;

    if (enabled) {
        subscribe();
    } else {
        unsubscribe();
    }
}

void OrientationSensor::subscribe()
{
    if (m_subscription) {
        return;
    }

    if (!QDBusConnection::systemBus().interface()->isServiceRegistered(serviceName())) {
        return;
    }

    m_subscription = std::make_unique<OrientationSensorSubscription>();
    connect(m_subscription.get(), &OrientationSensorSubscription::readingChanged, this, [this]() {
        updateOrientation(m_subscription->reading());
    });

    updateOrientation(m_subscription->reading());
}

void OrientationSensor::unsubscribe()
{
    m_subscription.reset();
}

void OrientationSensor::updateAvailable(bool available)
{
    if (m_available != available) {
        m_available = available;
        Q_EMIT availableChanged();
    }
}

void OrientationSensor::updateOrientation(AccelerometerOrientation value)
{
    if (m_orientation != value) {
        m_orientation = value;
        Q_EMIT readingReceived();
    }
}

} // namespace KWin

#include "moc_orientationsensor.cpp"
#include "orientationsensor.moc"
