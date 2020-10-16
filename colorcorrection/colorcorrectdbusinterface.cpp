/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2017 Roman Gilg <subdiff@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "colorcorrectdbusinterface.h"
#include "colorcorrectadaptor.h"

#include "manager.h"

#include <QDBusMessage>

namespace KWin {
namespace ColorCorrect {

ColorCorrectDBusInterface::ColorCorrectDBusInterface(Manager *parent)
    : QObject(parent)
    , m_manager(parent)
    , m_inhibitorWatcher(new QDBusServiceWatcher(this))
{
    m_inhibitorWatcher->setConnection(QDBusConnection::sessionBus());
    m_inhibitorWatcher->setWatchMode(QDBusServiceWatcher::WatchForUnregistration);
    connect(m_inhibitorWatcher, &QDBusServiceWatcher::serviceUnregistered,
            this, &ColorCorrectDBusInterface::removeInhibitorService);

    connect(m_manager, &Manager::inhibitedChanged, this, [this] {
        QVariantMap changedProperties;
        changedProperties.insert(QStringLiteral("inhibited"), m_manager->isInhibited());

        QDBusMessage message = QDBusMessage::createSignal(
            QStringLiteral("/ColorCorrect"),
            QStringLiteral("org.freedesktop.DBus.Properties"),
            QStringLiteral("PropertiesChanged")
        );

        message.setArguments({
            QStringLiteral("org.kde.kwin.ColorCorrect"),
            changedProperties,
            QStringList(), // invalidated_properties
        });

        QDBusConnection::sessionBus().send(message);
    });

    connect(m_manager, &Manager::enabledChanged, this, [this] {
        QVariantMap changedProperties;
        changedProperties.insert(QStringLiteral("enabled"), m_manager->isEnabled());

        QDBusMessage message = QDBusMessage::createSignal(
            QStringLiteral("/ColorCorrect"),
            QStringLiteral("org.freedesktop.DBus.Properties"),
            QStringLiteral("PropertiesChanged")
        );

        message.setArguments({
            QStringLiteral("org.kde.kwin.ColorCorrect"),
            changedProperties,
            QStringList(), // invalidated_properties
        });

        QDBusConnection::sessionBus().send(message);
    });

    connect(m_manager, &Manager::runningChanged, this, [this] {
        QVariantMap changedProperties;
        changedProperties.insert(QStringLiteral("running"), m_manager->isRunning());

        QDBusMessage message = QDBusMessage::createSignal(
            QStringLiteral("/ColorCorrect"),
            QStringLiteral("org.freedesktop.DBus.Properties"),
            QStringLiteral("PropertiesChanged")
        );

        message.setArguments({
            QStringLiteral("org.kde.kwin.ColorCorrect"),
            changedProperties,
            QStringList(), // invalidated_properties
        });

        QDBusConnection::sessionBus().send(message);
    });

    connect(m_manager, &Manager::currentTemperatureChanged, this, [this] {
        QVariantMap changedProperties;
        changedProperties.insert(QStringLiteral("currentTemperature"), m_manager->currentTemperature());

        QDBusMessage message = QDBusMessage::createSignal(
            QStringLiteral("/ColorCorrect"),
            QStringLiteral("org.freedesktop.DBus.Properties"),
            QStringLiteral("PropertiesChanged")
        );

        message.setArguments({
            QStringLiteral("org.kde.kwin.ColorCorrect"),
            changedProperties,
            QStringList(), // invalidated_properties
        });

        QDBusConnection::sessionBus().send(message);
    });

    connect(m_manager, &Manager::targetTemperatureChanged, this, [this] {
        QVariantMap changedProperties;
        changedProperties.insert(QStringLiteral("targetTemperature"), m_manager->targetTemperature());

        QDBusMessage message = QDBusMessage::createSignal(
            QStringLiteral("/ColorCorrect"),
            QStringLiteral("org.freedesktop.DBus.Properties"),
            QStringLiteral("PropertiesChanged")
        );

        message.setArguments({
            QStringLiteral("org.kde.kwin.ColorCorrect"),
            changedProperties,
            QStringList(), // invalidated_properties
        });

        QDBusConnection::sessionBus().send(message);
    });

    connect(m_manager, &Manager::modeChanged, this, [this] {
        QVariantMap changedProperties;
        changedProperties.insert(QStringLiteral("mode"), uint(m_manager->mode()));

        QDBusMessage message = QDBusMessage::createSignal(
            QStringLiteral("/ColorCorrect"),
            QStringLiteral("org.freedesktop.DBus.Properties"),
            QStringLiteral("PropertiesChanged")
        );

        message.setArguments({
            QStringLiteral("org.kde.kwin.ColorCorrect"),
            changedProperties,
            QStringList(), // invalidated_properties
        });

        QDBusConnection::sessionBus().send(message);
    });

    connect(m_manager, &Manager::previousTransitionTimingsChanged, this, [this] {
        QVariantMap changedProperties;
        changedProperties.insert(QStringLiteral("previousTransitionDateTime"), previousTransitionDateTime());
        changedProperties.insert(QStringLiteral("previousTransitionDuration"), previousTransitionDuration());

        QDBusMessage message = QDBusMessage::createSignal(
            QStringLiteral("/ColorCorrect"),
            QStringLiteral("org.freedesktop.DBus.Properties"),
            QStringLiteral("PropertiesChanged")
        );

        message.setArguments({
            QStringLiteral("org.kde.kwin.ColorCorrect"),
            changedProperties,
            QStringList(), // invalidated_properties
        });

        QDBusConnection::sessionBus().send(message);
    });

    connect(m_manager, &Manager::scheduledTransitionTimingsChanged, this, [this] {
        QVariantMap changedProperties;
        changedProperties.insert(QStringLiteral("scheduledTransitionDateTime"), scheduledTransitionDateTime());
        changedProperties.insert(QStringLiteral("scheduledTransitionDuration"), scheduledTransitionDuration());

        QDBusMessage message = QDBusMessage::createSignal(
            QStringLiteral("/ColorCorrect"),
            QStringLiteral("org.freedesktop.DBus.Properties"),
            QStringLiteral("PropertiesChanged")
        );

        message.setArguments({
            QStringLiteral("org.kde.kwin.ColorCorrect"),
            changedProperties,
            QStringList(), // invalidated_properties
        });

        QDBusConnection::sessionBus().send(message);
    });

    connect(m_manager, &Manager::configChange, this, &ColorCorrectDBusInterface::nightColorConfigChanged);
    new ColorCorrectAdaptor(this);
    QDBusConnection::sessionBus().registerObject(QStringLiteral("/ColorCorrect"), this);
}

bool ColorCorrectDBusInterface::isInhibited() const
{
    return m_manager->isInhibited();
}

bool ColorCorrectDBusInterface::isEnabled() const
{
    return m_manager->isEnabled();
}

bool ColorCorrectDBusInterface::isRunning() const
{
    return m_manager->isRunning();
}

bool ColorCorrectDBusInterface::isAvailable() const
{
    return m_manager->isAvailable();
}

int ColorCorrectDBusInterface::currentTemperature() const
{
    return m_manager->currentTemperature();
}

int ColorCorrectDBusInterface::targetTemperature() const
{
    return m_manager->targetTemperature();
}

int ColorCorrectDBusInterface::mode() const
{
    return m_manager->mode();
}

quint64 ColorCorrectDBusInterface::previousTransitionDateTime() const
{
    const QDateTime dateTime = m_manager->previousTransitionDateTime();
    if (dateTime.isValid()) {
        return quint64(dateTime.toSecsSinceEpoch());
    }
    return 0;
}

quint32 ColorCorrectDBusInterface::previousTransitionDuration() const
{
    return quint32(m_manager->previousTransitionDuration());
}

quint64 ColorCorrectDBusInterface::scheduledTransitionDateTime() const
{
    const QDateTime dateTime = m_manager->scheduledTransitionDateTime();
    if (dateTime.isValid()) {
        return quint64(dateTime.toSecsSinceEpoch());
    }
    return 0;
}

quint32 ColorCorrectDBusInterface::scheduledTransitionDuration() const
{
    return quint32(m_manager->scheduledTransitionDuration());
}

QHash<QString, QVariant> ColorCorrectDBusInterface::nightColorInfo()
{
    return m_manager->info();
}

bool ColorCorrectDBusInterface::setNightColorConfig(QHash<QString, QVariant> data)
{
    return m_manager->changeConfiguration(data);
}

void ColorCorrectDBusInterface::nightColorAutoLocationUpdate(double latitude, double longitude)
{
    m_manager->autoLocationUpdate(latitude, longitude);
}

uint ColorCorrectDBusInterface::inhibit()
{
    const QString serviceName = QDBusContext::message().service();

    if (!m_inhibitors.contains(serviceName)) {
        m_inhibitorWatcher->addWatchedService(serviceName);
    }

    m_inhibitors.insert(serviceName, ++m_lastInhibitionCookie);

    m_manager->inhibit();

    return m_lastInhibitionCookie;
}

void ColorCorrectDBusInterface::uninhibit(uint cookie)
{
    const QString serviceName = QDBusContext::message().service();

    uninhibit(serviceName, cookie);
}

void ColorCorrectDBusInterface::uninhibit(const QString &serviceName, uint cookie)
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

void ColorCorrectDBusInterface::removeInhibitorService(const QString &serviceName)
{
    const auto cookies = m_inhibitors.values(serviceName);
    for (const uint &cookie : cookies) {
        uninhibit(serviceName, cookie);
    }
}

}
}
