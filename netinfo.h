/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 1999, 2000 Matthias Ettrich <ettrich@kde.org>
    SPDX-FileCopyrightText: 2003 Lubos Lunak <l.lunak@kde.org>
    SPDX-FileCopyrightText: 2009 Lucas Murray <lmurray@undefinedfire.com>
    SPDX-FileCopyrightText: 2013 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#ifndef KWIN_NETINFO_H
#define KWIN_NETINFO_H

#include <NETWM>

#include <xcb/xcb.h>
#include <memory>

namespace KWin
{

class AbstractClient;
class RootInfoFilter;
class X11Client;

/**
 * NET WM Protocol handler class
 */
class RootInfo : public NETRootInfo
{
public:
    static RootInfo *create();
    static void destroy();

    void setActiveClient(AbstractClient *client);

protected:
    void changeNumberOfDesktops(int n) override;
    void changeCurrentDesktop(int d) override;
    void changeActiveWindow(xcb_window_t w, NET::RequestSource src, xcb_timestamp_t timestamp, xcb_window_t active_window) override;
    void closeWindow(xcb_window_t w) override;
    void moveResize(xcb_window_t w, int x_root, int y_root, unsigned long direction) override;
    void moveResizeWindow(xcb_window_t w, int flags, int x, int y, int width, int height) override;
    void gotPing(xcb_window_t w, xcb_timestamp_t timestamp) override;
    void restackWindow(xcb_window_t w, RequestSource source, xcb_window_t above, int detail, xcb_timestamp_t timestamp) override;
    void changeShowingDesktop(bool showing) override;

private:
    RootInfo(xcb_window_t w, const char* name, NET::Properties properties, NET::WindowTypes types,
             NET::States states, NET::Properties2 properties2, NET::Actions actions, int scr = -1);
    static RootInfo *s_self;
    friend RootInfo *rootInfo();

    xcb_window_t m_activeWindow;
    std::unique_ptr<RootInfoFilter> m_eventFilter;
};

inline RootInfo *rootInfo()
{
    return RootInfo::s_self;
}

/**
 * NET WM Protocol handler class
 */
class WinInfo : public NETWinInfo
{
public:
    WinInfo(X11Client *c, xcb_window_t window,
            xcb_window_t rwin, NET::Properties properties, NET::Properties2 properties2);
    void changeDesktop(int desktop) override;
    void changeFullscreenMonitors(NETFullscreenMonitors topology) override;
    void changeState(NET::States state, NET::States mask) override;
    void disable();

private:
    X11Client *m_client;
};

} // KWin

#endif
