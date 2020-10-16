/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2013 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
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
