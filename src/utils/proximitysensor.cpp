/*
    SPDX-FileCopyrightText: 2026 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "utils/proximitysensor.h"
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

class ProximitySensorSubscription : public QObject
{
    Q_OBJECT

public:
    ProximitySensorSubscription();
    ~ProximitySensorSubscription();

    std::optional<bool> reading() const;
    void setReading(bool near);

Q_SIGNALS:
    void readingChanged();

private Q_SLOTS:
    void onPropertiesChanged(const QString &interfaceName, const QVariantMap &properties);

private:
    std::optional<bool> m_reading;

    std::unique_ptr<OrgFreedesktopDBusPropertiesInterface> m_sensorProperties;
    std::unique_ptr<NetHadessSensorProxyInterface> m_sensorInterface;
};

ProximitySensorSubscription::ProximitySensorSubscription()
    : m_sensorProperties(std::make_unique<OrgFreedesktopDBusPropertiesInterface>(serviceName(), objectPath(), QDBusConnection::systemBus()))
    , m_sensorInterface(std::make_unique<NetHadessSensorProxyInterface>(serviceName(), objectPath(), QDBusConnection::systemBus()))
{
    connect(m_sensorProperties.get(), &OrgFreedesktopDBusPropertiesInterface::PropertiesChanged, this, &ProximitySensorSubscription::onPropertiesChanged);

    intoFuture<void>(m_sensorInterface->ClaimProximity()).then([](const std::expected<void, QDBusError> &result) {
        if (!result) {
            qCWarning(KWIN_CORE) << "net.hadess.SensorProxy.ClaimProximity() failed:" << result.error();
        }
    });

    intoFuture<QVariant>(m_sensorProperties->Get(m_sensorInterface->interface(), QStringLiteral("ProximityNear"))).then(this, [this](const std::expected<QVariant, QDBusError> &result) {
        if (result.has_value()) {
            setReading(result->toBool());
        } else {
            qCWarning(KWIN_CORE) << "Failed to fetch net.hadess.SensorProxy.ProximityNear property:" << result.error();
        }
    });
}

ProximitySensorSubscription::~ProximitySensorSubscription()
{
    m_sensorInterface->ReleaseProximity();
}

void ProximitySensorSubscription::onPropertiesChanged(const QString &interfaceName, const QVariantMap &properties)
{
    if (auto it = properties.find(QStringLiteral("ProximityNear")); it != properties.end()) {
        setReading(it->toBool());
    }
}

std::optional<bool> ProximitySensorSubscription::reading() const
{
    return m_reading;
}

void ProximitySensorSubscription::setReading(bool reading)
{
    if (m_reading != reading) {
        m_reading = reading;
        Q_EMIT readingChanged();
    }
}

ProximitySensor::ProximitySensor()
{
    m_watcher = std::make_unique<QDBusServiceWatcher>(serviceName(), QDBusConnection::systemBus(), QDBusServiceWatcher::WatchForOwnerChange);
    connect(m_watcher.get(), &QDBusServiceWatcher::serviceRegistered, this, &ProximitySensor::onServiceRegistered);
    connect(m_watcher.get(), &QDBusServiceWatcher::serviceUnregistered, this, &ProximitySensor::onServiceUnregistered);

    if (QDBusConnection::systemBus().interface()->isServiceRegistered(serviceName())) {
        onServiceRegistered();
    }
}

ProximitySensor::~ProximitySensor()
{
}

void ProximitySensor::onServiceRegistered()
{
    if (m_enabled) {
        subscribe();
    }
}

void ProximitySensor::onServiceUnregistered()
{
    unsubscribe();

    m_reading.reset();
}

std::optional<bool> ProximitySensor::reading() const
{
    return m_reading;
}

bool ProximitySensor::isEnabled() const
{
    return m_enabled;
}

void ProximitySensor::setEnabled(bool enabled)
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

void ProximitySensor::subscribe()
{
    if (m_subscription) {
        return;
    }

    if (!QDBusConnection::systemBus().interface()->isServiceRegistered(serviceName())) {
        return;
    }

    m_subscription = std::make_unique<ProximitySensorSubscription>();
    connect(m_subscription.get(), &ProximitySensorSubscription::readingChanged, this, [this]() {
        updateReading(m_subscription->reading());
    });

    updateReading(m_subscription->reading());
}

void ProximitySensor::unsubscribe()
{
    m_subscription.reset();
}

void ProximitySensor::updateReading(std::optional<bool> near)
{
    if (m_reading != near) {
        m_reading = near;
        Q_EMIT readingReceived();
    }
}

} // namespace KWin

#include "moc_proximitysensor.cpp"
#include "proximitysensor.moc"
