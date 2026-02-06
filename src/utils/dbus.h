/*
    SPDX-FileCopyrightText: 2026 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QDBusConnection>
#include <QDBusError>
#include <QDBusPendingCall>
#include <QDBusPendingReply>
#include <QFuture>

#include <expected>

namespace KWin
{

/**
 * Creates a QFuture object for the specified asynchronous dbus @a call.
 */
template<typename T>
static QFuture<std::expected<T, QDBusError>> intoFuture(const QDBusPendingCall &call)
{
    QPromise<std::expected<T, QDBusError>> promise;
    promise.start();

    QFuture<std::expected<T, QDBusError>> future = promise.future();

    QDBusPendingCallWatcher *watcher = new QDBusPendingCallWatcher(call);
    QObject::connect(watcher, &QDBusPendingCallWatcher::finished, watcher, [watcher, p = std::move(promise)]() mutable {
        watcher->deleteLater();

        const QDBusPendingReply<T> reply = *watcher;
        if (reply.isError()) {
            p.addResult(std::unexpected(reply.error()));
        } else {
            p.addResult(reply.template argumentAt<0>());
        }

        p.finish();
    });

    return future;
}

template<>
QFuture<std::expected<void, QDBusError>> intoFuture(const QDBusPendingCall &call)
{
    QPromise<std::expected<void, QDBusError>> promise;
    promise.start();

    QFuture<std::expected<void, QDBusError>> future = promise.future();

    QDBusPendingCallWatcher *watcher = new QDBusPendingCallWatcher(call);
    QObject::connect(watcher, &QDBusPendingCallWatcher::finished, watcher, [watcher, p = std::move(promise)]() mutable {
        watcher->deleteLater();

        const QDBusPendingReply reply = *watcher;
        if (reply.isError()) {
            p.addResult(std::unexpected(reply.error()));
        } else {
            p.addResult({});
        }

        p.finish();
    });

    return future;
}

} // namespace KWin
