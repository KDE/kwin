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
#include "rootinfo_filter.h"
#include "virtualdesktops.h"
#include "workspace.h"
#include "x11window.h"
// Qt
#include <QDebug>

namespace KWin
{

std::unique_ptr<RootInfo> RootInfo::s_self;

RootInfo *RootInfo::create()
{
    Q_ASSERT(!s_self);
    xcb_window_t supportWindow = xcb_generate_id(kwinApp()->x11Connection());
    const uint32_t values[] = {true};
    xcb_create_window(kwinApp()->x11Connection(), XCB_COPY_FROM_PARENT, supportWindow, kwinApp()->x11RootWindow(),
                      0, 0, 1, 1, 0, XCB_COPY_FROM_PARENT,
                      XCB_COPY_FROM_PARENT, XCB_CW_OVERRIDE_REDIRECT, values);
    const uint32_t lowerValues[] = {XCB_STACK_MODE_BELOW}; // See usage in layers.cpp
    // we need to do the lower window with a roundtrip, otherwise NETRootInfo is not functioning
    UniqueCPtr<xcb_generic_error_t> error(xcb_request_check(kwinApp()->x11Connection(),
                                                            xcb_configure_window_checked(kwinApp()->x11Connection(), supportWindow, XCB_CONFIG_WINDOW_STACK_MODE, lowerValues)));
    if (error) {
        qCDebug(KWIN_CORE) << "Error occurred while lowering support window: " << error->error_code;
    }

    const NET::Properties properties = NET::Supported
        | NET::SupportingWMCheck
        | NET::ClientList
        | NET::ClientListStacking
        | NET::DesktopGeometry
        | NET::NumberOfDesktops
        | NET::CurrentDesktop
        | NET::ActiveWindow
        | NET::WorkArea
        | NET::CloseWindow
        | NET::DesktopNames
        | NET::WMName
        | NET::WMVisibleName
        | NET::WMDesktop
        | NET::WMWindowType
        | NET::WMState
        | NET::WMStrut
        | NET::WMIconGeometry
        | NET::WMIcon
        | NET::WMPid
        | NET::WMMoveResize
        | NET::WMFrameExtents
        | NET::WMPing;
    const NET::WindowTypes types = NET::NormalMask
        | NET::DesktopMask
        | NET::DockMask
        | NET::ToolbarMask
        | NET::MenuMask
        | NET::DialogMask
        | NET::OverrideMask
        | NET::UtilityMask
        | NET::SplashMask; // No compositing window types here unless we support them also as managed window types
    const NET::States states = NET::Modal
        // | NET::Sticky // Large desktops not supported (and probably never will be)
        | NET::MaxVert
        | NET::MaxHoriz
        | NET::Shaded
        | NET::SkipTaskbar
        | NET::KeepAbove
        // | NET::StaysOnTop // The same like KeepAbove
        | NET::SkipPager
        | NET::Hidden
        | NET::FullScreen
        | NET::KeepBelow
        | NET::DemandsAttention
        | NET::SkipSwitcher
        | NET::Focused;
    NET::Properties2 properties2 = NET::WM2UserTime
        | NET::WM2StartupId
        | NET::WM2AllowedActions
        | NET::WM2RestackWindow
        | NET::WM2MoveResizeWindow
        | NET::WM2ExtendedStrut
        | NET::WM2ShowingDesktop
        | NET::WM2DesktopLayout
        | NET::WM2FullPlacement
        | NET::WM2FullscreenMonitors
        | NET::WM2KDEShadow
        | NET::WM2OpaqueRegion
        | NET::WM2GTKFrameExtents
        | NET::WM2GTKShowWindowMenu
        | NET::WM2Opacity;
#if KWIN_BUILD_ACTIVITIES
    properties2 |= NET::WM2Activities;
#endif
    const NET::Actions actions = NET::ActionMove
        | NET::ActionResize
        | NET::ActionMinimize
        | NET::ActionShade
        // | NET::ActionStick // Sticky state is not supported
        | NET::ActionMaxVert
        | NET::ActionMaxHoriz
        | NET::ActionFullScreen
        | NET::ActionChangeDesktop
        | NET::ActionClose;

    s_self = std::make_unique<RootInfo>(supportWindow, "KWin", properties, types, states, properties2, actions);
    return s_self.get();
}

void RootInfo::destroy()
{
    if (!s_self) {
        return;
    }
    xcb_window_t supportWindow = s_self->supportWindow();
    s_self.reset();
    xcb_destroy_window(kwinApp()->x11Connection(), supportWindow);
}

RootInfo::RootInfo(xcb_window_t w, const char *name, NET::Properties properties, NET::WindowTypes types,
                   NET::States states, NET::Properties2 properties2, NET::Actions actions, int scr)
    : NETRootInfo(kwinApp()->x11Connection(), w, name, properties, types, states, properties2, actions, scr)
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
    if (X11Window *c = workspace->findClient(Predicate::WindowMatch, w)) {
        if (timestamp == XCB_CURRENT_TIME) {
            timestamp = c->userTime();
        }
        if (src != NET::FromApplication && src != FromTool) {
            src = NET::FromTool;
        }
        if (src == NET::FromTool) {
            workspace->activateWindow(c, true); // force
        } else if (c == workspace->mostRecentlyActivatedWindow()) {
            return; // WORKAROUND? With > 1 plasma activities, we cause this ourselves. bug #240673
        } else { // NET::FromApplication
            X11Window *c2;
            if (c->allowWindowActivation(timestamp, false)) {
                workspace->activateWindow(c);
                // if activation of the requestor's window would be allowed, allow activation too
            } else if (active_window != XCB_WINDOW_NONE
                       && (c2 = workspace->findClient(Predicate::WindowMatch, active_window)) != nullptr
                       && c2->allowWindowActivation(timestampCompare(timestamp, c2->userTime() > 0 ? timestamp : c2->userTime()), false)) {
                workspace->activateWindow(c);
            } else {
                c->demandAttention();
            }
        }
    }
}

void RootInfo::restackWindow(xcb_window_t w, RequestSource src, xcb_window_t above, int detail, xcb_timestamp_t timestamp)
{
    if (X11Window *c = Workspace::self()->findClient(Predicate::WindowMatch, w)) {
        if (timestamp == XCB_CURRENT_TIME) {
            timestamp = c->userTime();
        }
        if (src != NET::FromApplication && src != FromTool) {
            src = NET::FromTool;
        }
        c->restackWindow(above, detail, src, timestamp);
    }
}

void RootInfo::closeWindow(xcb_window_t w)
{
    X11Window *c = Workspace::self()->findClient(Predicate::WindowMatch, w);
    if (c) {
        c->closeWindow();
    }
}

void RootInfo::moveResize(xcb_window_t w, int x_root, int y_root, unsigned long direction, xcb_button_t button, RequestSource source)
{
    X11Window *c = Workspace::self()->findClient(Predicate::WindowMatch, w);
    if (c) {
        kwinApp()->updateXTime(); // otherwise grabbing may have old timestamp - this message should include timestamp
        c->NETMoveResize(Xcb::fromXNative(x_root), Xcb::fromXNative(y_root), (Direction)direction, button);
    }
}

void RootInfo::moveResizeWindow(xcb_window_t w, int flags, int x, int y, int width, int height)
{
    X11Window *c = Workspace::self()->findClient(Predicate::WindowMatch, w);
    if (c) {
        c->NETMoveResizeWindow(flags, Xcb::fromXNative(x), Xcb::fromXNative(y), Xcb::fromXNative(width), Xcb::fromXNative(height));
    }
}

void RootInfo::showWindowMenu(xcb_window_t w, int device_id, int x_root, int y_root)
{
    if (X11Window *c = Workspace::self()->findClient(Predicate::WindowMatch, w)) {
        c->GTKShowWindowMenu(Xcb::fromXNative(x_root), Xcb::fromXNative(y_root));
    }
}

void RootInfo::gotPing(xcb_window_t w, xcb_timestamp_t timestamp)
{
    if (X11Window *c = Workspace::self()->findClient(Predicate::WindowMatch, w)) {
        c->gotPing(timestamp);
    }
}

void RootInfo::changeShowingDesktop(bool showing)
{
    Workspace::self()->setShowingDesktop(showing);
}

void RootInfo::setActiveClient(Window *client)
{
    xcb_window_t windowId = XCB_WINDOW_NONE;
    if (auto x11Window = qobject_cast<X11Window *>(client)) {
        windowId = x11Window->window();
    }
    if (m_activeWindow == windowId) {
        return;
    }
    m_activeWindow = windowId;
    setActiveWindow(m_activeWindow);
}

// ****************************************
// WinInfo
// ****************************************

WinInfo::WinInfo(X11Window *c, xcb_window_t window,
                 xcb_window_t rwin, NET::Properties properties, NET::Properties2 properties2)
    : NETWinInfo(kwinApp()->x11Connection(), window, rwin, properties, properties2, NET::WindowManager)
    , m_client(c)
{
}

void WinInfo::changeDesktop(int desktopId)
{
    if (desktopId == NET::OnAllDesktops) {
        Workspace::self()->sendWindowToDesktops(m_client, {}, true);
    } else if (VirtualDesktop *desktop = VirtualDesktopManager::self()->desktopForX11Id(desktopId)) {
        Workspace::self()->sendWindowToDesktops(m_client, {desktop}, true);
    }
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

    if ((mask & NET::FullScreen) != 0 && (state & NET::FullScreen) == 0) {
        m_client->setFullScreen(false);
    }
    if ((mask & NET::Max) == NET::Max) {
        m_client->setMaximize(state & NET::MaxVert, state & NET::MaxHoriz);
    } else if (mask & NET::MaxVert) {
        m_client->setMaximize(state & NET::MaxVert, m_client->requestedMaximizeMode() & MaximizeHorizontal);
    } else if (mask & NET::MaxHoriz) {
        m_client->setMaximize(m_client->requestedMaximizeMode() & MaximizeVertical, state & NET::MaxHoriz);
    }

    if (mask & NET::Shaded) {
        m_client->setShade(state & NET::Shaded ? ShadeNormal : ShadeNone);
    }
    if (mask & NET::KeepAbove) {
        m_client->setKeepAbove((state & NET::KeepAbove) != 0);
    }
    if (mask & NET::KeepBelow) {
        m_client->setKeepBelow((state & NET::KeepBelow) != 0);
    }
    if (mask & NET::SkipTaskbar) {
        m_client->setOriginalSkipTaskbar((state & NET::SkipTaskbar) != 0);
    }
    if (mask & NET::SkipPager) {
        m_client->setSkipPager((state & NET::SkipPager) != 0);
    }
    if (mask & NET::SkipSwitcher) {
        m_client->setSkipSwitcher((state & NET::SkipSwitcher) != 0);
    }
    if (mask & NET::DemandsAttention) {
        m_client->demandAttention((state & NET::DemandsAttention) != 0);
    }
    if (mask & NET::Modal) {
        m_client->setModal((state & NET::Modal) != 0);
    }
    // unsetting fullscreen first, setting it last (because e.g. maximize works only for !isFullScreen() )
    if ((mask & NET::FullScreen) != 0 && (state & NET::FullScreen) != 0) {
        m_client->setFullScreen(true);
    }
}

void WinInfo::disable()
{
    m_client = nullptr; // only used when the object is passed to Deleted
}

} // namespace
