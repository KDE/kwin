/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 1999, 2000 Matthias Ettrich <ettrich@kde.org>
    SPDX-FileCopyrightText: 2003 Lubos Lunak <l.lunak@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

/*

 This file contains things relevant to handling incoming events.

*/

#include "atoms.h"
#include "cursor.h"
#include "focuschain.h"
#include "netinfo.h"
#include "workspace.h"
#include "x11window.h"
#if KWIN_BUILD_TABBOX
#include "tabbox.h"
#endif
#include "effects.h"
#include "group.h"
#include "rules.h"
#include "screenedge.h"
#include "unmanaged.h"
#include "useractions.h"
#include "utils/xcbutils.h"
#include "wayland/surface_interface.h"
#include "wayland/xwaylandshell_v1_interface.h"
#include "wayland_server.h"

#include <KDecoration2/Decoration>

#include <QApplication>
#include <QDebug>
#include <QHoverEvent>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QStyleHints>
#include <QWheelEvent>

#include <kkeyserver.h>

#include <xcb/damage.h>
#include <xcb/sync.h>
#if XCB_ICCCM_FOUND
#include <xcb/xcb_icccm.h>
#endif

#include "composite.h"
#include "x11eventfilter.h"

#ifndef XCB_GE_GENERIC
#define XCB_GE_GENERIC 35
typedef struct xcb_ge_generic_event_t
{
    uint8_t response_type; /**<  */
    uint8_t extension; /**<  */
    uint16_t sequence; /**<  */
    uint32_t length; /**<  */
    uint16_t event_type; /**<  */
    uint8_t pad0[22]; /**<  */
    uint32_t full_sequence; /**<  */
} xcb_ge_generic_event_t;
#endif

namespace KWin
{

// ****************************************
// Workspace
// ****************************************

static xcb_window_t findEventWindow(xcb_generic_event_t *event)
{
    const uint8_t eventType = event->response_type & ~0x80;
    switch (eventType) {
    case XCB_KEY_PRESS:
    case XCB_KEY_RELEASE:
        return reinterpret_cast<xcb_key_press_event_t *>(event)->event;
    case XCB_BUTTON_PRESS:
    case XCB_BUTTON_RELEASE:
        return reinterpret_cast<xcb_button_press_event_t *>(event)->event;
    case XCB_MOTION_NOTIFY:
        return reinterpret_cast<xcb_motion_notify_event_t *>(event)->event;
    case XCB_ENTER_NOTIFY:
    case XCB_LEAVE_NOTIFY:
        return reinterpret_cast<xcb_enter_notify_event_t *>(event)->event;
    case XCB_FOCUS_IN:
    case XCB_FOCUS_OUT:
        return reinterpret_cast<xcb_focus_in_event_t *>(event)->event;
    case XCB_EXPOSE:
        return reinterpret_cast<xcb_expose_event_t *>(event)->window;
    case XCB_GRAPHICS_EXPOSURE:
        return reinterpret_cast<xcb_graphics_exposure_event_t *>(event)->drawable;
    case XCB_NO_EXPOSURE:
        return reinterpret_cast<xcb_no_exposure_event_t *>(event)->drawable;
    case XCB_VISIBILITY_NOTIFY:
        return reinterpret_cast<xcb_visibility_notify_event_t *>(event)->window;
    case XCB_CREATE_NOTIFY:
        return reinterpret_cast<xcb_create_notify_event_t *>(event)->window;
    case XCB_DESTROY_NOTIFY:
        return reinterpret_cast<xcb_destroy_notify_event_t *>(event)->window;
    case XCB_UNMAP_NOTIFY:
        return reinterpret_cast<xcb_unmap_notify_event_t *>(event)->window;
    case XCB_MAP_NOTIFY:
        return reinterpret_cast<xcb_map_notify_event_t *>(event)->window;
    case XCB_MAP_REQUEST:
        return reinterpret_cast<xcb_map_request_event_t *>(event)->window;
    case XCB_REPARENT_NOTIFY:
        return reinterpret_cast<xcb_reparent_notify_event_t *>(event)->window;
    case XCB_CONFIGURE_NOTIFY:
        return reinterpret_cast<xcb_configure_notify_event_t *>(event)->window;
    case XCB_CONFIGURE_REQUEST:
        return reinterpret_cast<xcb_configure_request_event_t *>(event)->window;
    case XCB_GRAVITY_NOTIFY:
        return reinterpret_cast<xcb_gravity_notify_event_t *>(event)->window;
    case XCB_RESIZE_REQUEST:
        return reinterpret_cast<xcb_resize_request_event_t *>(event)->window;
    case XCB_CIRCULATE_NOTIFY:
    case XCB_CIRCULATE_REQUEST:
        return reinterpret_cast<xcb_circulate_notify_event_t *>(event)->window;
    case XCB_PROPERTY_NOTIFY:
        return reinterpret_cast<xcb_property_notify_event_t *>(event)->window;
    case XCB_COLORMAP_NOTIFY:
        return reinterpret_cast<xcb_colormap_notify_event_t *>(event)->window;
    case XCB_CLIENT_MESSAGE:
        return reinterpret_cast<xcb_client_message_event_t *>(event)->window;
    default:
        // extension handling
        if (eventType == Xcb::Extensions::self()->shapeNotifyEvent()) {
            return reinterpret_cast<xcb_shape_notify_event_t *>(event)->affected_window;
        }
        if (eventType == Xcb::Extensions::self()->damageNotifyEvent()) {
            return reinterpret_cast<xcb_damage_notify_event_t *>(event)->drawable;
        }
        return XCB_WINDOW_NONE;
    }
}

/**
 * Handles workspace specific XCB event
 */
bool Workspace::workspaceEvent(xcb_generic_event_t *e)
{
    const uint8_t eventType = e->response_type & ~0x80;

    const xcb_window_t eventWindow = findEventWindow(e);
    if (eventWindow != XCB_WINDOW_NONE) {
        if (X11Window *window = findClient(Predicate::WindowMatch, eventWindow)) {
            if (window->windowEvent(e)) {
                return true;
            }
        } else if (X11Window *window = findClient(Predicate::WrapperIdMatch, eventWindow)) {
            if (window->windowEvent(e)) {
                return true;
            }
        } else if (X11Window *window = findClient(Predicate::FrameIdMatch, eventWindow)) {
            if (window->windowEvent(e)) {
                return true;
            }
        } else if (X11Window *window = findClient(Predicate::InputIdMatch, eventWindow)) {
            if (window->windowEvent(e)) {
                return true;
            }
        } else if (Unmanaged *window = findUnmanaged(eventWindow)) {
            if (window->windowEvent(e)) {
                return true;
            }
        }
    }

    switch (eventType) {
    case XCB_CREATE_NOTIFY: {
        const auto *event = reinterpret_cast<xcb_create_notify_event_t *>(e);
        if (event->parent == kwinApp()->x11RootWindow() && !QWidget::find(event->window) && !event->override_redirect) {
            // see comments for allowWindowActivation()
            kwinApp()->updateXTime();
            const xcb_timestamp_t t = xTime();
            xcb_change_property(kwinApp()->x11Connection(), XCB_PROP_MODE_REPLACE, event->window, atoms->kde_net_wm_user_creation_time, XCB_ATOM_CARDINAL, 32, 1, &t);
        }
        break;
    }
    case XCB_UNMAP_NOTIFY: {
        const auto *event = reinterpret_cast<xcb_unmap_notify_event_t *>(e);
        return (event->event != event->window); // hide wm typical event from Qt
    }
    case XCB_REPARENT_NOTIFY: {
        // do not confuse Qt with these events. After all, _we_ are the
        // window manager who does the reparenting.
        return true;
    }
    case XCB_MAP_REQUEST: {
        kwinApp()->updateXTime();

        const auto *event = reinterpret_cast<xcb_map_request_event_t *>(e);
        if (X11Window *window = findClient(Predicate::WindowMatch, event->window)) {
            // e->xmaprequest.window is different from e->xany.window
            // TODO this shouldn't be necessary now
            window->windowEvent(e);
            m_focusChain->update(window, FocusChain::Update);
        } else if (true /*|| e->xmaprequest.parent != root */) {
            // NOTICE don't check for the parent being the root window, this breaks when some app unmaps
            // a window, changes something and immediately maps it back, without giving KWin
            // a chance to reparent it back to root
            // since KWin can get MapRequest only for root window children and
            // children of WindowWrapper (=clients), the check is AFAIK useless anyway
            // NOTICE: The save-set support in X11Window::mapRequestEvent() actually requires that
            // this code doesn't check the parent to be root.
            if (!createX11Window(event->window, false)) {
                xcb_map_window(kwinApp()->x11Connection(), event->window);
                const uint32_t values[] = {XCB_STACK_MODE_ABOVE};
                xcb_configure_window(kwinApp()->x11Connection(), event->window, XCB_CONFIG_WINDOW_STACK_MODE, values);
            }
        }
        return true;
    }
    case XCB_MAP_NOTIFY: {
        const auto *event = reinterpret_cast<xcb_map_notify_event_t *>(e);
        if (event->override_redirect) {
            Unmanaged *window = findUnmanaged(event->window);
            if (window == nullptr) {
                window = createUnmanaged(event->window);
            }
            if (window) {
                // if hasScheduledRelease is true, it means a unamp and map sequence has occurred.
                // since release is scheduled after map notify, this old Unmanaged will get released
                // before KWIN has chance to remanage it again. so release it right now.
                if (window->hasScheduledRelease()) {
                    window->release();
                    window = createUnmanaged(event->window);
                }
                if (window) {
                    return window->windowEvent(e);
                }
            }
        }
        return (event->event != event->window); // hide wm typical event from Qt
    }

    case XCB_CONFIGURE_REQUEST: {
        const auto *event = reinterpret_cast<xcb_configure_request_event_t *>(e);
        if (event->parent == kwinApp()->x11RootWindow()) {
            uint32_t values[5] = {0, 0, 0, 0, 0};
            const uint32_t value_mask = event->value_mask
                & (XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y | XCB_CONFIG_WINDOW_WIDTH | XCB_CONFIG_WINDOW_HEIGHT | XCB_CONFIG_WINDOW_BORDER_WIDTH);
            int i = 0;
            if (value_mask & XCB_CONFIG_WINDOW_X) {
                values[i++] = event->x;
            }
            if (value_mask & XCB_CONFIG_WINDOW_Y) {
                values[i++] = event->y;
            }
            if (value_mask & XCB_CONFIG_WINDOW_WIDTH) {
                values[i++] = event->width;
            }
            if (value_mask & XCB_CONFIG_WINDOW_HEIGHT) {
                values[i++] = event->height;
            }
            if (value_mask & XCB_CONFIG_WINDOW_BORDER_WIDTH) {
                values[i++] = event->border_width;
            }
            xcb_configure_window(kwinApp()->x11Connection(), event->window, value_mask, values);
            return true;
        }
        break;
    }
    case XCB_CONFIGURE_NOTIFY: {
        const auto configureNotifyEvent = reinterpret_cast<xcb_configure_notify_event_t *>(e);
        if (configureNotifyEvent->override_redirect && configureNotifyEvent->event == kwinApp()->x11RootWindow()) {
            updateXStackingOrder();
        }
        break;
    }
    case XCB_FOCUS_IN: {
        const auto *event = reinterpret_cast<xcb_focus_in_event_t *>(e);
        if (event->event == kwinApp()->x11RootWindow()
            && (event->detail == XCB_NOTIFY_DETAIL_NONE || event->detail == XCB_NOTIFY_DETAIL_POINTER_ROOT || event->detail == XCB_NOTIFY_DETAIL_INFERIOR)) {
            Xcb::CurrentInput currentInput;
            kwinApp()->updateXTime(); // focusToNull() uses xTime(), which is old now (FocusIn has no timestamp)
            // it seems we can "loose" focus reversions when the closing window hold a grab
            // => catch the typical pattern (though we don't want the focus on the root anyway) #348935
            const bool lostFocusPointerToRoot = currentInput->focus == kwinApp()->x11RootWindow() && event->detail == XCB_NOTIFY_DETAIL_INFERIOR;
            if (!currentInput.isNull() && (currentInput->focus == XCB_WINDOW_NONE || currentInput->focus == XCB_INPUT_FOCUS_POINTER_ROOT || lostFocusPointerToRoot)) {
                // kWarning( 1212 ) << "X focus set to None/PointerRoot, reseting focus" ;
                Window *window = mostRecentlyActivatedWindow();
                if (window != nullptr) {
                    requestFocus(window, true);
                } else if (activateNextWindow(nullptr)) {
                    ; // ok, activated
                } else {
                    focusToNull();
                }
            }
        }
    }
        // fall through
    case XCB_FOCUS_OUT:
        return true; // always eat these, they would tell Qt that KWin is the active app
    default:
        break;
    }
    return false;
}

// ****************************************
// Client
// ****************************************

/**
 * General handler for XEvents concerning the client window
 */
bool X11Window::windowEvent(xcb_generic_event_t *e)
{
    if (findEventWindow(e) == window()) { // avoid doing stuff on frame or wrapper
        NET::Properties dirtyProperties;
        NET::Properties2 dirtyProperties2;
        info->event(e, &dirtyProperties, &dirtyProperties2); // pass through the NET stuff

        if ((dirtyProperties & NET::WMName) != 0) {
            fetchName();
        }
        if ((dirtyProperties & NET::WMIconName) != 0) {
            fetchIconicName();
        }
        if ((dirtyProperties & NET::WMStrut) != 0
            || (dirtyProperties2 & NET::WM2ExtendedStrut) != 0) {
            workspace()->updateClientArea();
        }
        if ((dirtyProperties & NET::WMIcon) != 0) {
            getIcons();
        }
        // Note there's a difference between userTime() and info->userTime()
        // info->userTime() is the value of the property, userTime() also includes
        // updates of the time done by KWin (ButtonPress on windowrapper etc.).
        if ((dirtyProperties2 & NET::WM2UserTime) != 0) {
            workspace()->setWasUserInteraction();
            updateUserTime(info->userTime());
        }
        if ((dirtyProperties2 & NET::WM2StartupId) != 0) {
            startupIdChanged();
        }
        if (dirtyProperties2 & NET::WM2Opacity) {
            if (Compositor::compositing()) {
                setOpacity(info->opacityF());
            } else {
                // forward to the frame if there's possibly another compositing manager running
                NETWinInfo i(kwinApp()->x11Connection(), frameId(), kwinApp()->x11RootWindow(), NET::Properties(), NET::Properties2());
                i.setOpacity(info->opacity());
            }
        }
        if (dirtyProperties2.testFlag(NET::WM2WindowRole)) {
            Q_EMIT windowRoleChanged();
        }
        if (dirtyProperties2.testFlag(NET::WM2WindowClass)) {
            getResourceClass();
        }
        if (dirtyProperties2.testFlag(NET::WM2BlockCompositing)) {
            setBlockingCompositing(info->isBlockingCompositing());
        }
        if (dirtyProperties2.testFlag(NET::WM2GroupLeader)) {
            checkGroup();
            updateAllowedActions(); // Group affects isMinimizable()
        }
        if (dirtyProperties2.testFlag(NET::WM2Urgency)) {
            updateUrgency();
        }
        if (dirtyProperties2 & NET::WM2OpaqueRegion) {
            getWmOpaqueRegion();
        }
        if (dirtyProperties2 & NET::WM2DesktopFileName) {
            setDesktopFileName(QString::fromUtf8(info->desktopFileName()));
        }
        if (dirtyProperties2 & NET::WM2GTKFrameExtents) {
            setClientFrameExtents(info->gtkFrameExtents());
        }
    }

    const uint8_t eventType = e->response_type & ~0x80;
    switch (eventType) {
    case XCB_UNMAP_NOTIFY:
        unmapNotifyEvent(reinterpret_cast<xcb_unmap_notify_event_t *>(e));
        break;
    case XCB_DESTROY_NOTIFY:
        destroyNotifyEvent(reinterpret_cast<xcb_destroy_notify_event_t *>(e));
        break;
    case XCB_MAP_REQUEST:
        // this one may pass the event to workspace
        return mapRequestEvent(reinterpret_cast<xcb_map_request_event_t *>(e));
    case XCB_CONFIGURE_REQUEST:
        configureRequestEvent(reinterpret_cast<xcb_configure_request_event_t *>(e));
        break;
    case XCB_PROPERTY_NOTIFY:
        propertyNotifyEvent(reinterpret_cast<xcb_property_notify_event_t *>(e));
        break;
    case XCB_KEY_PRESS:
        updateUserTime(reinterpret_cast<xcb_key_press_event_t *>(e)->time);
        break;
    case XCB_BUTTON_PRESS: {
        const auto *event = reinterpret_cast<xcb_button_press_event_t *>(e);
        updateUserTime(event->time);
        buttonPressEvent(event->event, event->detail, event->state,
                         event->event_x, event->event_y, event->root_x, event->root_y, event->time);
        break;
    }
    case XCB_KEY_RELEASE:
        // don't update user time on releases
        // e.g. if the user presses Alt+F2, the Alt release
        // would appear as user input to the currently active window
        break;
    case XCB_BUTTON_RELEASE: {
        const auto *event = reinterpret_cast<xcb_button_release_event_t *>(e);
        // don't update user time on releases
        // e.g. if the user presses Alt+F2, the Alt release
        // would appear as user input to the currently active window
        buttonReleaseEvent(event->event, event->detail, event->state,
                           event->event_x, event->event_y, event->root_x, event->root_y);
        break;
    }
    case XCB_MOTION_NOTIFY: {
        const auto *event = reinterpret_cast<xcb_motion_notify_event_t *>(e);

        int x = Xcb::fromXNative(event->event_x);
        int y = Xcb::fromXNative(event->event_y);
        int root_x = Xcb::fromXNative(event->root_x);
        int root_y = Xcb::fromXNative(event->root_y);

        motionNotifyEvent(event->event, event->state,
                          x, y, root_x, root_y);
        workspace()->updateFocusMousePosition(QPointF(root_x, root_y));
        break;
    }
    case XCB_ENTER_NOTIFY: {
        auto *event = reinterpret_cast<xcb_enter_notify_event_t *>(e);
        enterNotifyEvent(event);
        // MotionNotify is guaranteed to be generated only if the mouse
        // move start and ends in the window; for cases when it only
        // starts or only ends there, Enter/LeaveNotify are generated.
        // Fake a MotionEvent in such cases to make handle of mouse
        // events simpler (Qt does that too).
        int x = Xcb::fromXNative(event->event_x);
        int y = Xcb::fromXNative(event->event_y);
        int root_x = Xcb::fromXNative(event->root_x);
        int root_y = Xcb::fromXNative(event->root_y);

        motionNotifyEvent(event->event, event->state,
                          x, y, root_x, root_y);
        workspace()->updateFocusMousePosition(QPointF(root_x, root_y));
        break;
    }
    case XCB_LEAVE_NOTIFY: {
        auto *event = reinterpret_cast<xcb_leave_notify_event_t *>(e);

        int x = Xcb::fromXNative(event->event_x);
        int y = Xcb::fromXNative(event->event_y);
        int root_x = Xcb::fromXNative(event->root_x);
        int root_y = Xcb::fromXNative(event->root_y);
        motionNotifyEvent(event->event, event->state,
                          x, y, root_x, root_y);
        leaveNotifyEvent(event);
        // not here, it'd break following enter notify handling
        // workspace()->updateFocusMousePosition( QPoint( e->xcrossing.x_root, e->xcrossing.y_root ));
        break;
    }
    case XCB_FOCUS_IN:
        focusInEvent(reinterpret_cast<xcb_focus_in_event_t *>(e));
        break;
    case XCB_FOCUS_OUT:
        focusOutEvent(reinterpret_cast<xcb_focus_out_event_t *>(e));
        break;
    case XCB_REPARENT_NOTIFY:
        break;
    case XCB_CLIENT_MESSAGE:
        clientMessageEvent(reinterpret_cast<xcb_client_message_event_t *>(e));
        break;
    case XCB_EXPOSE: {
        xcb_expose_event_t *event = reinterpret_cast<xcb_expose_event_t *>(e);
        if (event->window == frameId() && !Compositor::self()->isActive()) {
            // TODO: only repaint required areas
            triggerDecorationRepaint();
        }
        break;
    }
    default:
        if (eventType == Xcb::Extensions::self()->shapeNotifyEvent() && reinterpret_cast<xcb_shape_notify_event_t *>(e)->affected_window == window()) {
            detectShape(window()); // workaround for #19644
            updateShape();
        }
        if (eventType == Xcb::Extensions::self()->damageNotifyEvent() && reinterpret_cast<xcb_damage_notify_event_t *>(e)->drawable == frameId()) {
            damageNotifyEvent();
        }
        break;
    }
    return true; // eat all events
}

/**
 * Handles map requests of the client window
 */
bool X11Window::mapRequestEvent(xcb_map_request_event_t *e)
{
    if (e->window != window()) {
        // Special support for the save-set feature, which is a bit broken.
        // If there's a window from one client embedded in another one,
        // e.g. using XEMBED, and the embedder suddenly loses its X connection,
        // save-set will reparent the embedded window to its closest ancestor
        // that will remains. Unfortunately, with reparenting window managers,
        // this is not the root window, but the frame (or in KWin's case,
        // it's the wrapper for the client window). In this case,
        // the wrapper will get ReparentNotify for a window it won't know,
        // which will be ignored, and then it gets MapRequest, as save-set
        // always maps. Returning true here means that Workspace::workspaceEvent()
        // will handle this MapRequest and manage this window (i.e. act as if
        // it was reparented to root window).
        if (e->parent == wrapperId()) {
            return false;
        }
        return true; // no messing with frame etc.
    }
    // also copied in clientMessage()
    if (isMinimized()) {
        unminimize();
    }
    if (isShade()) {
        setShade(ShadeNone);
    }
    if (!isOnCurrentDesktop()) {
        if (allowWindowActivation()) {
            workspace()->activateWindow(this);
        } else {
            demandAttention();
        }
    }
    return true;
}

/**
 * Handles unmap notify events of the client window
 */
void X11Window::unmapNotifyEvent(xcb_unmap_notify_event_t *e)
{
    if (e->window != window()) {
        return;
    }
    if (e->event != wrapperId()) {
        // most probably event from root window when initially reparenting
        bool ignore = true;
        if (e->event == kwinApp()->x11RootWindow() && (e->response_type & 0x80)) {
            ignore = false; // XWithdrawWindow()
        }
        if (ignore) {
            return;
        }
    }

    // check whether this is result of an XReparentWindow - window then won't be parented by wrapper
    // in this case do not release the window (causes reparent to root, removal from saveSet and what not)
    // but just destroy the window
    Xcb::Tree tree(m_client);
    xcb_window_t daddy = tree.parent();
    if (daddy == m_wrapper) {
        releaseWindow(); // unmapped from a regular window state
    } else {
        destroyWindow(); // the window was moved to some other parent
    }
}

void X11Window::destroyNotifyEvent(xcb_destroy_notify_event_t *e)
{
    if (e->window != window()) {
        return;
    }
    destroyWindow();
}

/**
 * Handles client messages for the client window
 */
void X11Window::clientMessageEvent(xcb_client_message_event_t *e)
{
    Window::clientMessageEvent(e);
    if (e->window != window()) {
        return; // ignore frame/wrapper
    }
    // WM_STATE
    if (e->type == atoms->wm_change_state) {
        if (e->data.data32[0] == XCB_ICCCM_WM_STATE_ICONIC) {
            minimize();
        }
        return;
    }
}

/**
 * Handles configure  requests of the client window
 */
void X11Window::configureRequestEvent(xcb_configure_request_event_t *e)
{
    if (e->window != window()) {
        return; // ignore frame/wrapper
    }
    if (isInteractiveResize() || isInteractiveMove()) {
        return; // we have better things to do right now
    }

    if (m_fullscreenMode == FullScreenNormal) { // refuse resizing of fullscreen windows
        // but allow resizing fullscreen hacks in order to let them cancel fullscreen mode
        sendSyntheticConfigureNotify();
        return;
    }
    if (isSplash()) { // no manipulations with splashscreens either
        sendSyntheticConfigureNotify();
        return;
    }

    if (e->value_mask & XCB_CONFIG_WINDOW_BORDER_WIDTH) {
        // first, get rid of a window border
        m_client.setBorderWidth(0);
    }

    if (e->value_mask & (XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y | XCB_CONFIG_WINDOW_HEIGHT | XCB_CONFIG_WINDOW_WIDTH)) {
        configureRequest(e->value_mask, Xcb::fromXNative(e->x),
                         Xcb::fromXNative(e->y), Xcb::fromXNative(e->width), Xcb::fromXNative(e->height), 0, false);
    }
    if (e->value_mask & XCB_CONFIG_WINDOW_STACK_MODE) {
        restackWindow(e->sibling, e->stack_mode, NET::FromApplication, userTime(), false);
    }

    // Sending a synthetic configure notify always is fine, even in cases where
    // the ICCCM doesn't require this - it can be though of as 'the WM decided to move
    // the window later'. The window should not cause that many configure request,
    // so this should not have any significant impact. With user moving/resizing
    // the it should be optimized though (see also X11Window::setGeometry()/resize()/move()).
    sendSyntheticConfigureNotify();

    // SELI TODO accept configure requests for isDesktop windows (because kdesktop
    // may get XRANDR resize event before kwin), but check it's still at the bottom?
}

/**
 * Handles property changes of the client window
 */
void X11Window::propertyNotifyEvent(xcb_property_notify_event_t *e)
{
    Window::propertyNotifyEvent(e);
    if (e->window != window()) {
        return; // ignore frame/wrapper
    }
    switch (e->atom) {
    case XCB_ATOM_WM_NORMAL_HINTS:
        getWmNormalHints();
        break;
    case XCB_ATOM_WM_NAME:
        fetchName();
        break;
    case XCB_ATOM_WM_ICON_NAME:
        fetchIconicName();
        break;
    case XCB_ATOM_WM_TRANSIENT_FOR:
        readTransient();
        break;
    case XCB_ATOM_WM_HINTS:
        getIcons(); // because KWin::icon() uses WMHints as fallback
        break;
    default:
        if (e->atom == atoms->motif_wm_hints) {
            getMotifHints();
        } else if (e->atom == atoms->net_wm_sync_request_counter) {
            getSyncCounter();
        } else if (e->atom == atoms->activities) {
            checkActivities();
        } else if (e->atom == atoms->kde_first_in_window_list) {
            updateFirstInTabBox();
        } else if (e->atom == atoms->kde_color_sheme) {
            updateColorScheme();
        } else if (e->atom == atoms->kde_screen_edge_show) {
            updateShowOnScreenEdge();
        } else if (e->atom == atoms->kde_net_wm_appmenu_service_name) {
            checkApplicationMenuServiceName();
        } else if (e->atom == atoms->kde_net_wm_appmenu_object_path) {
            checkApplicationMenuObjectPath();
        }
        break;
    }
}

void X11Window::enterNotifyEvent(xcb_enter_notify_event_t *e)
{
    if (waylandServer()) {
        return;
    }
    if (e->event != frameId()) {
        return; // care only about entering the whole frame
    }

    const bool mouseDrivenFocus = !options->focusPolicyIsReasonable() || (options->focusPolicy() == Options::FocusFollowsMouse && options->isNextFocusPrefersMouse());
    if (e->mode == XCB_NOTIFY_MODE_NORMAL || (e->mode == XCB_NOTIFY_MODE_UNGRAB && mouseDrivenFocus)) {
        pointerEnterEvent(QPoint(e->root_x, e->root_y));
        return;
    }
}

void X11Window::leaveNotifyEvent(xcb_leave_notify_event_t *e)
{
    if (waylandServer()) {
        return;
    }
    if (e->event != frameId()) {
        return; // care only about leaving the whole frame
    }
    if (e->mode == XCB_NOTIFY_MODE_NORMAL) {
        if (!isInteractiveMoveResizePointerButtonDown()) {
            setInteractiveMoveResizeGravity(Gravity::None);
            updateCursor();
        }
        bool lostMouse = !rect().contains(QPoint(e->event_x, e->event_y));
        // 'lostMouse' wouldn't work with e.g. B2 or Keramik, which have non-rectangular decorations
        // (i.e. the LeaveNotify event comes before leaving the rect and no LeaveNotify event
        // comes after leaving the rect) - so lets check if the pointer is really outside the window

        // TODO this still sucks if a window appears above this one - it should lose the mouse
        // if this window is another window, but not if it's a popup ... maybe after KDE3.1 :(
        // (repeat after me 'AARGHL!')
        if (!lostMouse && e->detail != XCB_NOTIFY_DETAIL_INFERIOR) {
            Xcb::Pointer pointer(frameId());
            if (!pointer || !pointer->same_screen || pointer->child == XCB_WINDOW_NONE) {
                // really lost the mouse
                lostMouse = true;
            }
        }
        if (lostMouse) {
            pointerLeaveEvent();
            if (isDecorated()) {
                // sending a move instead of a leave. With leave we need to send proper coords, with move it's handled internally
                QHoverEvent leaveEvent(QEvent::HoverMove, QPointF(-1, -1), QPointF(-1, -1), Qt::NoModifier);
                QCoreApplication::sendEvent(decoration(), &leaveEvent);
            }
        }
        if (options->focusPolicy() == Options::FocusStrictlyUnderMouse && isActive() && lostMouse) {
            workspace()->requestDelayFocus(nullptr);
        }
        return;
    }
}

static uint16_t x11CommandAllModifier()
{
    switch (options->commandAllModifier()) {
    case Qt::MetaModifier:
        return KKeyServer::modXMeta();
    case Qt::AltModifier:
        return KKeyServer::modXAlt();
    default:
        return 0;
    }
}

#define XCapL KKeyServer::modXLock()
#define XNumL KKeyServer::modXNumLock()
#define XScrL KKeyServer::modXScrollLock()
void X11Window::establishCommandWindowGrab(uint8_t button)
{
    // Unfortunately there are a lot of possible modifier combinations that we need to take into
    // account. We tackle that problem in a kind of smart way. First, we grab the button with all
    // possible modifiers, then we ungrab the ones that are relevant only to commandAllx().

    m_wrapper.grabButton(XCB_GRAB_MODE_SYNC, XCB_GRAB_MODE_ASYNC, XCB_MOD_MASK_ANY, button);

    uint16_t x11Modifier = x11CommandAllModifier();

    unsigned int mods[8] = {
        0, XCapL, XNumL, XNumL | XCapL,
        XScrL, XScrL | XCapL,
        XScrL | XNumL, XScrL | XNumL | XCapL};
    for (int i = 0; i < 8; ++i) {
        m_wrapper.ungrabButton(x11Modifier | mods[i], button);
    }
}

void X11Window::establishCommandAllGrab(uint8_t button)
{
    uint16_t x11Modifier = x11CommandAllModifier();

    unsigned int mods[8] = {
        0, XCapL, XNumL, XNumL | XCapL,
        XScrL, XScrL | XCapL,
        XScrL | XNumL, XScrL | XNumL | XCapL};
    for (int i = 0; i < 8; ++i) {
        m_wrapper.grabButton(XCB_GRAB_MODE_SYNC, XCB_GRAB_MODE_ASYNC, x11Modifier | mods[i], button);
    }
}
#undef XCapL
#undef XNumL
#undef XScrL

void X11Window::updateMouseGrab()
{
    if (waylandServer()) {
        return;
    }

    xcb_ungrab_button(kwinApp()->x11Connection(), XCB_BUTTON_INDEX_ANY, m_wrapper, XCB_MOD_MASK_ANY);

#if KWIN_BUILD_TABBOX
    if (workspace()->tabbox()->forcedGlobalMouseGrab()) { // see TabBox::establishTabBoxGrab()
        m_wrapper.grabButton(XCB_GRAB_MODE_SYNC, XCB_GRAB_MODE_ASYNC);
        return;
    }
#endif

    // When a passive grab is activated or deactivated, the X server will generate crossing
    // events as if the pointer were suddenly to warp from its current position to some position
    // in the grab window. Some /broken/ X11 clients do get confused by such EnterNotify and
    // LeaveNotify events so we release the passive grab for the active window.
    //
    // The passive grab below is established so the window can be raised or activated when it
    // is clicked.
    if ((options->focusPolicyIsReasonable() && !isActive()) || (options->isClickRaise() && !isMostRecentlyRaised())) {
        if (options->commandWindow1() != Options::MouseNothing) {
            establishCommandWindowGrab(XCB_BUTTON_INDEX_1);
        }
        if (options->commandWindow2() != Options::MouseNothing) {
            establishCommandWindowGrab(XCB_BUTTON_INDEX_2);
        }
        if (options->commandWindow3() != Options::MouseNothing) {
            establishCommandWindowGrab(XCB_BUTTON_INDEX_3);
        }
        if (options->commandWindowWheel() != Options::MouseNothing) {
            establishCommandWindowGrab(XCB_BUTTON_INDEX_4);
            establishCommandWindowGrab(XCB_BUTTON_INDEX_5);
        }
    }

    // We want to grab <command modifier> + buttons no matter what state the window is in. The
    // window will receive funky EnterNotify and LeaveNotify events, but there is nothing that
    // we can do about it, unfortunately.

    if (!workspace()->globalShortcutsDisabled()) {
        if (options->commandAll1() != Options::MouseNothing) {
            establishCommandAllGrab(XCB_BUTTON_INDEX_1);
        }
        if (options->commandAll2() != Options::MouseNothing) {
            establishCommandAllGrab(XCB_BUTTON_INDEX_2);
        }
        if (options->commandAll3() != Options::MouseNothing) {
            establishCommandAllGrab(XCB_BUTTON_INDEX_3);
        }
        if (options->commandAllWheel() != Options::MouseWheelNothing) {
            establishCommandAllGrab(XCB_BUTTON_INDEX_4);
            establishCommandAllGrab(XCB_BUTTON_INDEX_5);
        }
    }
}

static bool modKeyDown(int state)
{
    const uint keyModX = (options->keyCmdAllModKey() == Qt::Key_Meta) ? KKeyServer::modXMeta() : KKeyServer::modXAlt();
    return keyModX && (state & KKeyServer::accelModMaskX()) == keyModX;
}

// return value matters only when filtering events before decoration gets them
bool X11Window::buttonPressEvent(xcb_window_t w, int button, int state, int x, int y, int x_root, int y_root, xcb_timestamp_t time)
{
    if (waylandServer()) {
        return true;
    }
    if (isInteractiveMoveResizePointerButtonDown()) {
        if (w == wrapperId()) {
            xcb_allow_events(kwinApp()->x11Connection(), XCB_ALLOW_SYNC_POINTER, XCB_TIME_CURRENT_TIME); // xTime());
        }
        return true;
    }

    if (w == wrapperId() || w == frameId() || w == inputId()) {
        // FRAME neco s tohohle by se melo zpracovat, nez to dostane dekorace
        updateUserTime(time);
        const bool bModKeyHeld = modKeyDown(state);

        if (isSplash()
            && button == XCB_BUTTON_INDEX_1 && !bModKeyHeld) {
            // hide splashwindow if the user clicks on it
            hideClient();
            if (w == wrapperId()) {
                xcb_allow_events(kwinApp()->x11Connection(), XCB_ALLOW_SYNC_POINTER, XCB_TIME_CURRENT_TIME); // xTime());
            }
            return true;
        }

        Options::MouseCommand com = Options::MouseNothing;
        bool was_action = false;
        if (bModKeyHeld) {
            was_action = true;
            switch (button) {
            case XCB_BUTTON_INDEX_1:
                com = options->commandAll1();
                break;
            case XCB_BUTTON_INDEX_2:
                com = options->commandAll2();
                break;
            case XCB_BUTTON_INDEX_3:
                com = options->commandAll3();
                break;
            case XCB_BUTTON_INDEX_4:
            case XCB_BUTTON_INDEX_5:
                com = options->operationWindowMouseWheel(button == XCB_BUTTON_INDEX_4 ? 120 : -120);
                break;
            }
        } else {
            if (w == wrapperId()) {
                if (button < 4) {
                    com = getMouseCommand(x11ToQtMouseButton(button), &was_action);
                } else if (button < 6) {
                    com = getWheelCommand(Qt::Vertical, &was_action);
                }
            }
        }
        if (was_action) {
            bool replay = performMouseCommand(com, QPoint(x_root, y_root));

            if (isSpecialWindow()) {
                replay = true;
            }

            if (w == wrapperId()) { // these can come only from a grab
                xcb_allow_events(kwinApp()->x11Connection(), replay ? XCB_ALLOW_REPLAY_POINTER : XCB_ALLOW_SYNC_POINTER, XCB_TIME_CURRENT_TIME); // xTime());
            }
            return true;
        }
    }

    if (w == wrapperId()) { // these can come only from a grab
        xcb_allow_events(kwinApp()->x11Connection(), XCB_ALLOW_REPLAY_POINTER, XCB_TIME_CURRENT_TIME); // xTime());
        return true;
    }
    if (w == inputId()) {
        x = x_root - frameGeometry().x();
        y = y_root - frameGeometry().y();
        // New API processes core events FIRST and only passes unused ones to the decoration
        QMouseEvent ev(QMouseEvent::MouseButtonPress, QPoint(x, y), QPoint(x_root, y_root),
                       x11ToQtMouseButton(button), x11ToQtMouseButtons(state), Qt::KeyboardModifiers());
        return processDecorationButtonPress(&ev, true);
    }
    if (w == frameId() && isDecorated()) {
        if (button >= 4 && button <= 7) {
            const Qt::KeyboardModifiers modifiers = x11ToQtKeyboardModifiers(state);
            // Logic borrowed from qapplication_x11.cpp
            const int delta = 120 * ((button == 4 || button == 6) ? 1 : -1);
            const bool hor = (((button == 4 || button == 5) && (modifiers & Qt::AltModifier))
                              || (button == 6 || button == 7));

            const QPoint angle = hor ? QPoint(delta, 0) : QPoint(0, delta);
            QWheelEvent event(QPointF(x, y),
                              QPointF(x_root, y_root),
                              QPoint(),
                              angle,
                              x11ToQtMouseButtons(state),
                              modifiers,
                              Qt::NoScrollPhase,
                              false);
            event.setAccepted(false);
            QCoreApplication::sendEvent(decoration(), &event);
            if (!event.isAccepted() && !hor) {
                if (titlebarPositionUnderMouse()) {
                    performMouseCommand(options->operationTitlebarMouseWheel(delta), QPoint(x_root, y_root));
                }
            }
        } else {
            QMouseEvent event(QEvent::MouseButtonPress, QPointF(x, y), QPointF(x_root, y_root),
                              x11ToQtMouseButton(button), x11ToQtMouseButtons(state), x11ToQtKeyboardModifiers(state));
            event.setTimestamp(time);
            event.setAccepted(false);
            QCoreApplication::sendEvent(decoration(), &event);
            if (!event.isAccepted()) {
                processDecorationButtonPress(&event);
            }
        }
        return true;
    }
    return true;
}

// return value matters only when filtering events before decoration gets them
bool X11Window::buttonReleaseEvent(xcb_window_t w, int button, int state, int x, int y, int x_root, int y_root)
{
    if (waylandServer()) {
        return true;
    }
    if (w == frameId() && isDecorated()) {
        // wheel handled on buttonPress
        if (button < 4 || button > 7) {
            QMouseEvent event(QEvent::MouseButtonRelease,
                              QPointF(x, y),
                              QPointF(x_root, y_root),
                              x11ToQtMouseButton(button),
                              x11ToQtMouseButtons(state) & ~x11ToQtMouseButton(button),
                              x11ToQtKeyboardModifiers(state));
            event.setAccepted(false);
            QCoreApplication::sendEvent(decoration(), &event);
            if (event.isAccepted() || !titlebarPositionUnderMouse()) {
                invalidateDecorationDoubleClickTimer(); // click was for the deco and shall not init a doubleclick
            }
        }
    }
    if (w == wrapperId()) {
        xcb_allow_events(kwinApp()->x11Connection(), XCB_ALLOW_SYNC_POINTER, XCB_TIME_CURRENT_TIME); // xTime());
        return true;
    }
    if (w != frameId() && w != inputId() && w != moveResizeGrabWindow()) {
        return true;
    }
    if (w == frameId() && workspace()->userActionsMenu() && workspace()->userActionsMenu()->isShown()) {
        const_cast<UserActionsMenu *>(workspace()->userActionsMenu())->grabInput();
    }
    x = this->x(); // translate from grab window to local coords
    y = this->y();

    // Check whether other buttons are still left pressed
    int buttonMask = XCB_BUTTON_MASK_1 | XCB_BUTTON_MASK_2 | XCB_BUTTON_MASK_3;
    if (button == XCB_BUTTON_INDEX_1) {
        buttonMask &= ~XCB_BUTTON_MASK_1;
    } else if (button == XCB_BUTTON_INDEX_2) {
        buttonMask &= ~XCB_BUTTON_MASK_2;
    } else if (button == XCB_BUTTON_INDEX_3) {
        buttonMask &= ~XCB_BUTTON_MASK_3;
    }

    if ((state & buttonMask) == 0) {
        endInteractiveMoveResize();
    }
    return true;
}

// return value matters only when filtering events before decoration gets them
bool X11Window::motionNotifyEvent(xcb_window_t w, int state, int x, int y, int x_root, int y_root)
{
    if (waylandServer()) {
        return true;
    }
    if (w == frameId() && isDecorated() && !isMinimized()) {
        // TODO Mouse move event dependent on state
        QHoverEvent event(QEvent::HoverMove, QPointF(x, y), QPointF(x, y));
        QCoreApplication::instance()->sendEvent(decoration(), &event);
    }
    if (w != frameId() && w != inputId() && w != moveResizeGrabWindow()) {
        return true; // care only about the whole frame
    }
    if (!isInteractiveMoveResizePointerButtonDown()) {
        if (w == inputId()) {
            int x = x_root - frameGeometry().x(); // + padding_left;
            int y = y_root - frameGeometry().y(); // + padding_top;

            if (isDecorated()) {
                QHoverEvent event(QEvent::HoverMove, QPointF(x, y), QPointF(x, y));
                QCoreApplication::instance()->sendEvent(decoration(), &event);
            }
        }
        Gravity newGravity = modKeyDown(state) ? Gravity::None : mouseGravity();
        if (newGravity != interactiveMoveResizeGravity()) {
            setInteractiveMoveResizeGravity(newGravity);
            updateCursor();
        }
        return false;
    }
    if (w == moveResizeGrabWindow()) {
        x = this->x(); // translate from grab window to local coords
        y = this->y();
    }

    handleInteractiveMoveResize(QPoint(x, y), QPoint(x_root, y_root));
    if (isInteractiveMove()) {
        workspace()->screenEdges()->check(QPoint(x_root, y_root), QDateTime::fromMSecsSinceEpoch(xTime(), Qt::UTC));
    }

    return true;
}

void X11Window::focusInEvent(xcb_focus_in_event_t *e)
{
    if (e->event != window()) {
        return; // only window gets focus
    }
    if (e->mode == XCB_NOTIFY_MODE_UNGRAB) {
        return; // we don't care
    }
    if (e->detail == XCB_NOTIFY_DETAIL_POINTER) {
        return; // we don't care
    }
    if (isShade() || !isShown() || !isOnCurrentDesktop()) { // we unmapped it, but it got focus meanwhile ->
        return; // activateNextWindow() already transferred focus elsewhere
    }
    workspace()->forEachClient([](X11Window *window) {
        window->cancelFocusOutTimer();
    });
    // check if this window is in should_get_focus list or if activation is allowed
    bool activate = allowWindowActivation(-1U, true);
    workspace()->gotFocusIn(this); // remove from should_get_focus list
    if (activate) {
        setActive(true);
    } else {
        if (workspace()->restoreFocus()) {
            demandAttention();
        } else {
            qCWarning(KWIN_CORE, "Failed to restore focus. Activating 0x%x", window());
            setActive(true);
        }
    }
}

void X11Window::focusOutEvent(xcb_focus_out_event_t *e)
{
    if (e->event != window()) {
        return; // only window gets focus
    }
    if (e->mode == XCB_NOTIFY_MODE_GRAB) {
        return; // we don't care
    }
    if (isShade()) {
        return; // here neither
    }
    if (e->detail != XCB_NOTIFY_DETAIL_NONLINEAR
        && e->detail != XCB_NOTIFY_DETAIL_NONLINEAR_VIRTUAL) {
        // SELI check all this
        return; // hack for motif apps like netscape
    }
    if (QApplication::activePopupWidget()) {
        return;
    }

    // When a window loses focus, FocusOut events are usually immediatelly
    // followed by FocusIn events for another window that gains the focus
    // (unless the focus goes to another screen, or to the nofocus widget).
    // Without this check, the former focused window would have to be
    // deactivated, and after that, the new one would be activated, with
    // a short time when there would be no active window. This can cause
    // flicker sometimes, e.g. when a fullscreen is shown, and focus is transferred
    // from it to its transient, the fullscreen would be kept in the Active layer
    // at the beginning and at the end, but not in the middle, when the active
    // window would be temporarily none (see X11Window::belongToLayer() ).
    // Therefore the setActive(false) call is moved to the end of the current
    // event queue. If there is a matching FocusIn event in the current queue
    // this will be processed before the setActive(false) call and the activation
    // of the window which gained FocusIn will automatically deactivate the
    // previously active window.
    if (!m_focusOutTimer) {
        m_focusOutTimer = new QTimer(this);
        m_focusOutTimer->setSingleShot(true);
        m_focusOutTimer->setInterval(0);
        connect(m_focusOutTimer, &QTimer::timeout, this, [this]() {
            setActive(false);
        });
    }
    m_focusOutTimer->start();
}

// performs _NET_WM_MOVERESIZE
void X11Window::NETMoveResize(qreal x_root, qreal y_root, NET::Direction direction)
{
    if (direction == NET::Move) {
        // move cursor to the provided position to prevent the window jumping there on first movement
        // the expectation is that the cursor is already at the provided position,
        // thus it's more a safety measurement
        Cursors::self()->mouse()->setPos(QPointF(x_root, y_root));
        performMouseCommand(Options::MouseMove, QPointF(x_root, y_root));
    } else if (isInteractiveMoveResize() && direction == NET::MoveResizeCancel) {
        finishInteractiveMoveResize(true);
        setInteractiveMoveResizePointerButtonDown(false);
        updateCursor();
    } else if (direction >= NET::TopLeft && direction <= NET::Left) {
        static const Gravity convert[] = {
            Gravity::TopLeft,
            Gravity::Top,
            Gravity::TopRight,
            Gravity::Right,
            Gravity::BottomRight,
            Gravity::Bottom,
            Gravity::BottomLeft,
            Gravity::Left};
        if (!isResizable() || isShade()) {
            return;
        }
        if (isInteractiveMoveResize()) {
            finishInteractiveMoveResize(false);
        }
        setInteractiveMoveResizePointerButtonDown(true);
        setInteractiveMoveOffset(QPointF(x_root - x(), y_root - y())); // map from global
        setInvertedInteractiveMoveOffset(rect().bottomRight() - interactiveMoveOffset());
        setUnrestrictedInteractiveMoveResize(false);
        setInteractiveMoveResizeGravity(convert[direction]);
        if (!startInteractiveMoveResize()) {
            setInteractiveMoveResizePointerButtonDown(false);
        }
        updateCursor();
    } else if (direction == NET::KeyboardMove) {
        // ignore mouse coordinates given in the message, mouse position is used by the moving algorithm
        Cursors::self()->mouse()->setPos(frameGeometry().center());
        performMouseCommand(Options::MouseUnrestrictedMove, frameGeometry().center());
    } else if (direction == NET::KeyboardSize) {
        // ignore mouse coordinates given in the message, mouse position is used by the resizing algorithm
        Cursors::self()->mouse()->setPos(frameGeometry().bottomRight());
        performMouseCommand(Options::MouseUnrestrictedResize, frameGeometry().bottomRight());
    }
}

void X11Window::keyPressEvent(uint key_code, xcb_timestamp_t time)
{
    updateUserTime(time);
    Window::keyPressEvent(key_code);
}

// ****************************************
// Unmanaged
// ****************************************

bool Unmanaged::windowEvent(xcb_generic_event_t *e)
{
    NET::Properties dirtyProperties;
    NET::Properties2 dirtyProperties2;
    info->event(e, &dirtyProperties, &dirtyProperties2); // pass through the NET stuff
    if (dirtyProperties2 & NET::WM2Opacity) {
        if (Compositor::compositing()) {
            setOpacity(info->opacityF());
        }
    }
    if (dirtyProperties2 & NET::WM2OpaqueRegion) {
        getWmOpaqueRegion();
    }
    if (dirtyProperties2.testFlag(NET::WM2WindowRole)) {
        Q_EMIT windowRoleChanged();
    }
    if (dirtyProperties2.testFlag(NET::WM2WindowClass)) {
        getResourceClass();
    }
    const uint8_t eventType = e->response_type & ~0x80;
    switch (eventType) {
    case XCB_DESTROY_NOTIFY:
        release(ReleaseReason::Destroyed);
        break;
    case XCB_UNMAP_NOTIFY: {
        workspace()->updateFocusMousePosition(Cursors::self()->mouse()->pos()); // may cause leave event

        // unmap notify might have been emitted due to a destroy notify
        // but unmap notify gets emitted before the destroy notify, nevertheless at this
        // point the window is already destroyed. This means any XCB request with the window
        // will cause an error.
        // To not run into these errors we try to wait for the destroy notify. For this we
        // generate a round trip to the X server and wait a very short time span before
        // handling the release.
        kwinApp()->updateXTime();
        // using 1 msec to not just move it at the end of the event loop but add an very short
        // timespan to cover cases like unmap() followed by destroy(). The only other way to
        // ensure that the window is not destroyed when we do the release handling is to grab
        // the XServer which we do not want to do for an Unmanaged. The timespan of 1 msec is
        // short enough to not cause problems in the close window animations.
        // It's of course still possible that we miss the destroy in which case non-fatal
        // X errors are reported to the event loop and logged by Qt.
        m_scheduledRelease = true;
        QTimer::singleShot(1, this, [this]() {
            release();
        });
        break;
    }
    case XCB_CONFIGURE_NOTIFY:
        configureNotifyEvent(reinterpret_cast<xcb_configure_notify_event_t *>(e));
        break;
    case XCB_PROPERTY_NOTIFY:
        propertyNotifyEvent(reinterpret_cast<xcb_property_notify_event_t *>(e));
        break;
    case XCB_CLIENT_MESSAGE:
        clientMessageEvent(reinterpret_cast<xcb_client_message_event_t *>(e));
        break;
    default: {
        if (eventType == Xcb::Extensions::self()->shapeNotifyEvent()) {
            detectShape(window());
            Q_EMIT geometryShapeChanged(this, frameGeometry());
        }
        if (eventType == Xcb::Extensions::self()->damageNotifyEvent()) {
            damageNotifyEvent();
        }
        break;
    }
    }
    return false; // don't eat events, even our own unmanaged widgets are tracked
}

void Unmanaged::configureNotifyEvent(xcb_configure_notify_event_t *e)
{
    if (effects) {
        static_cast<EffectsHandlerImpl *>(effects)->checkInputWindowStacking(); // keep them on top
    }
    QRectF newgeom(Xcb::fromXNative(e->x), Xcb::fromXNative(e->y), Xcb::fromXNative(e->width), Xcb::fromXNative(e->height));
    if (newgeom != m_frameGeometry) {
        Q_EMIT frameGeometryAboutToChange(this);

        QRectF old = m_frameGeometry;
        m_clientGeometry = newgeom;
        m_frameGeometry = newgeom;
        m_bufferGeometry = newgeom;
        checkOutput();
        Q_EMIT bufferGeometryChanged(this, old);
        Q_EMIT clientGeometryChanged(this, old);
        Q_EMIT frameGeometryChanged(this, old);
        Q_EMIT geometryShapeChanged(this, old);
    }
}

// ****************************************
// Window
// ****************************************

void Window::propertyNotifyEvent(xcb_property_notify_event_t *e)
{
    if (e->window != window()) {
        return; // ignore frame/wrapper
    }
    switch (e->atom) {
    default:
        if (e->atom == atoms->wm_client_leader) {
            getWmClientLeader();
        } else if (e->atom == atoms->kde_net_wm_shadow) {
            updateShadow();
        } else if (e->atom == atoms->kde_skip_close_animation) {
            getSkipCloseAnimation();
        }
        break;
    }
}

void Window::clientMessageEvent(xcb_client_message_event_t *e)
{
    if (e->type == atoms->wl_surface_serial) {
        m_surfaceSerial = (uint64_t(e->data.data32[1]) << 32) | e->data.data32[0];
        if (auto w = waylandServer()) {
            if (KWaylandServer::XwaylandSurfaceV1Interface *xwaylandSurface = w->xwaylandShell()->findSurface(m_surfaceSerial)) {
                setSurface(xwaylandSurface->surface());
            }
        }
    } else if (e->type == atoms->wl_surface_id) {
        m_pendingSurfaceId = e->data.data32[0];
        if (auto w = waylandServer()) {
            if (auto s = KWaylandServer::SurfaceInterface::get(m_pendingSurfaceId, w->xWaylandConnection())) {
                setSurface(s);
            }
        }
    }
}

} // namespace
