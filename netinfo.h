/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 1999, 2000 Matthias Ettrich <ettrich@kde.org>
Copyright (C) 2003 Lubos Lunak <l.lunak@kde.org>
Copyright (C) 2009 Lucas Murray <lmurray@undefinedfire.com>
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
#ifndef KWIN_NETINFO_H
#define KWIN_NETINFO_H

#include <NETWM>

#include <xcb/xcb.h>
#include <memory>

namespace KWin
{

class AbstractClient;
class Client;
class RootInfoFilter;

/**
 * NET WM Protocol handler class
 **/
class RootInfo : public NETRootInfo
{
private:
    typedef KWin::Client Client;  // Because of NET::Client

public:
    static RootInfo *create();
    static void destroy();

    void setActiveClient(AbstractClient *client);

protected:
    virtual void changeNumberOfDesktops(int n) override;
    virtual void changeCurrentDesktop(int d) override;
    virtual void changeActiveWindow(xcb_window_t w, NET::RequestSource src, xcb_timestamp_t timestamp, xcb_window_t active_window) override;
    virtual void closeWindow(xcb_window_t w) override;
    virtual void moveResize(xcb_window_t w, int x_root, int y_root, unsigned long direction) override;
    virtual void moveResizeWindow(xcb_window_t w, int flags, int x, int y, int width, int height) override;
    virtual void gotPing(xcb_window_t w, xcb_timestamp_t timestamp) override;
    virtual void restackWindow(xcb_window_t w, RequestSource source, xcb_window_t above, int detail, xcb_timestamp_t timestamp) override;
    virtual void changeShowingDesktop(bool showing) override;

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
 **/
class WinInfo : public NETWinInfo
{
private:
    typedef KWin::Client Client; // Because of NET::Client

public:
    WinInfo(Client* c, xcb_window_t window,
            xcb_window_t rwin, NET::Properties properties, NET::Properties2 properties2);
    virtual void changeDesktop(int desktop) override;
    virtual void changeFullscreenMonitors(NETFullscreenMonitors topology) override;
    virtual void changeState(NET::States state, NET::States mask) override;
    void disable();

private:
    Client * m_client;
};

} // KWin

#endif
