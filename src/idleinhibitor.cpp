/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2024 Xaver Hugl <xaver.hugl@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "idleinhibitor.h"

#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusPendingCall>
#include <QDBusPendingReply>
#include <QPointer>

namespace KWin
{

static void closeRequest(const QString &path)
{
    QDBusMessage message = QDBusMessage::createMethodCall(QLatin1String("org.freedesktop.portal.Desktop"),
                                                          QLatin1String("/org/freedesktop/portal/desktop"),
                                                          QLatin1String("org.freedesktop.portal.Inhibit"),
                                                          QLatin1String("Close"));
    QDBusConnection::sessionBus().asyncCall(message);
}

IdleInhibitor::IdleInhibitor(const QString &reason)
{
    QDBusMessage message = QDBusMessage::createMethodCall(QLatin1String("org.freedesktop.portal.Desktop"),
                                                          QLatin1String("/org/freedesktop/portal/desktop"),
                                                          QLatin1String("org.freedesktop.portal.Inhibit"),
                                                          QLatin1String("Inhibit"));
    message << "" << 8U << QVariantMap({{QLatin1String("reason"), reason}});

    QDBusPendingCall pendingCall = QDBusConnection::sessionBus().asyncCall(message);
    auto watcher = new QDBusPendingCallWatcher(pendingCall);
    connect(watcher, &QDBusPendingCallWatcher::finished, watcher, [inhibitor = QPointer(this)](QDBusPendingCallWatcher *watcher) {
        QDBusPendingReply<QDBusObjectPath> reply = *watcher;
        if (reply.isError()) {
            qWarning() << "Couldn't get reply";
            qWarning() << "Error: " << reply.error().message();
        } else {
            if (inhibitor) {
                inhibitor->m_requestPath = reply.value().path();
            } else {
                closeRequest(reply.value().path());
            }
        }
    });
}

IdleInhibitor::~IdleInhibitor()
{
    if (!m_requestPath.isEmpty()) {
        closeRequest(m_requestPath);
    }
}

}
