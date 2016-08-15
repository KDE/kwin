/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2013 Martin Gräßlin <mgraesslin@kde.org>

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
#include "screenlockerwatcher.h"

#include <QFutureWatcher>
#include <QtConcurrentRun>
// dbus generated
#include "screenlocker_interface.h"

namespace KWin
{

static const QString SCREEN_LOCKER_SERVICE_NAME = QStringLiteral("org.freedesktop.ScreenSaver");

ScreenLockerWatcher::ScreenLockerWatcher(QObject *parent)
    : QObject(parent)
    , m_interface(NULL)
    , m_serviceWatcher(new QDBusServiceWatcher(this))
    , m_locked(false)
{
    connect(m_serviceWatcher, SIGNAL(serviceOwnerChanged(QString,QString,QString)), SLOT(serviceOwnerChanged(QString,QString,QString)));
    m_serviceWatcher->setWatchMode(QDBusServiceWatcher::WatchForOwnerChange);
    m_serviceWatcher->addWatchedService(SCREEN_LOCKER_SERVICE_NAME);
    // check whether service is registered
    QFutureWatcher<QDBusReply<bool> > *watcher = new QFutureWatcher<QDBusReply<bool> >(this);
    connect(watcher, SIGNAL(finished()), SLOT(serviceRegisteredQueried()));
    connect(watcher, SIGNAL(canceled()), watcher, SLOT(deleteLater()));
    watcher->setFuture(QtConcurrent::run(QDBusConnection::sessionBus().interface(),
                                         &QDBusConnectionInterface::isServiceRegistered,
                                         SCREEN_LOCKER_SERVICE_NAME));
}

ScreenLockerWatcher::~ScreenLockerWatcher()
{
}

void ScreenLockerWatcher::serviceOwnerChanged(const QString &serviceName, const QString &oldOwner, const QString &newOwner)
{
    Q_UNUSED(oldOwner)
    if (serviceName != SCREEN_LOCKER_SERVICE_NAME) {
        return;
    }
    delete m_interface;
    m_interface = NULL;
    m_locked = false;
    if (!newOwner.isEmpty()) {
        m_interface = new OrgFreedesktopScreenSaverInterface(newOwner, QStringLiteral("/ScreenSaver"), QDBusConnection::sessionBus(), this);
        connect(m_interface, SIGNAL(ActiveChanged(bool)), SLOT(setLocked(bool)));
        QDBusPendingCallWatcher *watcher = new QDBusPendingCallWatcher(m_interface->GetActive(), this);
        connect(watcher, SIGNAL(finished(QDBusPendingCallWatcher*)), SLOT(activeQueried(QDBusPendingCallWatcher*)));
    }
}

void ScreenLockerWatcher::serviceRegisteredQueried()
{
    QFutureWatcher<QDBusReply<bool> > *watcher = dynamic_cast<QFutureWatcher<QDBusReply<bool> > *>(sender());
    if (!watcher) {
        return;
    }
    const QDBusReply<bool> &reply = watcher->result();
    if (reply.isValid() && reply.value()) {
        QFutureWatcher<QDBusReply<QString> > *ownerWatcher = new QFutureWatcher<QDBusReply<QString> >(this);
        connect(ownerWatcher, SIGNAL(finished()), SLOT(serviceOwnerQueried()));
        connect(ownerWatcher, SIGNAL(canceled()), ownerWatcher, SLOT(deleteLater()));
        ownerWatcher->setFuture(QtConcurrent::run(QDBusConnection::sessionBus().interface(),
                                                  &QDBusConnectionInterface::serviceOwner,
                                                  SCREEN_LOCKER_SERVICE_NAME));
    }
    watcher->deleteLater();
}

void ScreenLockerWatcher::serviceOwnerQueried()
{
    QFutureWatcher<QDBusReply<QString> > *watcher = dynamic_cast<QFutureWatcher<QDBusReply<QString> > *>(sender());
    if (!watcher) {
        return;
    }
    const QDBusReply<QString> reply = watcher->result();
    if (reply.isValid()) {
        serviceOwnerChanged(SCREEN_LOCKER_SERVICE_NAME, QString(), reply.value());
    }

    watcher->deleteLater();
}

void ScreenLockerWatcher::activeQueried(QDBusPendingCallWatcher *watcher)
{
    QDBusPendingReply<bool> reply = *watcher;
    if (!reply.isError()) {
        setLocked(reply.value());
    }
    watcher->deleteLater();
}

void ScreenLockerWatcher::setLocked(bool activated)
{
    if (m_locked == activated) {
        return;
    }
    m_locked = activated;
    emit locked(m_locked);
}

}
