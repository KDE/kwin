/*
    SPDX-FileCopyrightText: 2019 Martin Flöser <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/
#include "windowsystem.h"

#include <KWindowSystem/KWindowSystem>

#include <QGuiApplication>
#include <QWindow>
#include <wayland/display.h>
#include <wayland/seat_interface.h>
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

void WindowSystem::activateWindow(WId win, long int time)
{
    // KWin cannot activate own windows
}

void WindowSystem::forceActiveWindow(WId win, long int time)
{
    // KWin cannot activate own windows
}

WId WindowSystem::activeWindow()
{
    // KWin internal should not use KWindowSystem to find active window
    return 0;
}

bool WindowSystem::allowedActionsSupported()
{
    return false;
}

void WindowSystem::allowExternalProcessWindowActivation(int pid)
{
}

bool WindowSystem::compositingActive()
{
    // wayland is always composited
    return true;
}

void WindowSystem::connectNotify(const QMetaMethod &signal)
{
}

QPoint WindowSystem::constrainViewportRelativePosition(const QPoint &pos)
{
    return QPoint();
}

int WindowSystem::currentDesktop()
{
    // KWin internal should not use KWindowSystem to find current desktop
    return 0;
}

#if KWINDOWSYSTEM_BUILD_DEPRECATED_SINCE(5, 101)
void WindowSystem::demandAttention(WId win, bool set)
{
}
#endif

QString WindowSystem::desktopName(int desktop)
{
    return QString();
}

QPoint WindowSystem::desktopToViewport(int desktop, bool absolute)
{
    return QPoint();
}

#if KWINDOWSYSTEM_BUILD_DEPRECATED_SINCE(5, 0)
WId WindowSystem::groupLeader(WId window)
{
    return 0;
}
#endif

bool WindowSystem::icccmCompliantMappingState()
{
    return false;
}

QPixmap WindowSystem::icon(WId win, int width, int height, bool scale, int flags)
{
    return QPixmap();
}

void WindowSystem::lowerWindow(WId win)
{
}

bool WindowSystem::mapViewport()
{
    return false;
}

void WindowSystem::minimizeWindow(WId win)
{
}

void WindowSystem::unminimizeWindow(WId win)
{
}

int WindowSystem::numberOfDesktops()
{
    // KWin internal should not use KWindowSystem to find number of desktops
    return 1;
}

void WindowSystem::raiseWindow(WId win)
{
}

QString WindowSystem::readNameProperty(WId window, long unsigned int atom)
{
    return QString();
}

void WindowSystem::setBlockingCompositing(WId window, bool active)
{
}

void WindowSystem::setCurrentDesktop(int desktop)
{
    // KWin internal should not use KWindowSystem to set current desktop
}

void WindowSystem::setDesktopName(int desktop, const QString &name)
{
    // KWin internal should not use KWindowSystem to set desktop name
}

void WindowSystem::setExtendedStrut(WId win, int left_width, int left_start, int left_end, int right_width, int right_start, int right_end, int top_width, int top_start, int top_end, int bottom_width, int bottom_start, int bottom_end)
{
}

void WindowSystem::setStrut(WId win, int left, int right, int top, int bottom)
{
}

#if KWINDOWSYSTEM_BUILD_DEPRECATED_SINCE(5, 101)
void WindowSystem::setIcons(WId win, const QPixmap &icon, const QPixmap &miniIcon)
{
}
#endif

void WindowSystem::setOnActivities(WId win, const QStringList &activities)
{
}

void WindowSystem::setOnAllDesktops(WId win, bool b)
{
}

void WindowSystem::setOnDesktop(WId win, int desktop)
{
}

void WindowSystem::setShowingDesktop(bool showing)
{
    // KWin should not use KWindowSystem to set showing desktop state
}

void WindowSystem::clearState(WId win, NET::States state)
{
    // KWin's windows don't support state
}

void WindowSystem::setState(WId win, NET::States state)
{
    // KWin's windows don't support state
}

void WindowSystem::setType(WId win, NET::WindowType windowType)
{
    const auto windows = qApp->allWindows();
    auto it = std::find_if(windows.begin(), windows.end(), [win](QWindow *w) {
        return w->handle() && w->winId() == win;
    });
    if (it == windows.end()) {
        return;
    }

    (*it)->setProperty("kwin_windowType", QVariant::fromValue(windowType));
}

void WindowSystem::setUserTime(WId win, long int time)
{
}

bool WindowSystem::showingDesktop()
{
    // KWin should not use KWindowSystem for showing desktop state
    return false;
}

QList<WId> WindowSystem::stackingOrder()
{
    // KWin should not use KWindowSystem for stacking order
    return {};
}

#if KWINDOWSYSTEM_BUILD_DEPRECATED_SINCE(5, 0)
WId WindowSystem::transientFor(WId window)
{
    return 0;
}
#endif

int WindowSystem::viewportToDesktop(const QPoint &pos)
{
    return 0;
}

int WindowSystem::viewportWindowToDesktop(const QRect &r)
{
    return 0;
}

QList<WId> WindowSystem::windows()
{
    return {};
}

QRect WindowSystem::workArea(const QList<WId> &excludes, int desktop)
{
    return {};
}

QRect WindowSystem::workArea(int desktop)
{
    return {};
}

void WindowSystem::requestToken(QWindow *win, uint32_t serial, const QString &appId)
{
    auto seat = KWin::waylandServer()->seat();
    auto token = KWin::waylandServer()->xdgActivationIntegration()->requestPrivilegedToken(nullptr, seat->display()->serial(), seat, appId);
    // Ensure that xdgActivationTokenArrived is always emitted asynchronously
    QTimer::singleShot(0, [serial, token] {
        Q_EMIT KWindowSystem::self()->xdgActivationTokenArrived(serial, token);
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
}
