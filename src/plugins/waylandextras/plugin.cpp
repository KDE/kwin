/*
    SPDX-FileCopyrightText: 2019 Martin Flöser <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/
#include "plugin.h"

#include <QGuiApplication>
#include <QWindow>
#include <wayland/display.h>
#include <wayland/seat.h>
#include <wayland_server.h>
#include <window.h>
#include <workspace.h>
#include <xdgactivationv1.h>

void KWaylandExtrasKWinPlugin::requestXdgActivationToken(QWindow *win, uint32_t serial, const QString &appId)
{
    auto seat = KWin::waylandServer()->seat();
    auto token = KWin::waylandServer()->xdgActivationIntegration()->requestPrivilegedToken(nullptr, seat->display()->serial(), seat, appId);
    // Ensure that xdgActivationTokenArrived is always emitted asynchronously
    QTimer::singleShot(0, [this, serial, token] {
        Q_EMIT xdgActivationTokenArrived(serial, token);
    });
}

quint32 KWaylandExtrasKWinPlugin::lastInputSerial(QWindow *window)
{
    auto w = KWin::workspace()->findInternal(window);
    if (!w) {
        return 0;
    }
    return w->lastUsageSerial();
}
