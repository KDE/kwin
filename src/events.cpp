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
#include "group.h"
#include "input.h"
#include "netinfo.h"
#include "pointer_input.h"
#include "touch_input.h"
#include "useractions.h"
#include "utils/xcbutils.h"
#include "wayland/xwaylandshell_v1.h"
#include "wayland_server.h"
#include "workspace.h"
#include "x11window.h"

#include <xcb/xcb_icccm.h>

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

static Qt::MouseButton x11ToQtMouseButton(int button)
{
    if (button == XCB_BUTTON_INDEX_1) {
        return Qt::LeftButton;
    }
    if (button == XCB_BUTTON_INDEX_2) {
        return Qt::MiddleButton;
    }
    if (button == XCB_BUTTON_INDEX_3) {
        return Qt::RightButton;
    }
    if (button == XCB_BUTTON_INDEX_4) {
        return Qt::XButton1;
    }
    if (button == XCB_BUTTON_INDEX_5) {
        return Qt::XButton2;
    }
    return Qt::NoButton;
}

/**
 * Handles workspace specific XCB event
 */
bool Workspace::workspaceEvent(xcb_generic_event_t *e)
{
    const uint8_t eventType = e->response_type & ~0x80;

    const xcb_window_t eventWindow = findEventWindow(e);
    if (eventWindow != XCB_WINDOW_NONE) {
        if (X11Window *window = findClient(eventWindow)) {
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
        if (event->parent == kwinApp()->x11RootWindow() && !event->override_redirect) {
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
    case XCB_MAP_REQUEST: {
        kwinApp()->updateXTime();

        const auto *event = reinterpret_cast<xcb_map_request_event_t *>(e);
        if (!createX11Window(event->window, false)) {
            xcb_map_window(kwinApp()->x11Connection(), event->window);
            const uint32_t values[] = {XCB_STACK_MODE_ABOVE};
            xcb_configure_window(kwinApp()->x11Connection(), event->window, XCB_CONFIG_WINDOW_STACK_MODE, values);
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
            if (updateXStackingOrder()) {
                updateStackingOrder();
            }
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
                    // kWarning( 1212 ) << "X focus set to None/PointerRoot, resetting focus" ;
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
                shapeNotifyEvent(reinterpret_cast<xcb_shape_notify_event_t *>(e));
            }
            break;
        }
        }
        return false; // don't eat events, even our own unmanaged widgets are tracked
    }

    NET::Properties dirtyProperties;
    NET::Properties2 dirtyProperties2;
    info->event(e, &dirtyProperties, &dirtyProperties2); // pass through the NET stuff

    if ((dirtyProperties & NET::WMName) != 0) {
        fetchName();
    }
    if ((dirtyProperties & NET::WMIconName) != 0) {
        fetchIconicName();
    }
    if ((dirtyProperties & NET::WMIcon) != 0) {
        getIcons();
    }
    if ((dirtyProperties2 & NET::WM2UserTime) != 0) {
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

    const uint8_t eventType = e->response_type & ~0x80;
    switch (eventType) {
    case XCB_UNMAP_NOTIFY:
        unmapNotifyEvent(reinterpret_cast<xcb_unmap_notify_event_t *>(e));
        break;
    case XCB_DESTROY_NOTIFY:
        destroyNotifyEvent(reinterpret_cast<xcb_destroy_notify_event_t *>(e));
        break;
    case XCB_MAP_REQUEST:
        mapRequestEvent(reinterpret_cast<xcb_map_request_event_t *>(e));
        break;
    case XCB_CONFIGURE_REQUEST:
        configureRequestEvent(reinterpret_cast<xcb_configure_request_event_t *>(e));
        break;
    case XCB_PROPERTY_NOTIFY:
        propertyNotifyEvent(reinterpret_cast<xcb_property_notify_event_t *>(e));
        break;
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
        if (eventType == Xcb::Extensions::self()->shapeNotifyEvent()) {
            shapeNotifyEvent(reinterpret_cast<xcb_shape_notify_event_t *>(e));
        }
        break;
    }
    return true; // eat all events
}

/**
 * Handles map requests of the client window
 */
void X11Window::mapRequestEvent(xcb_map_request_event_t *e)
{
    // also copied in clientMessage()
    if (isMinimized()) {
        setMinimized(false);
    }
    if (!isOnCurrentDesktop()) {
        if (allowWindowActivation()) {
            workspace()->activateWindow(this);
        } else {
            demandAttention();
        }
    }
}

/**
 * Handles unmap notify events of the client window
 */
void X11Window::unmapNotifyEvent(xcb_unmap_notify_event_t *e)
{
    if (m_inflightUnmaps == 0) {
        releaseWindow();
    } else {
        m_inflightUnmaps--;
    }
}

void X11Window::destroyNotifyEvent(xcb_destroy_notify_event_t *e)
{
    destroyWindow();
}

/**
 * Handles client messages for the client window
 */
void X11Window::clientMessageEvent(xcb_client_message_event_t *e)
{
    if (e->type == atoms->wl_surface_serial) {
        m_surfaceSerial = (uint64_t(e->data.data32[1]) << 32) | e->data.data32[0];
        if (XwaylandSurfaceV1Interface *xwaylandSurface = waylandServer()->xwaylandShell()->findSurface(m_surfaceSerial)) {
            associate(xwaylandSurface);
        }
    }

    if (e->type == atoms->wm_change_state) {
        if (e->data.data32[0] == XCB_ICCCM_WM_STATE_ICONIC) {
            setMinimized(true);
        }
        return;
    }
}

void X11Window::configureNotifyEvent(xcb_configure_notify_event_t *e)
{
    QRectF newgeom(Xcb::fromXNative(e->x), Xcb::fromXNative(e->y), Xcb::fromXNative(e->width), Xcb::fromXNative(e->height));
    if (newgeom != m_frameGeometry) {
        Q_EMIT frameGeometryAboutToChange();

        QRectF old = m_frameGeometry;
        m_clientGeometry = newgeom;
        m_frameGeometry = newgeom;
        m_bufferGeometry = newgeom;
        checkOutput();
        updateShapeRegion();
        Q_EMIT bufferGeometryChanged(old);
        Q_EMIT clientGeometryChanged(old);
        Q_EMIT frameGeometryChanged(old);
    }
}

/**
 * Handles configure  requests of the client window
 */
void X11Window::configureRequestEvent(xcb_configure_request_event_t *e)
{
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
        } else if (e->atom == atoms->xwayland_xrandr_emulation) {
            configure(Xcb::toXNative(m_bufferGeometry));
        }
        break;
    }
}

void X11Window::focusInEvent(xcb_focus_in_event_t *e)
{
    if (e->mode == XCB_NOTIFY_MODE_GRAB || e->mode == XCB_NOTIFY_MODE_UNGRAB) {
        return; // we don't care
    }
    if (e->detail == XCB_NOTIFY_DETAIL_POINTER) {
        return; // we don't care
    }
    if (!isShown() || !isOnCurrentDesktop()) { // we unmapped it, but it got focus meanwhile ->
        return; // activateNextWindow() already transferred focus elsewhere
    }
    workspace()->forEachClient([](X11Window *window) {
        window->cancelFocusOutTimer();
    });
    // check if this window is in should_get_focus list or if activation is allowed
    bool activate = allowWindowActivation(-1U, true);
    workspace()->gotFocusIn(this); // remove from should_get_focus list
    if (activate) {
        workspace()->setActiveWindow(this);
    } else {
        if (workspace()->restoreFocus()) {
            demandAttention();
        } else {
            qCWarning(KWIN_CORE, "Failed to restore focus. Activating 0x%x", window());
            workspace()->setActiveWindow(this);
        }
    }
}

void X11Window::focusOutEvent(xcb_focus_out_event_t *e)
{
    if (e->mode == XCB_NOTIFY_MODE_GRAB || e->mode == XCB_NOTIFY_MODE_UNGRAB) {
        return; // we don't care
    }
    if (e->detail != XCB_NOTIFY_DETAIL_NONLINEAR
        && e->detail != XCB_NOTIFY_DETAIL_NONLINEAR_VIRTUAL) {
        // SELI check all this
        return; // hack for motif apps like netscape
    }

    // When a window loses focus, FocusOut events are usually immediately
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
            if (workspace()->activeWindow() == this) {
                workspace()->setActiveWindow(nullptr);
            }
        });
    }
    m_focusOutTimer->start();
}

void X11Window::shapeNotifyEvent(xcb_shape_notify_event_t *e)
{
    if (e->affected_window != window()) {
        return;
    }

    switch (e->shape_kind) {
    case XCB_SHAPE_SK_BOUNDING:
    case XCB_SHAPE_SK_CLIP:
        updateShapeRegion();
        break;
    case XCB_SHAPE_SK_INPUT:
        break;
    }
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
            input()->pointer()->warp(QPointF(x_root, y_root));
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
            if (!isResizable()) {
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
        input()->pointer()->warp(frameGeometry().center());
        performMousePressCommand(Options::MouseUnrestrictedMove, frameGeometry().center());
    } else if (direction == NET::KeyboardSize) {
        // ignore mouse coordinates given in the message, mouse position is used by the resizing algorithm
        input()->pointer()->warp(frameGeometry().bottomRight());
        performMousePressCommand(Options::MouseUnrestrictedResize, frameGeometry().bottomRight());
    }
}

} // namespace
