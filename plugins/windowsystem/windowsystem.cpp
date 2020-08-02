/*
    SPDX-FileCopyrightText: 2019 Martin Flöser <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/
#include "windowsystem.h"

#include <KWindowSystem/KWindowSystem>

#include <QGuiApplication>
#include <QWindow>

Q_DECLARE_METATYPE(NET::WindowType)

namespace KWin
{

WindowSystem::WindowSystem()
    : QObject()
    , KWindowSystemPrivate()
{
}

void WindowSystem::activateWindow(WId win, long int time)
{
    Q_UNUSED(win)
    Q_UNUSED(time)
    // KWin cannot activate own windows
}

void WindowSystem::forceActiveWindow(WId win, long int time)
{
    Q_UNUSED(win)
    Q_UNUSED(time)
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
    Q_UNUSED(pid)
}

bool WindowSystem::compositingActive()
{
    // wayland is always composited
    return true;
}

void WindowSystem::connectNotify(const QMetaMethod &signal)
{
    Q_UNUSED(signal)
}

QPoint WindowSystem::constrainViewportRelativePosition(const QPoint &pos)
{
    Q_UNUSED(pos)
    return QPoint();
}

int WindowSystem::currentDesktop()
{
    // KWin internal should not use KWindowSystem to find current desktop
    return 0;
}

void WindowSystem::demandAttention(WId win, bool set)
{
    Q_UNUSED(win)
    Q_UNUSED(set)
}

QString WindowSystem::desktopName(int desktop)
{
    Q_UNUSED(desktop)
    return QString();
}

QPoint WindowSystem::desktopToViewport(int desktop, bool absolute)
{
    Q_UNUSED(desktop)
    Q_UNUSED(absolute)
    return QPoint();
}

#if KWINDOWSYSTEM_BUILD_DEPRECATED_SINCE(5, 0)
WId WindowSystem::groupLeader(WId window)
{
    Q_UNUSED(window)
    return 0;
}
#endif

bool WindowSystem::icccmCompliantMappingState()
{
    return false;
}

QPixmap WindowSystem::icon(WId win, int width, int height, bool scale, int flags)
{
    Q_UNUSED(win)
    Q_UNUSED(width)
    Q_UNUSED(height)
    Q_UNUSED(scale)
    Q_UNUSED(flags)
    return QPixmap();
}

void WindowSystem::lowerWindow(WId win)
{
    Q_UNUSED(win)
}

bool WindowSystem::mapViewport()
{
    return false;
}

void WindowSystem::minimizeWindow(WId win)
{
    Q_UNUSED(win)
}

void WindowSystem::unminimizeWindow(WId win)
{
    Q_UNUSED(win)
}

int WindowSystem::numberOfDesktops()
{
    // KWin internal should not use KWindowSystem to find number of desktops
    return 1;
}

void WindowSystem::raiseWindow(WId win)
{
    Q_UNUSED(win)
}

QString WindowSystem::readNameProperty(WId window, long unsigned int atom)
{
    Q_UNUSED(window)
    Q_UNUSED(atom)
    return QString();
}

void WindowSystem::setBlockingCompositing(WId window, bool active)
{
    Q_UNUSED(window)
    Q_UNUSED(active)
}

void WindowSystem::setCurrentDesktop(int desktop)
{
    Q_UNUSED(desktop)
    // KWin internal should not use KWindowSystem to set current desktop
}

void WindowSystem::setDesktopName(int desktop, const QString &name)
{
    Q_UNUSED(desktop)
    Q_UNUSED(name)
    // KWin internal should not use KWindowSystem to set desktop name
}

void WindowSystem::setExtendedStrut(WId win, int left_width, int left_start, int left_end, int right_width, int right_start, int right_end, int top_width, int top_start, int top_end, int bottom_width, int bottom_start, int bottom_end)
{
    Q_UNUSED(win)
    Q_UNUSED(left_width)
    Q_UNUSED(left_start)
    Q_UNUSED(left_end)
    Q_UNUSED(right_width)
    Q_UNUSED(right_start)
    Q_UNUSED(right_end)
    Q_UNUSED(top_width)
    Q_UNUSED(top_start)
    Q_UNUSED(top_end)
    Q_UNUSED(bottom_width)
    Q_UNUSED(bottom_start)
    Q_UNUSED(bottom_end)
}

void WindowSystem::setStrut(WId win, int left, int right, int top, int bottom)
{
    Q_UNUSED(win)
    Q_UNUSED(left)
    Q_UNUSED(right)
    Q_UNUSED(top)
    Q_UNUSED(bottom)
}

void WindowSystem::setIcons(WId win, const QPixmap &icon, const QPixmap &miniIcon)
{
    Q_UNUSED(win)
    Q_UNUSED(icon)
    Q_UNUSED(miniIcon)
}

void WindowSystem::setOnActivities(WId win, const QStringList &activities)
{
    Q_UNUSED(win)
    Q_UNUSED(activities)
}

void WindowSystem::setOnAllDesktops(WId win, bool b)
{
    Q_UNUSED(win)
    Q_UNUSED(b)
}

void WindowSystem::setOnDesktop(WId win, int desktop)
{
    Q_UNUSED(win)
    Q_UNUSED(desktop)
}

void WindowSystem::setShowingDesktop(bool showing)
{
    Q_UNUSED(showing)
    // KWin should not use KWindowSystem to set showing desktop state
}

void WindowSystem::clearState(WId win, NET::States state)
{
    // KWin's windows don't support state
    Q_UNUSED(win)
    Q_UNUSED(state)
}

void WindowSystem::setState(WId win, NET::States state)
{
    // KWin's windows don't support state
    Q_UNUSED(win)
    Q_UNUSED(state)
}

void WindowSystem::setType(WId win, NET::WindowType windowType)
{
    const auto windows = qApp->allWindows();
    auto it = std::find_if(windows.begin(), windows.end(), [win] (QWindow *w) { return w->winId() == win; });
    if (it == windows.end()) {
        return;
    }

    (*it)->setProperty("kwin_windowType", QVariant::fromValue(windowType));
}

void WindowSystem::setUserTime(WId win, long int time)
{
    Q_UNUSED(win)
    Q_UNUSED(time)
}

bool WindowSystem::showingDesktop()
{
    // KWin should not use KWindowSystem for showing desktop state
    return false;
}

QList< WId > WindowSystem::stackingOrder()
{
    // KWin should not use KWindowSystem for stacking order
    return {};
}

#if KWINDOWSYSTEM_BUILD_DEPRECATED_SINCE(5, 0)
WId WindowSystem::transientFor(WId window)
{
    Q_UNUSED(window)
    return 0;
}
#endif

int WindowSystem::viewportToDesktop(const QPoint &pos)
{
    Q_UNUSED(pos)
    return 0;
}

int WindowSystem::viewportWindowToDesktop(const QRect &r)
{
    Q_UNUSED(r)
    return 0;
}

QList< WId > WindowSystem::windows()
{
    return {};
}

QRect WindowSystem::workArea(const QList< WId > &excludes, int desktop)
{
    Q_UNUSED(excludes)
    Q_UNUSED(desktop)
    return {};
}

QRect WindowSystem::workArea(int desktop)
{
    Q_UNUSED(desktop)
    return {};
}

}
