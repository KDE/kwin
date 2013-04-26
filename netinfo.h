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

#include <KDE/NETRootInfo>

namespace KWin
{

class Client;

/**
 * NET WM Protocol handler class
 */
class RootInfo : public NETRootInfo
{
private:
    typedef KWin::Client Client;  // Because of NET::Client

public:
    RootInfo(Window w, const char* name, unsigned long pr[],
             int pr_num, int scr = -1);

protected:
    virtual void changeNumberOfDesktops(int n);
    virtual void changeCurrentDesktop(int d);
    virtual void changeActiveWindow(Window w, NET::RequestSource src, Time timestamp, Window active_window);
    virtual void closeWindow(Window w);
    virtual void moveResize(Window w, int x_root, int y_root, unsigned long direction);
    virtual void moveResizeWindow(Window w, int flags, int x, int y, int width, int height);
    virtual void gotPing(Window w, Time timestamp);
    virtual void restackWindow(Window w, RequestSource source, Window above, int detail, Time timestamp);
    virtual void gotTakeActivity(Window w, Time timestamp, long flags);
    virtual void changeShowingDesktop(bool showing);
};

/**
 * NET WM Protocol handler class
 */
class WinInfo : public NETWinInfo2
{
private:
    typedef KWin::Client Client; // Because of NET::Client

public:
    WinInfo(Client* c, Display * display, Window window,
            Window rwin, const unsigned long pr[], int pr_size);
    virtual void changeDesktop(int desktop);
    virtual void changeFullscreenMonitors(NETFullscreenMonitors topology);
    virtual void changeState(unsigned long state, unsigned long mask);
    void disable();

private:
    Client * m_client;
};

} // KWin

#endif
