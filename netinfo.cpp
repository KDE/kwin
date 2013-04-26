/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 1999, 2000 Matthias Ettrich <ettrich@kde.org>
Copyright (C) 2003 Lubos Lunak <l.lunak@kde.org>
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
// own
#include "netinfo.h"
// kwin
#include "client.h"
#include "virtualdesktops.h"
#include "workspace.h"

namespace KWin
{

RootInfo::RootInfo(Window w, const char *name, unsigned long pr[], int pr_num, int scr)
    : NETRootInfo(display(), w, name, pr, pr_num, scr)
{
}

void RootInfo::changeNumberOfDesktops(int n)
{
    VirtualDesktopManager::self()->setCount(n);
}

void RootInfo::changeCurrentDesktop(int d)
{
    VirtualDesktopManager::self()->setCurrent(d);
}

void RootInfo::changeActiveWindow(Window w, NET::RequestSource src, Time timestamp, Window active_window)
{
    Workspace *workspace = Workspace::self();
    if (Client* c = workspace->findClient(WindowMatchPredicate(w))) {
        if (timestamp == CurrentTime)
            timestamp = c->userTime();
        if (src != NET::FromApplication && src != FromTool)
            src = NET::FromTool;
        if (src == NET::FromTool)
            workspace->activateClient(c, true);   // force
        else if (c == workspace->mostRecentlyActivatedClient()) {
            return; // WORKAROUND? With > 1 plasma activities, we cause this ourselves. bug #240673
        } else { // NET::FromApplication
            Client* c2;
            if (workspace->allowClientActivation(c, timestamp, false, true))
                workspace->activateClient(c);
            // if activation of the requestor's window would be allowed, allow activation too
            else if (active_window != None
                    && (c2 = workspace->findClient(WindowMatchPredicate(active_window))) != NULL
                    && workspace->allowClientActivation(c2,
                            timestampCompare(timestamp, c2->userTime() > 0 ? timestamp : c2->userTime()), false, true)) {
                workspace->activateClient(c);
            } else
                c->demandAttention();
        }
    }
}

void RootInfo::restackWindow(Window w, RequestSource src, Window above, int detail, Time timestamp)
{
    if (Client* c = Workspace::self()->findClient(WindowMatchPredicate(w))) {
        if (timestamp == CurrentTime)
            timestamp = c->userTime();
        if (src != NET::FromApplication && src != FromTool)
            src = NET::FromTool;
        c->restackWindow(above, detail, src, timestamp, true);
    }
}

void RootInfo::gotTakeActivity(Window w, Time timestamp, long flags)
{
    Workspace *workspace = Workspace::self();
    if (Client* c = workspace->findClient(WindowMatchPredicate(w)))
        workspace->handleTakeActivity(c, timestamp, flags);
}

void RootInfo::closeWindow(Window w)
{
    Client* c = Workspace::self()->findClient(WindowMatchPredicate(w));
    if (c)
        c->closeWindow();
}

void RootInfo::moveResize(Window w, int x_root, int y_root, unsigned long direction)
{
    Client* c = Workspace::self()->findClient(WindowMatchPredicate(w));
    if (c) {
        updateXTime(); // otherwise grabbing may have old timestamp - this message should include timestamp
        c->NETMoveResize(x_root, y_root, (Direction)direction);
    }
}

void RootInfo::moveResizeWindow(Window w, int flags, int x, int y, int width, int height)
{
    Client* c = Workspace::self()->findClient(WindowMatchPredicate(w));
    if (c)
        c->NETMoveResizeWindow(flags, x, y, width, height);
}

void RootInfo::gotPing(Window w, Time timestamp)
{
    if (Client* c = Workspace::self()->findClient(WindowMatchPredicate(w)))
        c->gotPing(timestamp);
}

void RootInfo::changeShowingDesktop(bool showing)
{
    Workspace::self()->setShowingDesktop(showing);
}

// ****************************************
// WinInfo
// ****************************************

WinInfo::WinInfo(Client * c, Display * display, Window window,
                 Window rwin, const unsigned long pr[], int pr_size)
    : NETWinInfo2(display, window, rwin, pr, pr_size, NET::WindowManager), m_client(c)
{
}

void WinInfo::changeDesktop(int desktop)
{
    Workspace::self()->sendClientToDesktop(m_client, desktop, true);
}

void WinInfo::changeFullscreenMonitors(NETFullscreenMonitors topology)
{
    m_client->updateFullscreenMonitors(topology);
}

void WinInfo::changeState(unsigned long state, unsigned long mask)
{
    mask &= ~NET::Sticky; // KWin doesn't support large desktops, ignore
    mask &= ~NET::Hidden; // clients are not allowed to change this directly
    state &= mask; // for safety, clear all other bits

    if ((mask & NET::FullScreen) != 0 && (state & NET::FullScreen) == 0)
        m_client->setFullScreen(false, false);
    if ((mask & NET::Max) == NET::Max)
        m_client->setMaximize(state & NET::MaxVert, state & NET::MaxHoriz);
    else if (mask & NET::MaxVert)
        m_client->setMaximize(state & NET::MaxVert, m_client->maximizeMode() & Client::MaximizeHorizontal);
    else if (mask & NET::MaxHoriz)
        m_client->setMaximize(m_client->maximizeMode() & Client::MaximizeVertical, state & NET::MaxHoriz);

    if (mask & NET::Shaded)
        m_client->setShade(state & NET::Shaded ? ShadeNormal : ShadeNone);
    if (mask & NET::KeepAbove)
        m_client->setKeepAbove((state & NET::KeepAbove) != 0);
    if (mask & NET::KeepBelow)
        m_client->setKeepBelow((state & NET::KeepBelow) != 0);
    if (mask & NET::SkipTaskbar)
        m_client->setSkipTaskbar((state & NET::SkipTaskbar) != 0, true);
    if (mask & NET::SkipPager)
        m_client->setSkipPager((state & NET::SkipPager) != 0);
    if (mask & NET::DemandsAttention)
        m_client->demandAttention((state & NET::DemandsAttention) != 0);
    if (mask & NET::Modal)
        m_client->setModal((state & NET::Modal) != 0);
    // unsetting fullscreen first, setting it last (because e.g. maximize works only for !isFullScreen() )
    if ((mask & NET::FullScreen) != 0 && (state & NET::FullScreen) != 0)
        m_client->setFullScreen(true, false);
}

void WinInfo::disable()
{
    m_client = NULL; // only used when the object is passed to Deleted
}

} // namespace
