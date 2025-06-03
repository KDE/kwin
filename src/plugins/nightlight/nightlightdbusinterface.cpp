/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2017 Roman Gilg <subdiff@gmail.com>
    SPDX-FileCopyrightText: 2024 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "nightlightdbusinterface.h"
#include "nightlightadaptor.h"
#include "nightlightmanager.h"

#include <QDBusMessage>

namespace KWin
{

static void announceChangedProperties(const QVariantMap &properties)
{
    QDBusMessage message = QDBusMessage::createSignal(
        QStringLiteral("/org/kde/KWin/NightLight"),
        QStringLiteral("org.freedesktop.DBus.Properties"),
        QStringLiteral("PropertiesChanged")
    );

    message.setArguments({
        QStringLiteral("org.kde.KWin.NightLight"),
        properties,
        QStringList(), // invalidated_properties
    });

    QDBusConnection::sessionBus().send(message);
}

NightLightDBusInterface::NightLightDBusInterface(NightLightManager *parent)
    : QObject(parent)
    , m_manager(parent)
    , m_inhibitorWatcher(new QDBusServiceWatcher(this))
{
    m_inhibitorWatcher->setConnection(QDBusConnection::sessionBus());
    m_inhibitorWatcher->setWatchMode(QDBusServiceWatcher::WatchForUnregistration);
    connect(m_inhibitorWatcher, &QDBusServiceWatcher::serviceUnregistered,
            this, &NightLightDBusInterface::removeInhibitorService);

    connect(m_manager, &NightLightManager::inhibitedChanged, this, [this] {
        announceChangedProperties({
            {QStringLiteral("inhibited"), isInhibited()},
        });
    });

    connect(m_manager, &NightLightManager::enabledChanged, this, [this] {
        announceChangedProperties({
            {QStringLiteral("enabled"), isEnabled()},
        });
    });

    connect(m_manager, &NightLightManager::runningChanged, this, [this] {
        announceChangedProperties({
            {QStringLiteral("running"), isRunning()},
        });
    });

    connect(m_manager, &NightLightManager::currentTemperatureChanged, this, [this] {
        announceChangedProperties({
            {QStringLiteral("currentTemperature"), currentTemperature()},
        });
    });

    connect(m_manager, &NightLightManager::targetTemperatureChanged, this, [this] {
        announceChangedProperties({
            {QStringLiteral("targetTemperature"), targetTemperature()},
        });
    });

    connect(m_manager, &NightLightManager::modeChanged, this, [this] {
        announceChangedProperties({
            {QStringLiteral("mode"), mode()},
        });
    });

    connect(m_manager, &NightLightManager::daylightChanged, this, [this] {
        announceChangedProperties({
            {QStringLiteral("daylight"), daylight()},
        });
    });

    connect(m_manager, &NightLightManager::previousTransitionTimingsChanged, this, [this] {
        announceChangedProperties({
            {QStringLiteral("previousTransitionDateTime"), previousTransitionDateTime()},
            {QStringLiteral("previousTransitionDuration"), previousTransitionDuration()},
        });
    });

    connect(m_manager, &NightLightManager::scheduledTransitionTimingsChanged, this, [this] {
        announceChangedProperties({
            {QStringLiteral("scheduledTransitionDateTime"), scheduledTransitionDateTime()},
            {QStringLiteral("scheduledTransitionDuration"), scheduledTransitionDuration()},
        });
    });

    new NightLightAdaptor(this);
    QDBusConnection::sessionBus().registerObject(QStringLiteral("/org/kde/KWin/NightLight"), this);
    QDBusConnection::sessionBus().registerService(QStringLiteral("org.kde.KWin.NightLight"));
}

NightLightDBusInterface::~NightLightDBusInterface()
{
    QDBusConnection::sessionBus().unregisterService(QStringLiteral("org.kde.KWin.NightLight"));
}

bool NightLightDBusInterface::isInhibited() const
{
    return m_manager->isInhibited();
}

bool NightLightDBusInterface::isEnabled() const
{
    return m_manager->isEnabled();
}

bool NightLightDBusInterface::isRunning() const
{
    return m_manager->isRunning();
}

bool NightLightDBusInterface::isAvailable() const
{
    return true; // TODO: Night color should register its own dbus service instead.
}

quint32 NightLightDBusInterface::currentTemperature() const
{
    return m_manager->currentTemperature();
}

quint32 NightLightDBusInterface::targetTemperature() const
{
    return m_manager->targetTemperature();
}

quint32 NightLightDBusInterface::mode() const
{
    return m_manager->mode();
}

bool NightLightDBusInterface::daylight() const
{
    return m_manager->daylight();
}

quint64 NightLightDBusInterface::previousTransitionDateTime() const
{
    const QDateTime dateTime = m_manager->previousTransitionDateTime();
    if (dateTime.isValid()) {
        return quint64(dateTime.toSecsSinceEpoch());
    }
    return 0;
}

quint32 NightLightDBusInterface::previousTransitionDuration() const
{
    return quint32(m_manager->previousTransitionDuration());
}

quint64 NightLightDBusInterface::scheduledTransitionDateTime() const
{
    const QDateTime dateTime = m_manager->scheduledTransitionDateTime();
    if (dateTime.isValid()) {
        return quint64(dateTime.toSecsSinceEpoch());
    }
    return 0;
}

quint32 NightLightDBusInterface::scheduledTransitionDuration() const
{
    return quint32(m_manager->scheduledTransitionDuration());
}

void NightLightDBusInterface::setLocation(double latitude, double longitude)
{
    // This method is left blank intentionally.
}

uint NightLightDBusInterface::inhibit()
{
    const QString serviceName = QDBusContext::message().service();

    if (!m_inhibitors.contains(serviceName)) {
        m_inhibitorWatcher->addWatchedService(serviceName);
    }

    m_inhibitors.insert(serviceName, ++m_lastInhibitionCookie);

    m_manager->inhibit();

    return m_lastInhibitionCookie;
}

void NightLightDBusInterface::uninhibit(uint cookie)
{
    const QString serviceName = QDBusContext::message().service();

    uninhibit(serviceName, cookie);
}

void NightLightDBusInterface::uninhibit(const QString &serviceName, uint cookie)
{
    const int removedCount = m_inhibitors.remove(serviceName, cookie);
    if (!removedCount) {
        return;
    }

    if (!m_inhibitors.contains(serviceName)) {
        m_inhibitorWatcher->removeWatchedService(serviceName);
    }

    m_manager->uninhibit();
}

void NightLightDBusInterface::removeInhibitorService(const QString &serviceName)
{
    const auto cookies = m_inhibitors.values(serviceName);
    for (const uint &cookie : cookies) {
        uninhibit(serviceName, cookie);
    }
}

void NightLightDBusInterface::preview(uint previewTemp)
{
    m_manager->preview(previewTemp);
}

void NightLightDBusInterface::stopPreview()
{
    m_manager->stopPreview();
}

}

#include "moc_nightlightdbusinterface.cpp"
