/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2017 Roman Gilg <subdiff@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "nightcolordbusinterface.h"
#include "colorcorrectadaptor.h"
#include "nightcolormanager.h"

#include <QDBusMessage>

namespace KWin
{

NightColorDBusInterface::NightColorDBusInterface(NightColorManager *parent)
    : QObject(parent)
    , m_manager(parent)
    , m_inhibitorWatcher(new QDBusServiceWatcher(this))
{
    m_inhibitorWatcher->setConnection(QDBusConnection::sessionBus());
    m_inhibitorWatcher->setWatchMode(QDBusServiceWatcher::WatchForUnregistration);
    connect(m_inhibitorWatcher, &QDBusServiceWatcher::serviceUnregistered,
            this, &NightColorDBusInterface::removeInhibitorService);

    connect(m_manager, &NightColorManager::inhibitedChanged, this, [this] {
        QVariantMap changedProperties;
        changedProperties.insert(QStringLiteral("inhibited"), m_manager->isInhibited());

        QDBusMessage message = QDBusMessage::createSignal(
            QStringLiteral("/ColorCorrect"),
            QStringLiteral("org.freedesktop.DBus.Properties"),
            QStringLiteral("PropertiesChanged"));

        message.setArguments({
            QStringLiteral("org.kde.kwin.ColorCorrect"),
            changedProperties,
            QStringList(), // invalidated_properties
        });

        QDBusConnection::sessionBus().send(message);
    });

    connect(m_manager, &NightColorManager::enabledChanged, this, [this] {
        QVariantMap changedProperties;
        changedProperties.insert(QStringLiteral("enabled"), m_manager->isEnabled());

        QDBusMessage message = QDBusMessage::createSignal(
            QStringLiteral("/ColorCorrect"),
            QStringLiteral("org.freedesktop.DBus.Properties"),
            QStringLiteral("PropertiesChanged"));

        message.setArguments({
            QStringLiteral("org.kde.kwin.ColorCorrect"),
            changedProperties,
            QStringList(), // invalidated_properties
        });

        QDBusConnection::sessionBus().send(message);
    });

    connect(m_manager, &NightColorManager::runningChanged, this, [this] {
        QVariantMap changedProperties;
        changedProperties.insert(QStringLiteral("running"), m_manager->isRunning());

        QDBusMessage message = QDBusMessage::createSignal(
            QStringLiteral("/ColorCorrect"),
            QStringLiteral("org.freedesktop.DBus.Properties"),
            QStringLiteral("PropertiesChanged"));

        message.setArguments({
            QStringLiteral("org.kde.kwin.ColorCorrect"),
            changedProperties,
            QStringList(), // invalidated_properties
        });

        QDBusConnection::sessionBus().send(message);
    });

    connect(m_manager, &NightColorManager::currentTemperatureChanged, this, [this] {
        QVariantMap changedProperties;
        changedProperties.insert(QStringLiteral("currentTemperature"), m_manager->currentTemperature());

        QDBusMessage message = QDBusMessage::createSignal(
            QStringLiteral("/ColorCorrect"),
            QStringLiteral("org.freedesktop.DBus.Properties"),
            QStringLiteral("PropertiesChanged"));

        message.setArguments({
            QStringLiteral("org.kde.kwin.ColorCorrect"),
            changedProperties,
            QStringList(), // invalidated_properties
        });

        QDBusConnection::sessionBus().send(message);
    });

    connect(m_manager, &NightColorManager::targetTemperatureChanged, this, [this] {
        QVariantMap changedProperties;
        changedProperties.insert(QStringLiteral("targetTemperature"), m_manager->targetTemperature());

        QDBusMessage message = QDBusMessage::createSignal(
            QStringLiteral("/ColorCorrect"),
            QStringLiteral("org.freedesktop.DBus.Properties"),
            QStringLiteral("PropertiesChanged"));

        message.setArguments({
            QStringLiteral("org.kde.kwin.ColorCorrect"),
            changedProperties,
            QStringList(), // invalidated_properties
        });

        QDBusConnection::sessionBus().send(message);
    });

    connect(m_manager, &NightColorManager::modeChanged, this, [this] {
        QVariantMap changedProperties;
        changedProperties.insert(QStringLiteral("mode"), uint(m_manager->mode()));

        QDBusMessage message = QDBusMessage::createSignal(
            QStringLiteral("/ColorCorrect"),
            QStringLiteral("org.freedesktop.DBus.Properties"),
            QStringLiteral("PropertiesChanged"));

        message.setArguments({
            QStringLiteral("org.kde.kwin.ColorCorrect"),
            changedProperties,
            QStringList(), // invalidated_properties
        });

        QDBusConnection::sessionBus().send(message);
    });

    connect(m_manager, &NightColorManager::previousTransitionTimingsChanged, this, [this] {
        QVariantMap changedProperties;
        changedProperties.insert(QStringLiteral("previousTransitionDateTime"), previousTransitionDateTime());
        changedProperties.insert(QStringLiteral("previousTransitionDuration"), previousTransitionDuration());

        QDBusMessage message = QDBusMessage::createSignal(
            QStringLiteral("/ColorCorrect"),
            QStringLiteral("org.freedesktop.DBus.Properties"),
            QStringLiteral("PropertiesChanged"));

        message.setArguments({
            QStringLiteral("org.kde.kwin.ColorCorrect"),
            changedProperties,
            QStringList(), // invalidated_properties
        });

        QDBusConnection::sessionBus().send(message);
    });

    connect(m_manager, &NightColorManager::scheduledTransitionTimingsChanged, this, [this] {
        QVariantMap changedProperties;
        changedProperties.insert(QStringLiteral("scheduledTransitionDateTime"), scheduledTransitionDateTime());
        changedProperties.insert(QStringLiteral("scheduledTransitionDuration"), scheduledTransitionDuration());

        QDBusMessage message = QDBusMessage::createSignal(
            QStringLiteral("/ColorCorrect"),
            QStringLiteral("org.freedesktop.DBus.Properties"),
            QStringLiteral("PropertiesChanged"));

        message.setArguments({
            QStringLiteral("org.kde.kwin.ColorCorrect"),
            changedProperties,
            QStringList(), // invalidated_properties
        });

        QDBusConnection::sessionBus().send(message);
    });

    new ColorCorrectAdaptor(this);
    QDBusConnection::sessionBus().registerObject(QStringLiteral("/ColorCorrect"), this);
    QDBusConnection::sessionBus().registerService(QStringLiteral("org.kde.NightColor"));
}

NightColorDBusInterface::~NightColorDBusInterface()
{
    QDBusConnection::sessionBus().unregisterService(QStringLiteral("org.kde.NightColor"));
}

bool NightColorDBusInterface::isInhibited() const
{
    return m_manager->isInhibited();
}

bool NightColorDBusInterface::isEnabled() const
{
    return m_manager->isEnabled();
}

bool NightColorDBusInterface::isRunning() const
{
    return m_manager->isRunning();
}

bool NightColorDBusInterface::isAvailable() const
{
    return true; // TODO: Night color should register its own dbus service instead.
}

int NightColorDBusInterface::currentTemperature() const
{
    return m_manager->currentTemperature();
}

int NightColorDBusInterface::targetTemperature() const
{
    return m_manager->targetTemperature();
}

int NightColorDBusInterface::mode() const
{
    return m_manager->mode();
}

quint64 NightColorDBusInterface::previousTransitionDateTime() const
{
    const QDateTime dateTime = m_manager->previousTransitionDateTime();
    if (dateTime.isValid()) {
        return quint64(dateTime.toSecsSinceEpoch());
    }
    return 0;
}

quint32 NightColorDBusInterface::previousTransitionDuration() const
{
    return quint32(m_manager->previousTransitionDuration());
}

quint64 NightColorDBusInterface::scheduledTransitionDateTime() const
{
    const QDateTime dateTime = m_manager->scheduledTransitionDateTime();
    if (dateTime.isValid()) {
        return quint64(dateTime.toSecsSinceEpoch());
    }
    return 0;
}

quint32 NightColorDBusInterface::scheduledTransitionDuration() const
{
    return quint32(m_manager->scheduledTransitionDuration());
}

void NightColorDBusInterface::nightColorAutoLocationUpdate(double latitude, double longitude)
{
    m_manager->autoLocationUpdate(latitude, longitude);
}

uint NightColorDBusInterface::inhibit()
{
    const QString serviceName = QDBusContext::message().service();

    if (!m_inhibitors.contains(serviceName)) {
        m_inhibitorWatcher->addWatchedService(serviceName);
    }

    m_inhibitors.insert(serviceName, ++m_lastInhibitionCookie);

    m_manager->inhibit();

    return m_lastInhibitionCookie;
}

void NightColorDBusInterface::uninhibit(uint cookie)
{
    const QString serviceName = QDBusContext::message().service();

    uninhibit(serviceName, cookie);
}

void NightColorDBusInterface::uninhibit(const QString &serviceName, uint cookie)
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

void NightColorDBusInterface::removeInhibitorService(const QString &serviceName)
{
    const auto cookies = m_inhibitors.values(serviceName);
    for (const uint &cookie : cookies) {
        uninhibit(serviceName, cookie);
    }
}

void NightColorDBusInterface::preview(uint previewTemp)
{
    m_manager->preview(previewTemp);
}

void NightColorDBusInterface::stopPreview()
{
    m_manager->stopPreview();
}

}
