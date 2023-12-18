/*
    SPDX-FileCopyrightText: 2019 Martin Flöser <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/
#include "windowsystem.h"

#include <KWaylandExtras>
#include <KWindowSystem>

#include <QGuiApplication>
#include <QWindow>
#include <wayland/display.h>
#include <wayland/seat.h>
#include <wayland_server.h>
#include <window.h>
#include <workspace.h>
#include <xdgactivationv1.h>

Q_DECLARE_METATYPE(NET::WindowType)

namespace KWin
{

WindowSystem::WindowSystem()
    : QObject()
    , KWindowSystemPrivateV2()
{
}

void WindowSystem::activateWindow(QWindow *win, long int time)
{
    // KWin cannot activate own windows
}

void WindowSystem::setShowingDesktop(bool showing)
{
    // KWin should not use KWindowSystem to set showing desktop state
}

bool WindowSystem::showingDesktop()
{
    // KWin should not use KWindowSystem for showing desktop state
    return false;
}

void WindowSystem::requestToken(QWindow *win, uint32_t serial, const QString &appId)
{
    auto seat = KWin::waylandServer()->seat();
    auto token = KWin::waylandServer()->xdgActivationIntegration()->requestPrivilegedToken(nullptr, seat->display()->serial(), seat, appId);
    // Ensure that xdgActivationTokenArrived is always emitted asynchronously
    QTimer::singleShot(0, [serial, token] {
        Q_EMIT KWaylandExtras::self()->xdgActivationTokenArrived(serial, token);
    });
}

void WindowSystem::setCurrentToken(const QString &token)
{
    // KWin cannot activate own windows
}

quint32 WindowSystem::lastInputSerial(QWindow *window)
{
    auto w = workspace()->findInternal(window);
    if (!w) {
        return 0;
    }
    return w->lastUsageSerial();
}

void WindowSystem::exportWindow(QWindow *window)
{
    Q_UNUSED(window);
}

void WindowSystem::unexportWindow(QWindow *window)
{
    Q_UNUSED(window);
}

void WindowSystem::setMainWindow(QWindow *window, const QString &handle)
{
    Q_UNUSED(window);
    Q_UNUSED(handle);
}
}

#include "moc_windowsystem.cpp"
