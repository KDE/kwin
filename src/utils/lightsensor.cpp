/*
    SPDX-FileCopyrightText: 2025 Xaver Hugl <xaver.hugl@kde.org>
    SPDX-FileCopyrightText: 2026 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "utils/lightsensor.h"
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

class LightSensorSubscription : public QObject
{
    Q_OBJECT

public:
    LightSensorSubscription();
    ~LightSensorSubscription();

    std::optional<QString> unit() const;
    void setUnit(const QString &unit);

    std::optional<double> reading() const;
    void setReading(double reading);

Q_SIGNALS:
    void readingChanged();

private Q_SLOTS:
    void onPropertiesChanged(const QString &interfaceName, const QVariantMap &properties);

private:
    std::unique_ptr<OrgFreedesktopDBusPropertiesInterface> m_sensorProperties;
    std::unique_ptr<NetHadessSensorProxyInterface> m_sensorInterface;
    std::optional<QString> m_unit;
    std::optional<double> m_reading;
};

LightSensorSubscription::LightSensorSubscription()
    : m_sensorProperties(std::make_unique<OrgFreedesktopDBusPropertiesInterface>(serviceName(), objectPath(), QDBusConnection::systemBus()))
    , m_sensorInterface(std::make_unique<NetHadessSensorProxyInterface>(serviceName(), objectPath(), QDBusConnection::systemBus()))
{
    connect(m_sensorProperties.get(), &OrgFreedesktopDBusPropertiesInterface::PropertiesChanged, this, &LightSensorSubscription::onPropertiesChanged);

    intoFuture<void>(m_sensorInterface->ClaimLight()).then([](const std::expected<void, QDBusError> &result) {
        if (!result) {
            qCWarning(KWIN_CORE) << "net.hadess.SensorProxy.ClaimLight() failed:" << result.error();
        }
    });

    intoFuture<QVariant>(m_sensorProperties->Get(m_sensorInterface->interface(), QStringLiteral("LightLevelUnit"))).then(this, [this](const std::expected<QVariant, QDBusError> &result) {
        if (result.has_value()) {
            setUnit(result->toString());
        } else {
            qCWarning(KWIN_CORE) << "Failed to fetch net.hadess.SensorProxy.LightLevelUnit property:" << result.error();
        }
    });

    intoFuture<QVariant>(m_sensorProperties->Get(m_sensorInterface->interface(), QStringLiteral("LightLevel"))).then(this, [this](const std::expected<QVariant, QDBusError> &result) {
        if (result.has_value()) {
            setReading(result->toDouble());
        } else {
            qCWarning(KWIN_CORE) << "Failed to fetch net.hadess.SensorProxy.LightLevel property:" << result.error();
        }
    });
}

LightSensorSubscription::~LightSensorSubscription()
{
    m_sensorInterface->ReleaseLight();
}

void LightSensorSubscription::onPropertiesChanged(const QString &interfaceName, const QVariantMap &properties)
{
    if (auto it = properties.find(QStringLiteral("LightLevelUnit")); it != properties.end()) {
        setUnit(it->toString());
    }

    if (auto it = properties.find(QStringLiteral("LightLevel")); it != properties.end()) {
        setReading(it->toDouble());
    }
}

std::optional<QString> LightSensorSubscription::unit() const
{
    return m_unit;
}

void LightSensorSubscription::setUnit(const QString &unit)
{
    m_unit = unit;
}

std::optional<double> LightSensorSubscription::reading() const
{
    return m_reading;
}

void LightSensorSubscription::setReading(double reading)
{
    if (m_reading != reading) {
        m_reading = reading;
        Q_EMIT readingChanged();
    }
}

LightSensor::LightSensor()
{
    m_watcher = std::make_unique<QDBusServiceWatcher>(serviceName(), QDBusConnection::systemBus(), QDBusServiceWatcher::WatchForOwnerChange);
    connect(m_watcher.get(), &QDBusServiceWatcher::serviceRegistered, this, &LightSensor::onServiceRegistered);
    connect(m_watcher.get(), &QDBusServiceWatcher::serviceUnregistered, this, &LightSensor::onServiceUnregistered);

    m_properties = std::make_unique<OrgFreedesktopDBusPropertiesInterface>(serviceName(), objectPath(), QDBusConnection::systemBus());
    connect(m_properties.get(), &OrgFreedesktopDBusPropertiesInterface::PropertiesChanged, this, &LightSensor::onPropertiesChanged);

    if (QDBusConnection::systemBus().interface()->isServiceRegistered(serviceName())) {
        onServiceRegistered();
    }
}

LightSensor::~LightSensor()
{
}

void LightSensor::onServiceRegistered()
{
    intoFuture<QVariant>(m_properties->Get(interfaceName(), QStringLiteral("HasAmbientLight"))).then(this, [this](const std::expected<QVariant, QDBusError> &value) {
        if (value.has_value()) {
            updateAvailable(value->toBool());
        } else {
            qCWarning(KWIN_CORE) << "Failed to fetch net.hadess.SensorProxy.HasAmbientLight property:" << value.error();
        }
    });

    if (m_enabled) {
        subscribe();
    }
}

void LightSensor::onServiceUnregistered()
{
    unsubscribe();

    m_unit.reset();
    m_reading.reset();

    updateAvailable(false);
}

void LightSensor::onPropertiesChanged(const QString &interfaceName, const QVariantMap &properties)
{
    if (auto it = properties.find(QStringLiteral("HasAmbientLight")); it != properties.end()) {
        updateAvailable(it->toBool());
    }
}

bool LightSensor::isAvailable() const
{
    return m_available;
}

std::optional<double> LightSensor::reading() const
{
    return m_reading;
}

bool LightSensor::isEnabled() const
{
    return m_enabled;
}

void LightSensor::setEnabled(bool enabled)
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

void LightSensor::subscribe()
{
    if (m_subscription) {
        return;
    }

    if (!QDBusConnection::systemBus().interface()->isServiceRegistered(serviceName())) {
        return;
    }

    m_subscription = std::make_unique<LightSensorSubscription>();
    connect(m_subscription.get(), &LightSensorSubscription::readingChanged, this, [this]() {
        updateLevelUnit(m_subscription->unit());
        updateLevel(m_subscription->reading());
    });

    updateLevelUnit(m_subscription->unit());
    updateLevel(m_subscription->reading());
}

void LightSensor::unsubscribe()
{
    m_subscription.reset();
}

void LightSensor::updateAvailable(bool available)
{
    if (m_available != available) {
        m_available = available;
        Q_EMIT availableChanged();
    }
}

void LightSensor::updateLevelUnit(std::optional<QString> unit)
{
    m_unit = unit;
}

void LightSensor::updateLevel(std::optional<double> value)
{
    if (m_unit != QStringLiteral("lux")) {
        m_reading.reset();
        return;
    }

    if (m_reading != value) {
        m_reading = value;
        Q_EMIT readingReceived();
    }
}

} // namespace KWin

#include "lightsensor.moc"
#include "moc_lightsensor.cpp"
