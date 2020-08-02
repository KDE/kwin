/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 1999, 2000 Matthias Ettrich <ettrich@kde.org>
    SPDX-FileCopyrightText: 2003 Lubos Lunak <l.lunak@kde.org>
    SPDX-FileCopyrightText: 2013 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
// own
#include "netinfo.h"
// kwin
#include "x11client.h"
#include "rootinfo_filter.h"
#include "virtualdesktops.h"
#include "workspace.h"
// Qt
#include <QDebug>

namespace KWin
{
extern int screen_number;

RootInfo *RootInfo::s_self = nullptr;

RootInfo *RootInfo::create()
{
    Q_ASSERT(!s_self);
    xcb_window_t supportWindow = xcb_generate_id(connection());
    const uint32_t values[] = {true};
    xcb_create_window(connection(), XCB_COPY_FROM_PARENT, supportWindow, KWin::rootWindow(),
                      0, 0, 1, 1, 0, XCB_COPY_FROM_PARENT,
                      XCB_COPY_FROM_PARENT, XCB_CW_OVERRIDE_REDIRECT, values);
    const uint32_t lowerValues[] = { XCB_STACK_MODE_BELOW }; // See usage in layers.cpp
    // we need to do the lower window with a roundtrip, otherwise NETRootInfo is not functioning
    ScopedCPointer<xcb_generic_error_t> error(xcb_request_check(connection(),
        xcb_configure_window_checked(connection(), supportWindow, XCB_CONFIG_WINDOW_STACK_MODE, lowerValues)));
    if (!error.isNull()) {
        qCDebug(KWIN_CORE) << "Error occurred while lowering support window: " << error->error_code;
    }

    const NET::Properties properties = NET::Supported |
        NET::SupportingWMCheck |
        NET::ClientList |
        NET::ClientListStacking |
        NET::DesktopGeometry |
        NET::NumberOfDesktops |
        NET::CurrentDesktop |
        NET::ActiveWindow |
        NET::WorkArea |
        NET::CloseWindow |
        NET::DesktopNames |
        NET::WMName |
        NET::WMVisibleName |
        NET::WMDesktop |
        NET::WMWindowType |
        NET::WMState |
        NET::WMStrut |
        NET::WMIconGeometry |
        NET::WMIcon |
        NET::WMPid |
        NET::WMMoveResize |
        NET::WMFrameExtents |
        NET::WMPing;
    const NET::WindowTypes types = NET::NormalMask |
        NET::DesktopMask |
        NET::DockMask |
        NET::ToolbarMask |
        NET::MenuMask |
        NET::DialogMask |
        NET::OverrideMask |
        NET::UtilityMask |
        NET::SplashMask; // No compositing window types here unless we support them also as managed window types
    const NET::States states = NET::Modal |
        //NET::Sticky | // Large desktops not supported (and probably never will be)
        NET::MaxVert |
        NET::MaxHoriz |
        NET::Shaded |
        NET::SkipTaskbar |
        NET::KeepAbove |
        //NET::StaysOnTop | // The same like KeepAbove
        NET::SkipPager |
        NET::Hidden |
        NET::FullScreen |
        NET::KeepBelow |
        NET::DemandsAttention |
        NET::SkipSwitcher |
        NET::Focused;
    NET::Properties2 properties2 = NET::WM2UserTime |
        NET::WM2StartupId |
        NET::WM2AllowedActions |
        NET::WM2RestackWindow |
        NET::WM2MoveResizeWindow |
        NET::WM2ExtendedStrut |
        NET::WM2KDETemporaryRules |
        NET::WM2ShowingDesktop |
        NET::WM2DesktopLayout |
        NET::WM2FullPlacement |
        NET::WM2FullscreenMonitors |
        NET::WM2KDEShadow |
        NET::WM2OpaqueRegion |
        NET::WM2GTKFrameExtents;
#ifdef KWIN_BUILD_ACTIVITIES
        properties2 |= NET::WM2Activities;
#endif
    const NET::Actions actions = NET::ActionMove |
        NET::ActionResize |
        NET::ActionMinimize |
        NET::ActionShade |
        //NET::ActionStick | // Sticky state is not supported
        NET::ActionMaxVert |
        NET::ActionMaxHoriz |
        NET::ActionFullScreen |
        NET::ActionChangeDesktop |
        NET::ActionClose;

    s_self = new RootInfo(supportWindow, "KWin", properties, types, states, properties2, actions, screen_number);
    return s_self;
}

void RootInfo::destroy()
{
    if (!s_self) {
        return;
    }
    xcb_window_t supportWindow = s_self->supportWindow();
    delete s_self;
    s_self = nullptr;
    xcb_destroy_window(connection(), supportWindow);
}

RootInfo::RootInfo(xcb_window_t w, const char *name, NET::Properties properties, NET::WindowTypes types,
                   NET::States states, NET::Properties2 properties2, NET::Actions actions, int scr)
    : NETRootInfo(connection(), w, name, properties, types, states, properties2, actions, scr)
    , m_activeWindow(activeWindow())
    , m_eventFilter(std::make_unique<RootInfoFilter>(this))
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

void RootInfo::changeActiveWindow(xcb_window_t w, NET::RequestSource src, xcb_timestamp_t timestamp, xcb_window_t active_window)
{
    Workspace *workspace = Workspace::self();
    if (X11Client *c = workspace->findClient(Predicate::WindowMatch, w)) {
        if (timestamp == XCB_CURRENT_TIME)
            timestamp = c->userTime();
        if (src != NET::FromApplication && src != FromTool)
            src = NET::FromTool;
        if (src == NET::FromTool)
            workspace->activateClient(c, true);   // force
        else if (c == workspace->mostRecentlyActivatedClient()) {
            return; // WORKAROUND? With > 1 plasma activities, we cause this ourselves. bug #240673
        } else { // NET::FromApplication
            X11Client *c2;
            if (workspace->allowClientActivation(c, timestamp, false, true))
                workspace->activateClient(c);
            // if activation of the requestor's window would be allowed, allow activation too
            else if (active_window != XCB_WINDOW_NONE
                    && (c2 = workspace->findClient(Predicate::WindowMatch, active_window)) != nullptr
                    && workspace->allowClientActivation(c2,
                            timestampCompare(timestamp, c2->userTime() > 0 ? timestamp : c2->userTime()), false, true)) {
                workspace->activateClient(c);
            } else
                c->demandAttention();
        }
    }
}

void RootInfo::restackWindow(xcb_window_t w, RequestSource src, xcb_window_t above, int detail, xcb_timestamp_t timestamp)
{
    if (X11Client *c = Workspace::self()->findClient(Predicate::WindowMatch, w)) {
        if (timestamp == XCB_CURRENT_TIME)
            timestamp = c->userTime();
        if (src != NET::FromApplication && src != FromTool)
            src = NET::FromTool;
        c->restackWindow(above, detail, src, timestamp, true);
    }
}

void RootInfo::closeWindow(xcb_window_t w)
{
    X11Client *c = Workspace::self()->findClient(Predicate::WindowMatch, w);
    if (c)
        c->closeWindow();
}

void RootInfo::moveResize(xcb_window_t w, int x_root, int y_root, unsigned long direction)
{
    X11Client *c = Workspace::self()->findClient(Predicate::WindowMatch, w);
    if (c) {
        updateXTime(); // otherwise grabbing may have old timestamp - this message should include timestamp
        c->NETMoveResize(x_root, y_root, (Direction)direction);
    }
}

void RootInfo::moveResizeWindow(xcb_window_t w, int flags, int x, int y, int width, int height)
{
    X11Client *c = Workspace::self()->findClient(Predicate::WindowMatch, w);
    if (c)
        c->NETMoveResizeWindow(flags, x, y, width, height);
}

void RootInfo::gotPing(xcb_window_t w, xcb_timestamp_t timestamp)
{
    if (X11Client *c = Workspace::self()->findClient(Predicate::WindowMatch, w))
        c->gotPing(timestamp);
}

void RootInfo::changeShowingDesktop(bool showing)
{
    Workspace::self()->setShowingDesktop(showing);
}

void RootInfo::setActiveClient(AbstractClient *client)
{
    const xcb_window_t w = client ? client->window() : xcb_window_t{XCB_WINDOW_NONE};
    if (m_activeWindow == w) {
        return;
    }
    m_activeWindow = w;
    setActiveWindow(m_activeWindow);
}

// ****************************************
// WinInfo
// ****************************************

WinInfo::WinInfo(X11Client *c, xcb_window_t window,
                 xcb_window_t rwin, NET::Properties properties, NET::Properties2 properties2)
    : NETWinInfo(connection(), window, rwin, properties, properties2, NET::WindowManager), m_client(c)
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

void WinInfo::changeState(NET::States state, NET::States mask)
{
    mask &= ~NET::Sticky; // KWin doesn't support large desktops, ignore
    mask &= ~NET::Hidden; // clients are not allowed to change this directly
    state &= mask; // for safety, clear all other bits

    if ((mask & NET::FullScreen) != 0 && (state & NET::FullScreen) == 0)
        m_client->setFullScreen(false, false);
    if ((mask & NET::Max) == NET::Max)
        m_client->setMaximize(state & NET::MaxVert, state & NET::MaxHoriz);
    else if (mask & NET::MaxVert)
        m_client->setMaximize(state & NET::MaxVert, m_client->maximizeMode() & MaximizeHorizontal);
    else if (mask & NET::MaxHoriz)
        m_client->setMaximize(m_client->maximizeMode() & MaximizeVertical, state & NET::MaxHoriz);

    if (mask & NET::Shaded)
        m_client->setShade(state & NET::Shaded ? ShadeNormal : ShadeNone);
    if (mask & NET::KeepAbove)
        m_client->setKeepAbove((state & NET::KeepAbove) != 0);
    if (mask & NET::KeepBelow)
        m_client->setKeepBelow((state & NET::KeepBelow) != 0);
    if (mask & NET::SkipTaskbar)
        m_client->setOriginalSkipTaskbar((state & NET::SkipTaskbar) != 0);
    if (mask & NET::SkipPager)
        m_client->setSkipPager((state & NET::SkipPager) != 0);
    if (mask & NET::SkipSwitcher)
        m_client->setSkipSwitcher((state & NET::SkipSwitcher) != 0);
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
    m_client = nullptr; // only used when the object is passed to Deleted
}

} // namespace
