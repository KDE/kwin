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
#include "effect/effecthandler.h"
#include "focuschain.h"
#include "group.h"
#include "input.h"
#include "netinfo.h"
#include "rules.h"
#include "screenedge.h"
#include "touch_input.h"
#include "useractions.h"
#include "utils/xcbutils.h"
#include "wayland/surface.h"
#include "wayland/xwaylandshell_v1.h"
#include "wayland_server.h"
#include "workspace.h"
#include "x11window.h"

#if KWIN_BUILD_TABBOX
#include "tabbox/tabbox.h"
#endif

#include <KDecoration3/Decoration>

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
#include <xcb/xcb_icccm.h>

#include "compositor.h"
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
        } else if (X11Window *window = findUnmanaged(eventWindow)) {
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
        if (event->override_redirect && event->event == kwinApp()->x11RootWindow()) {
            X11Window *window = findUnmanaged(event->window);
            if (window == nullptr) {
                window = createUnmanaged(event->window);
            }
            if (window) {
                // if hasScheduledRelease is true, it means a unamp and map sequence has occurred.
                // since release is scheduled after map notify, this old Unmanaged will get released
                // before KWIN has chance to remanage it again. so release it right now.
                if (window->hasScheduledRelease()) {
                    window->releaseWindow();
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
            if (!currentInput.isNull()) {
                // it seems we can "loose" focus reversions when the closing window hold a grab
                // => catch the typical pattern (though we don't want the focus on the root anyway) #348935
                const bool lostFocusPointerToRoot = currentInput->focus == kwinApp()->x11RootWindow() && event->detail == XCB_NOTIFY_DETAIL_INFERIOR;
                if (currentInput->focus == XCB_WINDOW_NONE || currentInput->focus == XCB_INPUT_FOCUS_POINTER_ROOT || lostFocusPointerToRoot) {
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
    if (isUnmanaged()) {
        NET::Properties dirtyProperties;
        NET::Properties2 dirtyProperties2;
        info->event(e, &dirtyProperties, &dirtyProperties2); // pass through the NET stuff
        if (dirtyProperties2 & NET::WM2Opacity) {
            setOpacity(info->opacityF());
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
            destroyWindow();
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
            m_releaseTimer.start(1);
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
                detectShape();
                Q_EMIT shapeChanged();
            }
            break;
        }
        }
        return false; // don't eat events, even our own unmanaged widgets are tracked
    }

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
            workspace()->rearrange();
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
            setOpacity(info->opacityF());
        }
        if (dirtyProperties2.testFlag(NET::WM2WindowRole)) {
            Q_EMIT windowRoleChanged();
        }
        if (dirtyProperties2.testFlag(NET::WM2WindowClass)) {
            getResourceClass();
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
        break;
    }
    case XCB_KEY_RELEASE:
        // don't update user time on releases
        // e.g. if the user presses Alt+F2, the Alt release
        // would appear as user input to the currently active window
        break;
    case XCB_BUTTON_RELEASE: {
        // don't update user time on releases
        // e.g. if the user presses Alt+F2, the Alt release
        // would appear as user input to the currently active window
        break;
    }
    case XCB_MOTION_NOTIFY: {
        const auto *event = reinterpret_cast<xcb_motion_notify_event_t *>(e);

        int x = Xcb::fromXNative(event->event_x);
        int y = Xcb::fromXNative(event->event_y);
        int root_x = Xcb::fromXNative(event->root_x);
        int root_y = Xcb::fromXNative(event->root_y);

        workspace()->updateFocusMousePosition(QPointF(root_x, root_y));
        break;
    }
    case XCB_ENTER_NOTIFY: {
        auto *event = reinterpret_cast<xcb_enter_notify_event_t *>(e);
        // MotionNotify is guaranteed to be generated only if the mouse
        // move start and ends in the window; for cases when it only
        // starts or only ends there, Enter/LeaveNotify are generated.
        // Fake a MotionEvent in such cases to make handle of mouse
        // events simpler (Qt does that too).
        int x = Xcb::fromXNative(event->event_x);
        int y = Xcb::fromXNative(event->event_y);
        int root_x = Xcb::fromXNative(event->root_x);
        int root_y = Xcb::fromXNative(event->root_y);

        workspace()->updateFocusMousePosition(QPointF(root_x, root_y));
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
    default:
        if (eventType == Xcb::Extensions::self()->shapeNotifyEvent() && reinterpret_cast<xcb_shape_notify_event_t *>(e)->affected_window == window()) {
            detectShape(); // workaround for #19644
            updateShape();
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
        setMinimized(false);
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
    if (e->type == atoms->wl_surface_serial) {
        m_surfaceSerial = (uint64_t(e->data.data32[1]) << 32) | e->data.data32[0];
        if (auto w = waylandServer()) {
            if (XwaylandSurfaceV1Interface *xwaylandSurface = w->xwaylandShell()->findSurface(m_surfaceSerial)) {
                setSurface(xwaylandSurface->surface());
            }
        }
    }

    if (e->window != window()) {
        return; // ignore frame/wrapper
    }
    // WM_STATE
    if (e->type == atoms->wm_change_state) {
        if (e->data.data32[0] == XCB_ICCCM_WM_STATE_ICONIC) {
            setMinimized(true);
        }
        return;
    }
}

void X11Window::configureNotifyEvent(xcb_configure_notify_event_t *e)
{
    if (effects) {
        effects->checkInputWindowStacking(); // keep them on top
    }
    QRectF newgeom(Xcb::fromXNative(e->x), Xcb::fromXNative(e->y), Xcb::fromXNative(e->width), Xcb::fromXNative(e->height));
    if (newgeom != m_frameGeometry) {
        Q_EMIT frameGeometryAboutToChange();

        QRectF old = m_frameGeometry;
        m_clientGeometry = newgeom;
        m_frameGeometry = newgeom;
        m_bufferGeometry = newgeom;
        checkOutput();
        Q_EMIT bufferGeometryChanged(old);
        Q_EMIT clientGeometryChanged(old);
        Q_EMIT frameGeometryChanged(old);
        Q_EMIT shapeChanged();
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
        restackWindow(e->sibling, e->stack_mode, NET::FromApplication, userTime());
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
        } else if (e->atom == atoms->kde_color_sheme) {
            updateColorScheme();
        } else if (e->atom == atoms->kde_screen_edge_show) {
            updateShowOnScreenEdge();
        } else if (e->atom == atoms->kde_net_wm_appmenu_service_name) {
            checkApplicationMenuServiceName();
        } else if (e->atom == atoms->kde_net_wm_appmenu_object_path) {
            checkApplicationMenuObjectPath();
        } else if (e->atom == atoms->wm_client_leader) {
            getWmClientLeader();
        } else if (e->atom == atoms->kde_net_wm_shadow) {
            updateShadow();
        } else if (e->atom == atoms->kde_skip_close_animation) {
            getSkipCloseAnimation();
        }
        break;
    }
}

void X11Window::focusInEvent(xcb_focus_in_event_t *e)
{
    if (e->event != window()) {
        return; // only window gets focus
    }
    if (e->mode == XCB_NOTIFY_MODE_GRAB || e->mode == XCB_NOTIFY_MODE_UNGRAB) {
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
    if (e->mode == XCB_NOTIFY_MODE_GRAB || e->mode == XCB_NOTIFY_MODE_UNGRAB) {
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
void X11Window::NETMoveResize(qreal x_root, qreal y_root, NET::Direction direction, xcb_button_t button)
{
    if (isInteractiveMoveResize() && direction == NET::MoveResizeCancel) {
        finishInteractiveMoveResize(true);
        setInteractiveMoveResizePointerButtonDown(false);
        updateCursor();
    } else if (direction == NET::Move || (direction >= NET::TopLeft && direction <= NET::Left)) {
        if (!button) {
            if (!input()->qtButtonStates() && !input()->touch()->touchPointCount()) {
                return;
            }
        } else {
            if (!(input()->qtButtonStates() & x11ToQtMouseButton(button)) && !input()->touch()->touchPointCount()) {
                return;
            }
        }

        if (direction == NET::Move) {
            // move cursor to the provided position to prevent the window jumping there on first movement
            // the expectation is that the cursor is already at the provided position,
            // thus it's more a safety measurement
            Cursors::self()->mouse()->setPos(QPointF(x_root, y_root));
            performMousePressCommand(Options::MouseMove, QPointF(x_root, y_root));
        } else {
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
            setInteractiveMoveResizeAnchor(QPointF(x_root, y_root));
            setInteractiveMoveResizeModifiers(Qt::KeyboardModifiers());
            setInteractiveMoveOffset(QPointF(qreal(x_root - x()) / width(), qreal(y_root - y()) / height())); // map from global
            setUnrestrictedInteractiveMoveResize(false);
            setInteractiveMoveResizeGravity(convert[direction]);
            if (!startInteractiveMoveResize()) {
                setInteractiveMoveResizePointerButtonDown(false);
            }
            updateCursor();
        }
    } else if (direction == NET::KeyboardMove) {
        // ignore mouse coordinates given in the message, mouse position is used by the moving algorithm
        Cursors::self()->mouse()->setPos(frameGeometry().center());
        performMousePressCommand(Options::MouseUnrestrictedMove, frameGeometry().center());
    } else if (direction == NET::KeyboardSize) {
        // ignore mouse coordinates given in the message, mouse position is used by the resizing algorithm
        Cursors::self()->mouse()->setPos(frameGeometry().bottomRight());
        performMousePressCommand(Options::MouseUnrestrictedResize, frameGeometry().bottomRight());
    }
}

void X11Window::keyPressEvent(uint key_code, xcb_timestamp_t time)
{
    updateUserTime(time);
    Window::keyPressEvent(key_code);
}

} // namespace
