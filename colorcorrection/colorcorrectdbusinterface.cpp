/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright 2017 Roman Gilg <subdiff@gmail.com>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*********************************************************************/

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

    // Argh, all this code is just to send one innocent signal...
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

    connect(m_manager, &Manager::configChange, this, &ColorCorrectDBusInterface::nightColorConfigChanged);
    new ColorCorrectAdaptor(this);
    QDBusConnection::sessionBus().registerObject(QStringLiteral("/ColorCorrect"), this);
}

bool ColorCorrectDBusInterface::isInhibited() const
{
    return m_manager->isInhibited();
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

    if (m_inhibitors.values(serviceName).isEmpty()) {
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

    if (m_inhibitors.values(serviceName).isEmpty()) {
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
