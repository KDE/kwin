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
#include "dbuscall.h"

#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusPendingCall>

namespace KWin {

DBusCall::DBusCall(QObject *parent)
    : QObject(parent)
{
}

DBusCall::~DBusCall()
{
}

void DBusCall::call()
{
    QDBusMessage msg = QDBusMessage::createMethodCall(m_service, m_path, m_interface, m_method);
    msg.setArguments(m_arguments);

    QDBusPendingCallWatcher *watcher = new QDBusPendingCallWatcher(QDBusConnection::sessionBus().asyncCall(msg), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, [this, watcher]() {
        watcher->deleteLater();
        if (watcher->isError()) {
            emit failed();
            return;
        }
        emit finished(watcher->reply().arguments());
    });
}

} // KWin
