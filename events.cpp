/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 1999, 2000 Matthias Ettrich <ettrich@kde.org>
Copyright (C) 2003 Lubos Lunak <l.lunak@kde.org>

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

/*

 This file contains things relevant to handling incoming events.

*/

#include "client.h"
#include "cursor.h"
#include "focuschain.h"
#include "netinfo.h"
#include "workspace.h"
#include "atoms.h"
#ifdef KWIN_BUILD_TABBOX
#include "tabbox.h"
#endif
#include "group.h"
#include "rules.h"
#include "unmanaged.h"
#include "useractions.h"
#include "effects.h"
#include "screens.h"
#include "xcbutils.h"

#include <KDecoration2/Decoration>

#include <QApplication>
#include <QDebug>
#include <QHoverEvent>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QStyleHints>
#include <QWheelEvent>

#include <kkeyserver.h>

#include <xcb/sync.h>
#ifdef XCB_ICCCM_FOUND
#include <xcb/xcb_icccm.h>
#endif

#include "composite.h"
#include "x11eventfilter.h"

#include "wayland_server.h"
#include <KWayland/Server/surface_interface.h>

#ifndef XCB_GE_GENERIC
#define XCB_GE_GENERIC 35
typedef struct xcb_ge_generic_event_t {
    uint8_t  response_type; /**<  */
    uint8_t  extension; /**<  */
    uint16_t sequence; /**<  */
    uint32_t length; /**<  */
    uint16_t event_type; /**<  */
    uint8_t  pad0[22]; /**<  */
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
    switch(eventType) {
    case XCB_KEY_PRESS:
    case XCB_KEY_RELEASE:
        return reinterpret_cast<xcb_key_press_event_t*>(event)->event;
    case XCB_BUTTON_PRESS:
    case XCB_BUTTON_RELEASE:
        return reinterpret_cast<xcb_button_press_event_t*>(event)->event;
    case XCB_MOTION_NOTIFY:
        return reinterpret_cast<xcb_motion_notify_event_t*>(event)->event;
    case XCB_ENTER_NOTIFY:
    case XCB_LEAVE_NOTIFY:
        return reinterpret_cast<xcb_enter_notify_event_t*>(event)->event;
    case XCB_FOCUS_IN:
    case XCB_FOCUS_OUT:
        return reinterpret_cast<xcb_focus_in_event_t*>(event)->event;
    case XCB_EXPOSE:
        return reinterpret_cast<xcb_expose_event_t*>(event)->window;
    case XCB_GRAPHICS_EXPOSURE:
        return reinterpret_cast<xcb_graphics_exposure_event_t*>(event)->drawable;
    case XCB_NO_EXPOSURE:
        return reinterpret_cast<xcb_no_exposure_event_t*>(event)->drawable;
    case XCB_VISIBILITY_NOTIFY:
        return reinterpret_cast<xcb_visibility_notify_event_t*>(event)->window;
    case XCB_CREATE_NOTIFY:
        return reinterpret_cast<xcb_create_notify_event_t*>(event)->window;
    case XCB_DESTROY_NOTIFY:
        return reinterpret_cast<xcb_destroy_notify_event_t*>(event)->window;
    case XCB_UNMAP_NOTIFY:
        return reinterpret_cast<xcb_unmap_notify_event_t*>(event)->window;
    case XCB_MAP_NOTIFY:
        return reinterpret_cast<xcb_map_notify_event_t*>(event)->window;
    case XCB_MAP_REQUEST:
        return reinterpret_cast<xcb_map_request_event_t*>(event)->window;
    case XCB_REPARENT_NOTIFY:
        return reinterpret_cast<xcb_reparent_notify_event_t*>(event)->window;
    case XCB_CONFIGURE_NOTIFY:
        return reinterpret_cast<xcb_configure_notify_event_t*>(event)->window;
    case XCB_CONFIGURE_REQUEST:
        return reinterpret_cast<xcb_configure_request_event_t*>(event)->window;
    case XCB_GRAVITY_NOTIFY:
        return reinterpret_cast<xcb_gravity_notify_event_t*>(event)->window;
    case XCB_RESIZE_REQUEST:
        return reinterpret_cast<xcb_resize_request_event_t*>(event)->window;
    case XCB_CIRCULATE_NOTIFY:
    case XCB_CIRCULATE_REQUEST:
        return reinterpret_cast<xcb_circulate_notify_event_t*>(event)->window;
    case XCB_PROPERTY_NOTIFY:
        return reinterpret_cast<xcb_property_notify_event_t*>(event)->window;
    case XCB_COLORMAP_NOTIFY:
        return reinterpret_cast<xcb_colormap_notify_event_t*>(event)->window;
    case XCB_CLIENT_MESSAGE:
        return reinterpret_cast<xcb_client_message_event_t*>(event)->window;
    default:
        // extension handling
        if (eventType == Xcb::Extensions::self()->shapeNotifyEvent()) {
            return reinterpret_cast<xcb_shape_notify_event_t*>(event)->affected_window;
        }
        if (eventType == Xcb::Extensions::self()->damageNotifyEvent()) {
            return reinterpret_cast<xcb_damage_notify_event_t*>(event)->drawable;
        }
        return XCB_WINDOW_NONE;
    }
}

QVector<QByteArray> s_xcbEerrors({
    QByteArrayLiteral("Success"),
    QByteArrayLiteral("BadRequest"),
    QByteArrayLiteral("BadValue"),
    QByteArrayLiteral("BadWindow"),
    QByteArrayLiteral("BadPixmap"),
    QByteArrayLiteral("BadAtom"),
    QByteArrayLiteral("BadCursor"),
    QByteArrayLiteral("BadFont"),
    QByteArrayLiteral("BadMatch"),
    QByteArrayLiteral("BadDrawable"),
    QByteArrayLiteral("BadAccess"),
    QByteArrayLiteral("BadAlloc"),
    QByteArrayLiteral("BadColor"),
    QByteArrayLiteral("BadGC"),
    QByteArrayLiteral("BadIDChoice"),
    QByteArrayLiteral("BadName"),
    QByteArrayLiteral("BadLength"),
    QByteArrayLiteral("BadImplementation"),
    QByteArrayLiteral("Unknown")});


void Workspace::registerEventFilter(X11EventFilter *filter)
{
    if (filter->isGenericEvent())
        m_genericEventFilters.append(filter);
    else
        m_eventFilters.append(filter);
}

void Workspace::unregisterEventFilter(X11EventFilter *filter)
{
    if (filter->isGenericEvent())
        m_genericEventFilters.removeOne(filter);
    else
        m_eventFilters.removeOne(filter);
}


/**
 * Handles workspace specific XCB event
 **/
bool Workspace::workspaceEvent(xcb_generic_event_t *e)
{
    const uint8_t eventType = e->response_type & ~0x80;
    if (!eventType) {
        // let's check whether it's an error from one of the extensions KWin uses
        xcb_generic_error_t *error = reinterpret_cast<xcb_generic_error_t*>(e);
        const QVector<Xcb::ExtensionData> extensions = Xcb::Extensions::self()->extensions();
        for (const auto &extension : extensions) {
            if (error->major_code == extension.majorOpcode) {
                QByteArray errorName;
                if (error->error_code < s_xcbEerrors.size()) {
                    errorName = s_xcbEerrors.at(error->error_code);
                } else if (error->error_code >= extension.errorBase) {
                    const int index = error->error_code - extension.errorBase;
                    if (index >= 0 && index < extension.errorCodes.size()) {
                        errorName = extension.errorCodes.at(index);
                    }
                }
                if (errorName.isEmpty()) {
                    errorName = QByteArrayLiteral("Unknown");
                }
                qCWarning(KWIN_CORE, "XCB error: %d (%s), sequence: %d, resource id: %d, major code: %d (%s), minor code: %d (%s)",
                         int(error->error_code), errorName.constData(),
                         int(error->sequence), int(error->resource_id),
                         int(error->major_code), extension.name.constData(),
                         int(error->minor_code),
                         extension.opCodes.size() > error->minor_code ? extension.opCodes.at(error->minor_code).constData() : "Unknown");
                return true;
            }
        }
        return false;
    }

    if (eventType == XCB_GE_GENERIC) {
        xcb_ge_generic_event_t *ge = reinterpret_cast<xcb_ge_generic_event_t *>(e);

        foreach (X11EventFilter *filter, m_genericEventFilters) {
            if (filter->extension() == ge->extension && filter->genericEventTypes().contains(ge->event_type) && filter->event(e)) {
                return true;
            }
        }
    } else {
        foreach (X11EventFilter *filter, m_eventFilters) {
            if (filter->eventTypes().contains(eventType) && filter->event(e)) {
                return true;
            }
        }
    }

    if (effects && static_cast< EffectsHandlerImpl* >(effects)->hasKeyboardGrab()
            && (eventType == XCB_KEY_PRESS || eventType == XCB_KEY_RELEASE))
        return false; // let Qt process it, it'll be intercepted again in eventFilter()

    // events that should be handled before Clients can get them
    switch (eventType) {
    case XCB_CONFIGURE_NOTIFY:
        if (reinterpret_cast<xcb_configure_notify_event_t*>(e)->event == rootWindow())
            markXStackingOrderAsDirty();
        break;
    };

    const xcb_window_t eventWindow = findEventWindow(e);
    if (eventWindow != XCB_WINDOW_NONE) {
        if (Client* c = findClient(Predicate::WindowMatch, eventWindow)) {
            if (c->windowEvent(e))
                return true;
        } else if (Client* c = findClient(Predicate::WrapperIdMatch, eventWindow)) {
            if (c->windowEvent(e))
                return true;
        } else if (Client* c = findClient(Predicate::FrameIdMatch, eventWindow)) {
            if (c->windowEvent(e))
                return true;
        } else if (Client *c = findClient(Predicate::InputIdMatch, eventWindow)) {
            if (c->windowEvent(e))
                return true;
        } else if (Unmanaged* c = findUnmanaged(eventWindow)) {
            if (c->windowEvent(e))
                return true;
        }
    }

    switch (eventType) {
    case XCB_CREATE_NOTIFY: {
        const auto *event = reinterpret_cast<xcb_create_notify_event_t*>(e);
        if (event->parent == rootWindow() &&
                !QWidget::find(event->window) &&
                !event->override_redirect) {
            // see comments for allowClientActivation()
            updateXTime();
            const xcb_timestamp_t t = xTime();
            xcb_change_property(connection(), XCB_PROP_MODE_REPLACE, event->window, atoms->kde_net_wm_user_creation_time, XCB_ATOM_CARDINAL, 32, 1, &t);
        }
        break;
    }
    case XCB_UNMAP_NOTIFY: {
        const auto *event = reinterpret_cast<xcb_unmap_notify_event_t*>(e);
        return (event->event != event->window);   // hide wm typical event from Qt
    }
    case XCB_REPARENT_NOTIFY: {
        //do not confuse Qt with these events. After all, _we_ are the
        //window manager who does the reparenting.
        return true;
    }
    case XCB_MAP_REQUEST: {
        updateXTime();

        const auto *event = reinterpret_cast<xcb_map_request_event_t*>(e);
        if (Client* c = findClient(Predicate::WindowMatch, event->window)) {
            // e->xmaprequest.window is different from e->xany.window
            // TODO this shouldn't be necessary now
            c->windowEvent(e);
            FocusChain::self()->update(c, FocusChain::Update);
        } else if ( true /*|| e->xmaprequest.parent != root */ ) {
            // NOTICE don't check for the parent being the root window, this breaks when some app unmaps
            // a window, changes something and immediately maps it back, without giving KWin
            // a chance to reparent it back to root
            // since KWin can get MapRequest only for root window children and
            // children of WindowWrapper (=clients), the check is AFAIK useless anyway
            // NOTICE: The save-set support in Client::mapRequestEvent() actually requires that
            // this code doesn't check the parent to be root.
            if (!createClient(event->window, false)) {
                xcb_map_window(connection(), event->window);
                const uint32_t values[] = { XCB_STACK_MODE_ABOVE };
                xcb_configure_window(connection(), event->window, XCB_CONFIG_WINDOW_STACK_MODE, values);
            }
        }
        return true;
    }
    case XCB_MAP_NOTIFY: {
        const auto *event = reinterpret_cast<xcb_map_notify_event_t*>(e);
        if (event->override_redirect) {
            Unmanaged* c = findUnmanaged(event->window);
            if (c == NULL)
                c = createUnmanaged(event->window);
            if (c)
                return c->windowEvent(e);
        }
        return (event->event != event->window);   // hide wm typical event from Qt
    }

    case XCB_CONFIGURE_REQUEST: {
        const auto *event = reinterpret_cast<xcb_configure_request_event_t*>(e);
        if (event->parent == rootWindow()) {
            uint32_t values[5] = { 0, 0, 0, 0, 0};
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
            xcb_configure_window(connection(), event->window, value_mask, values);
            return true;
        }
        break;
    }
    case XCB_FOCUS_IN: {
        const auto *event = reinterpret_cast<xcb_focus_in_event_t*>(e);
        if (event->event == rootWindow()
                && (event->detail == XCB_NOTIFY_DETAIL_NONE || event->detail == XCB_NOTIFY_DETAIL_POINTER_ROOT || event->detail == XCB_NOTIFY_DETAIL_INFERIOR)) {
            Xcb::CurrentInput currentInput;
            updateXTime(); // focusToNull() uses xTime(), which is old now (FocusIn has no timestamp)
            // it seems we can "loose" focus reversions when the closing client hold a grab
            // => catch the typical pattern (though we don't want the focus on the root anyway) #348935
            const bool lostFocusPointerToRoot = currentInput->focus == rootWindow() && event->detail == XCB_NOTIFY_DETAIL_INFERIOR;
            if (!currentInput.isNull() && (currentInput->focus == XCB_WINDOW_NONE || currentInput->focus == XCB_INPUT_FOCUS_POINTER_ROOT || lostFocusPointerToRoot)) {
                //kWarning( 1212 ) << "X focus set to None/PointerRoot, reseting focus" ;
                AbstractClient *c = mostRecentlyActivatedClient();
                if (c != NULL)
                    requestFocus(c, true);
                else if (activateNextClient(NULL))
                    ; // ok, activated
                else
                    focusToNull();
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

// Used only to filter events that need to be processed by Qt first
// (e.g. keyboard input to be composed), otherwise events are
// handle by the XEvent filter above
bool Workspace::workspaceEvent(QEvent* e)
{
    if ((e->type() == QEvent::KeyPress || e->type() == QEvent::KeyRelease || e->type() == QEvent::ShortcutOverride)
            && effects && static_cast< EffectsHandlerImpl* >(effects)->hasKeyboardGrab()) {
        static_cast< EffectsHandlerImpl* >(effects)->grabbedKeyboardEvent(static_cast< QKeyEvent* >(e));
        return true;
    }
    return false;
}

// ****************************************
// Client
// ****************************************

/**
 * General handler for XEvents concerning the client window
 **/
bool Client::windowEvent(xcb_generic_event_t *e)
{
    if (findEventWindow(e) == window()) { // avoid doing stuff on frame or wrapper
        NET::Properties dirtyProperties;
        NET::Properties2 dirtyProperties2;
        double old_opacity = opacity();
        info->event(e, &dirtyProperties, &dirtyProperties2);   // pass through the NET stuff

        if ((dirtyProperties & NET::WMName) != 0)
            fetchName();
        if ((dirtyProperties & NET::WMIconName) != 0)
            fetchIconicName();
        if ((dirtyProperties & NET::WMStrut) != 0
                || (dirtyProperties2 & NET::WM2ExtendedStrut) != 0) {
            workspace()->updateClientArea();
        }
        if ((dirtyProperties & NET::WMIcon) != 0)
            getIcons();
        // Note there's a difference between userTime() and info->userTime()
        // info->userTime() is the value of the property, userTime() also includes
        // updates of the time done by KWin (ButtonPress on windowrapper etc.).
        if ((dirtyProperties2 & NET::WM2UserTime) != 0) {
            workspace()->setWasUserInteraction();
            updateUserTime(info->userTime());
        }
        if ((dirtyProperties2 & NET::WM2StartupId) != 0)
            startupIdChanged();
        if (dirtyProperties2 & NET::WM2Opacity) {
            if (compositing()) {
                addRepaintFull();
                emit opacityChanged(this, old_opacity);
            } else {
                // forward to the frame if there's possibly another compositing manager running
                NETWinInfo i(connection(), frameId(), rootWindow(), 0, 0);
                i.setOpacity(info->opacity());
            }
        }
        if (dirtyProperties2 & NET::WM2FrameOverlap) {
            // ### Inform the decoration
        }
        if (dirtyProperties2.testFlag(NET::WM2WindowRole)) {
            emit windowRoleChanged();
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
            setDesktopFileName(QByteArray(info->desktopFileName()));
        }
    }

    const uint8_t eventType = e->response_type & ~0x80;
    switch(eventType) {
    case XCB_UNMAP_NOTIFY:
        unmapNotifyEvent(reinterpret_cast<xcb_unmap_notify_event_t*>(e));
        break;
    case XCB_DESTROY_NOTIFY:
        destroyNotifyEvent(reinterpret_cast<xcb_destroy_notify_event_t*>(e));
        break;
    case XCB_MAP_REQUEST:
        // this one may pass the event to workspace
        return mapRequestEvent(reinterpret_cast<xcb_map_request_event_t*>(e));
    case XCB_CONFIGURE_REQUEST:
        configureRequestEvent(reinterpret_cast<xcb_configure_request_event_t*>(e));
        break;
    case XCB_PROPERTY_NOTIFY:
        propertyNotifyEvent(reinterpret_cast<xcb_property_notify_event_t*>(e));
        break;
    case XCB_KEY_PRESS:
        updateUserTime(reinterpret_cast<xcb_key_press_event_t*>(e)->time);
        break;
    case XCB_BUTTON_PRESS: {
        const auto *event = reinterpret_cast<xcb_button_press_event_t*>(e);
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
        const auto *event = reinterpret_cast<xcb_button_release_event_t*>(e);
        // don't update user time on releases
        // e.g. if the user presses Alt+F2, the Alt release
        // would appear as user input to the currently active window
        buttonReleaseEvent(event->event, event->detail, event->state,
                           event->event_x, event->event_y, event->root_x, event->root_y);
        break;
    }
    case XCB_MOTION_NOTIFY: {
        const auto *event = reinterpret_cast<xcb_motion_notify_event_t*>(e);
        motionNotifyEvent(event->event, event->state,
                          event->event_x, event->event_y, event->root_x, event->root_y);
        workspace()->updateFocusMousePosition(QPoint(event->root_x, event->root_y));
        break;
    }
    case XCB_ENTER_NOTIFY: {
        auto *event = reinterpret_cast<xcb_enter_notify_event_t*>(e);
        enterNotifyEvent(event);
        // MotionNotify is guaranteed to be generated only if the mouse
        // move start and ends in the window; for cases when it only
        // starts or only ends there, Enter/LeaveNotify are generated.
        // Fake a MotionEvent in such cases to make handle of mouse
        // events simpler (Qt does that too).
        motionNotifyEvent(event->event, event->state,
                          event->event_x, event->event_y, event->root_x, event->root_y);
        workspace()->updateFocusMousePosition(QPoint(event->root_x, event->root_y));
        break;
    }
    case XCB_LEAVE_NOTIFY: {
        auto *event = reinterpret_cast<xcb_leave_notify_event_t*>(e);
        motionNotifyEvent(event->event, event->state,
                          event->event_x, event->event_y, event->root_x, event->root_y);
        leaveNotifyEvent(event);
        // not here, it'd break following enter notify handling
        // workspace()->updateFocusMousePosition( QPoint( e->xcrossing.x_root, e->xcrossing.y_root ));
        break;
    }
    case XCB_FOCUS_IN:
        focusInEvent(reinterpret_cast<xcb_focus_in_event_t*>(e));
        break;
    case XCB_FOCUS_OUT:
        focusOutEvent(reinterpret_cast<xcb_focus_out_event_t*>(e));
        break;
    case XCB_REPARENT_NOTIFY:
        break;
    case XCB_CLIENT_MESSAGE:
        clientMessageEvent(reinterpret_cast<xcb_client_message_event_t*>(e));
        break;
    case XCB_EXPOSE: {
        xcb_expose_event_t *event = reinterpret_cast<xcb_expose_event_t*>(e);
        if (event->window == frameId() && !Compositor::self()->isActive()) {
            // TODO: only repaint required areas
            triggerDecorationRepaint();
        }
        break;
    }
    default:
        if (eventType == Xcb::Extensions::self()->shapeNotifyEvent() && reinterpret_cast<xcb_shape_notify_event_t*>(e)->affected_window == window()) {
            detectShape(window());  // workaround for #19644
            updateShape();
        }
        if (eventType == Xcb::Extensions::self()->damageNotifyEvent() && reinterpret_cast<xcb_damage_notify_event_t*>(e)->drawable == frameId())
            damageNotifyEvent();
        break;
    }
    return true; // eat all events
}

/**
 * Handles map requests of the client window
 **/
bool Client::mapRequestEvent(xcb_map_request_event_t *e)
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
        if (e->parent == wrapperId())
            return false;
        return true; // no messing with frame etc.
    }
    // also copied in clientMessage()
    if (isMinimized())
        unminimize();
    if (isShade())
        setShade(ShadeNone);
    if (!isOnCurrentDesktop()) {
        if (workspace()->allowClientActivation(this))
            workspace()->activateClient(this);
        else
            demandAttention();
    }
    return true;
}

/**
 * Handles unmap notify events of the client window
 **/
void Client::unmapNotifyEvent(xcb_unmap_notify_event_t *e)
{
    if (e->window != window())
        return;
    if (e->event != wrapperId()) {
        // most probably event from root window when initially reparenting
        bool ignore = true;
        if (e->event == rootWindow() && (e->response_type & 0x80))
            ignore = false; // XWithdrawWindow()
        if (ignore)
            return;
    }

    // check whether this is result of an XReparentWindow - client then won't be parented by wrapper
    // in this case do not release the client (causes reparent to root, removal from saveSet and what not)
    // but just destroy the client
    Xcb::Tree tree(m_client);
    xcb_window_t daddy = tree.parent();
    if (daddy == m_wrapper) {
        releaseWindow(); // unmapped from a regular client state
    } else {
        destroyClient(); // the client was moved to some other parent
    }
}

void Client::destroyNotifyEvent(xcb_destroy_notify_event_t *e)
{
    if (e->window != window())
        return;
    destroyClient();
}


/**
 * Handles client messages for the client window
 **/
void Client::clientMessageEvent(xcb_client_message_event_t *e)
{
    Toplevel::clientMessageEvent(e);
    if (e->window != window())
        return; // ignore frame/wrapper
    // WM_STATE
    if (e->type == atoms->wm_change_state) {
        if (e->data.data32[0] == XCB_ICCCM_WM_STATE_ICONIC)
            minimize();
        return;
    }
}


/**
 * Handles configure  requests of the client window
 **/
void Client::configureRequestEvent(xcb_configure_request_event_t *e)
{
    if (e->window != window())
        return; // ignore frame/wrapper
    if (isResize() || isMove())
        return; // we have better things to do right now

    if (fullscreen_mode == FullScreenNormal) { // refuse resizing of fullscreen windows
        // but allow resizing fullscreen hacks in order to let them cancel fullscreen mode
        sendSyntheticConfigureNotify();
        return;
    }
    if (isSplash()) {  // no manipulations with splashscreens either
        sendSyntheticConfigureNotify();
        return;
    }

    if (e->value_mask & XCB_CONFIG_WINDOW_BORDER_WIDTH) {
        // first, get rid of a window border
        m_client.setBorderWidth(0);
    }

    if (e->value_mask & (XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y | XCB_CONFIG_WINDOW_HEIGHT | XCB_CONFIG_WINDOW_WIDTH))
        configureRequest(e->value_mask, e->x, e->y, e->width, e->height, 0, false);

    if (e->value_mask & XCB_CONFIG_WINDOW_STACK_MODE)
        restackWindow(e->sibling, e->stack_mode, NET::FromApplication, userTime(), false);

    // Sending a synthetic configure notify always is fine, even in cases where
    // the ICCCM doesn't require this - it can be though of as 'the WM decided to move
    // the window later'. The client should not cause that many configure request,
    // so this should not have any significant impact. With user moving/resizing
    // the it should be optimized though (see also Client::setGeometry()/plainResize()/move()).
    sendSyntheticConfigureNotify();

    // SELI TODO accept configure requests for isDesktop windows (because kdesktop
    // may get XRANDR resize event before kwin), but check it's still at the bottom?
}


/**
 * Handles property changes of the client window
 **/
void Client::propertyNotifyEvent(xcb_property_notify_event_t *e)
{
    Toplevel::propertyNotifyEvent(e);
    if (e->window != window())
        return; // ignore frame/wrapper
    switch(e->atom) {
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
        } else if (e->atom == atoms->net_wm_sync_request_counter)
            getSyncCounter();
        else if (e->atom == atoms->activities)
            checkActivities();
        else if (e->atom == atoms->kde_first_in_window_list)
            updateFirstInTabBox();
        else if (e->atom == atoms->kde_color_sheme)
            updateColorScheme();
        else if (e->atom == atoms->kde_screen_edge_show)
            updateShowOnScreenEdge();
        else if (e->atom == atoms->gtk_frame_extents)
            detectGtkFrameExtents();
        else if (e->atom == atoms->kde_net_wm_appmenu_service_name)
            checkApplicationMenuServiceName();
        else if (e->atom == atoms->kde_net_wm_appmenu_object_path)
            checkApplicationMenuObjectPath();
        break;
    }
}


void Client::enterNotifyEvent(xcb_enter_notify_event_t *e)
{
    if (e->event != frameId())
        return; // care only about entering the whole frame

#define MOUSE_DRIVEN_FOCUS (!options->focusPolicyIsReasonable() || \
                            (options->focusPolicy() == Options::FocusFollowsMouse && options->isNextFocusPrefersMouse()))
    if (e->mode == XCB_NOTIFY_MODE_NORMAL || (e->mode == XCB_NOTIFY_MODE_UNGRAB && MOUSE_DRIVEN_FOCUS)) {

        if (options->isShadeHover()) {
            cancelShadeHoverTimer();
            if (isShade()) {
                shadeHoverTimer = new QTimer(this);
                connect(shadeHoverTimer, SIGNAL(timeout()), this, SLOT(shadeHover()));
                shadeHoverTimer->setSingleShot(true);
                shadeHoverTimer->start(options->shadeHoverInterval());
            }
        }
#undef MOUSE_DRIVEN_FOCUS

        enterEvent(QPoint(e->root_x, e->root_y));
        return;
    }
}

void Client::leaveNotifyEvent(xcb_leave_notify_event_t *e)
{
    if (e->event != frameId())
        return; // care only about leaving the whole frame
    if (e->mode == XCB_NOTIFY_MODE_NORMAL) {
        if (!isMoveResizePointerButtonDown()) {
            setMoveResizePointerMode(PositionCenter);
            updateCursor();
        }
        bool lostMouse = !rect().contains(QPoint(e->event_x, e->event_y));
        // 'lostMouse' wouldn't work with e.g. B2 or Keramik, which have non-rectangular decorations
        // (i.e. the LeaveNotify event comes before leaving the rect and no LeaveNotify event
        // comes after leaving the rect) - so lets check if the pointer is really outside the window

        // TODO this still sucks if a window appears above this one - it should lose the mouse
        // if this window is another client, but not if it's a popup ... maybe after KDE3.1 :(
        // (repeat after me 'AARGHL!')
        if (!lostMouse && e->detail != XCB_NOTIFY_DETAIL_INFERIOR) {
            Xcb::Pointer pointer(frameId());
            if (!pointer || !pointer->same_screen || pointer->child == XCB_WINDOW_NONE) {
                // really lost the mouse
                lostMouse = true;
            }
        }
        if (lostMouse) {
            leaveEvent();
            cancelShadeHoverTimer();
            if (shade_mode == ShadeHover && !isMoveResize() && !isMoveResizePointerButtonDown()) {
                shadeHoverTimer = new QTimer(this);
                connect(shadeHoverTimer, SIGNAL(timeout()), this, SLOT(shadeUnhover()));
                shadeHoverTimer->setSingleShot(true);
                shadeHoverTimer->start(options->shadeHoverInterval());
            }
            if (isDecorated()) {
                // sending a move instead of a leave. With leave we need to send proper coords, with move it's handled internally
                QHoverEvent leaveEvent(QEvent::HoverMove, QPointF(-1, -1), QPointF(-1, -1), Qt::NoModifier);
                QCoreApplication::sendEvent(decoration(), &leaveEvent);
            }
        }
        if (options->focusPolicy() == Options::FocusStrictlyUnderMouse && isActive() && lostMouse) {
            workspace()->requestDelayFocus(0);
        }
        return;
    }
}

#define XCapL KKeyServer::modXLock()
#define XNumL KKeyServer::modXNumLock()
#define XScrL KKeyServer::modXScrollLock()
void Client::grabButton(int modifier)
{
    unsigned int mods[ 8 ] = {
        0, XCapL, XNumL, XNumL | XCapL,
        XScrL, XScrL | XCapL,
        XScrL | XNumL, XScrL | XNumL | XCapL
    };
    for (int i = 0;
            i < 8;
            ++i)
        m_wrapper.grabButton(XCB_GRAB_MODE_SYNC, XCB_GRAB_MODE_ASYNC, modifier | mods[ i ]);
}

void Client::ungrabButton(int modifier)
{
    unsigned int mods[ 8 ] = {
        0, XCapL, XNumL, XNumL | XCapL,
        XScrL, XScrL | XCapL,
        XScrL | XNumL, XScrL | XNumL | XCapL
    };
    for (int i = 0;
            i < 8;
            ++i)
        m_wrapper.ungrabButton(modifier | mods[ i ]);
}
#undef XCapL
#undef XNumL
#undef XScrL

/**
 * Releases the passive grab for some modifier combinations when a
 * window becomes active. This helps broken X programs that
 * missinterpret LeaveNotify events in grab mode to work properly
 * (Motif, AWT, Tk, ...)
 **/
void Client::updateMouseGrab()
{
    if (workspace()->globalShortcutsDisabled()) {
        m_wrapper.ungrabButton();
        // keep grab for the simple click without modifiers if needed (see below)
        bool not_obscured = workspace()->topClientOnDesktop(VirtualDesktopManager::self()->current(), -1, true, false) == this;
        if (!(!options->isClickRaise() || not_obscured))
            grabButton(XCB_NONE);
        return;
    }
    if (isActive() && !TabBox::TabBox::self()->forcedGlobalMouseGrab()) { // see TabBox::establishTabBoxGrab()
        // first grab all modifier combinations
        m_wrapper.grabButton(XCB_GRAB_MODE_SYNC, XCB_GRAB_MODE_ASYNC);
        // remove the grab for no modifiers only if the window
        // is unobscured or if the user doesn't want click raise
        // (it is unobscured if it the topmost in the unconstrained stacking order, i.e. it is
        // the most recently raised window)
        bool not_obscured = workspace()->topClientOnDesktop(VirtualDesktopManager::self()->current(), -1, true, false) == this;
        if (!options->isClickRaise() || not_obscured)
            ungrabButton(XCB_NONE);
        else
            grabButton(XCB_NONE);
        ungrabButton(XCB_MOD_MASK_SHIFT);
        ungrabButton(XCB_MOD_MASK_CONTROL);
        ungrabButton(XCB_MOD_MASK_CONTROL | XCB_MOD_MASK_SHIFT);
    } else {
        m_wrapper.ungrabButton();
        // simply grab all modifier combinations
        m_wrapper.grabButton(XCB_GRAB_MODE_SYNC, XCB_GRAB_MODE_ASYNC);
    }
}

static bool modKeyDown(int state) {
    const uint keyModX = (options->keyCmdAllModKey() == Qt::Key_Meta) ?
                                                    KKeyServer::modXMeta() : KKeyServer::modXAlt();
    return keyModX  && (state & KKeyServer::accelModMaskX()) == keyModX;
}


// return value matters only when filtering events before decoration gets them
bool Client::buttonPressEvent(xcb_window_t w, int button, int state, int x, int y, int x_root, int y_root, xcb_timestamp_t time)
{
    if (isMoveResizePointerButtonDown()) {
        if (w == wrapperId())
            xcb_allow_events(connection(), XCB_ALLOW_SYNC_POINTER, XCB_TIME_CURRENT_TIME);  //xTime());
        return true;
    }

    if (w == wrapperId() || w == frameId() || w == inputId()) {
        // FRAME neco s tohohle by se melo zpracovat, nez to dostane dekorace
        updateUserTime(time);
        const bool bModKeyHeld = modKeyDown(state);

        if (isSplash()
                && button == XCB_BUTTON_INDEX_1 && !bModKeyHeld) {
            // hide splashwindow if the user clicks on it
            hideClient(true);
            if (w == wrapperId())
                xcb_allow_events(connection(), XCB_ALLOW_SYNC_POINTER, XCB_TIME_CURRENT_TIME);  //xTime());
            return true;
        }

        Options::MouseCommand com = Options::MouseNothing;
        bool was_action = false;
        if (bModKeyHeld) {
            was_action = true;
            switch(button) {
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

            if (isSpecialWindow())
                replay = true;

            if (w == wrapperId())  // these can come only from a grab
                xcb_allow_events(connection(), replay ? XCB_ALLOW_REPLAY_POINTER : XCB_ALLOW_SYNC_POINTER, XCB_TIME_CURRENT_TIME);  //xTime());
            return true;
        }
    }

    if (w == wrapperId()) { // these can come only from a grab
        xcb_allow_events(connection(), XCB_ALLOW_REPLAY_POINTER, XCB_TIME_CURRENT_TIME);  //xTime());
        return true;
    }
    if (w == inputId()) {
        x = x_root - geometry().x();
        y = y_root - geometry().y();
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
                              delta,
                              hor ? Qt::Horizontal : Qt::Vertical,
                              x11ToQtMouseButtons(state),
                              modifiers);
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
bool Client::buttonReleaseEvent(xcb_window_t w, int button, int state, int x, int y, int x_root, int y_root)
{
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
        xcb_allow_events(connection(), XCB_ALLOW_SYNC_POINTER, XCB_TIME_CURRENT_TIME);  //xTime());
        return true;
    }
    if (w != frameId() && w != inputId() && w != moveResizeGrabWindow())
        return true;
    if (w == frameId() && workspace()->userActionsMenu() && workspace()->userActionsMenu()->isShown()) {
        const_cast<UserActionsMenu*>(workspace()->userActionsMenu())->grabInput();
    }
    x = this->x(); // translate from grab window to local coords
    y = this->y();

    // Check whether other buttons are still left pressed
    int buttonMask = XCB_BUTTON_MASK_1 | XCB_BUTTON_MASK_2 | XCB_BUTTON_MASK_3;
    if (button == XCB_BUTTON_INDEX_1)
        buttonMask &= ~XCB_BUTTON_MASK_1;
    else if (button == XCB_BUTTON_INDEX_2)
        buttonMask &= ~XCB_BUTTON_MASK_2;
    else if (button == XCB_BUTTON_INDEX_3)
        buttonMask &= ~XCB_BUTTON_MASK_3;

    if ((state & buttonMask) == 0) {
        endMoveResize();
    }
    return true;
}

// return value matters only when filtering events before decoration gets them
bool Client::motionNotifyEvent(xcb_window_t w, int state, int x, int y, int x_root, int y_root)
{
    if (w == frameId() && isDecorated() && !isMinimized()) {
        // TODO Mouse move event dependent on state
        QHoverEvent event(QEvent::HoverMove, QPointF(x, y), QPointF(x, y));
        QCoreApplication::instance()->sendEvent(decoration(), &event);
    }
    if (w != frameId() && w != inputId() && w != moveResizeGrabWindow())
        return true; // care only about the whole frame
    if (!isMoveResizePointerButtonDown()) {
        if (w == inputId()) {
            int x = x_root - geometry().x();// + padding_left;
            int y = y_root - geometry().y();// + padding_top;

            if (isDecorated()) {
                QHoverEvent event(QEvent::HoverMove, QPointF(x, y), QPointF(x, y));
                QCoreApplication::instance()->sendEvent(decoration(), &event);
            }
        }
        Position newmode = modKeyDown(state) ? PositionCenter : mousePosition();
        if (newmode != moveResizePointerMode()) {
            setMoveResizePointerMode(newmode);
            updateCursor();
        }
        return false;
    }
    if (w == moveResizeGrabWindow()) {
        x = this->x(); // translate from grab window to local coords
        y = this->y();
    }

    handleMoveResize(QPoint(x, y), QPoint(x_root, y_root));
    return true;
}

void Client::focusInEvent(xcb_focus_in_event_t *e)
{
    if (e->event != window())
        return; // only window gets focus
    if (e->mode == XCB_NOTIFY_MODE_UNGRAB)
        return; // we don't care
    if (e->detail == XCB_NOTIFY_DETAIL_POINTER)
        return;  // we don't care
    if (!isShown(false) || !isOnCurrentDesktop())    // we unmapped it, but it got focus meanwhile ->
        return;            // activateNextClient() already transferred focus elsewhere
    workspace()->forEachClient([](Client *client) {
        client->cancelFocusOutTimer();
    });
    // check if this client is in should_get_focus list or if activation is allowed
    bool activate =  workspace()->allowClientActivation(this, -1U, true);
    workspace()->gotFocusIn(this);   // remove from should_get_focus list
    if (activate)
        setActive(true);
    else {
        workspace()->restoreFocus();
        demandAttention();
    }
}

void Client::focusOutEvent(xcb_focus_out_event_t *e)
{
    if (e->event != window())
        return; // only window gets focus
    if (e->mode == XCB_NOTIFY_MODE_GRAB)
        return; // we don't care
    if (isShade())
        return; // here neither
    if (e->detail != XCB_NOTIFY_DETAIL_NONLINEAR
            && e->detail != XCB_NOTIFY_DETAIL_NONLINEAR_VIRTUAL)
        // SELI check all this
        return; // hack for motif apps like netscape
    if (QApplication::activePopupWidget())
        return;

    // When a client loses focus, FocusOut events are usually immediatelly
    // followed by FocusIn events for another client that gains the focus
    // (unless the focus goes to another screen, or to the nofocus widget).
    // Without this check, the former focused client would have to be
    // deactivated, and after that, the new one would be activated, with
    // a short time when there would be no active client. This can cause
    // flicker sometimes, e.g. when a fullscreen is shown, and focus is transferred
    // from it to its transient, the fullscreen would be kept in the Active layer
    // at the beginning and at the end, but not in the middle, when the active
    // client would be temporarily none (see Client::belongToLayer() ).
    // Therefore the setActive(false) call is moved to the end of the current
    // event queue. If there is a matching FocusIn event in the current queue
    // this will be processed before the setActive(false) call and the activation
    // of the Client which gained FocusIn will automatically deactivate the
    // previously active client.
    if (!m_focusOutTimer) {
        m_focusOutTimer = new QTimer(this);
        m_focusOutTimer->setSingleShot(true);
        m_focusOutTimer->setInterval(0);
        connect(m_focusOutTimer, &QTimer::timeout, [this]() {
            setActive(false);
        });
    }
    m_focusOutTimer->start();
}

// performs _NET_WM_MOVERESIZE
void Client::NETMoveResize(int x_root, int y_root, NET::Direction direction)
{
    if (direction == NET::Move) {
        // move cursor to the provided position to prevent the window jumping there on first movement
        // the expectation is that the cursor is already at the provided position,
        // thus it's more a safety measurement
        Cursor::setPos(QPoint(x_root, y_root));
        performMouseCommand(Options::MouseMove, QPoint(x_root, y_root));
    } else if (isMoveResize() && direction == NET::MoveResizeCancel) {
        finishMoveResize(true);
        setMoveResizePointerButtonDown(false);
        updateCursor();
    } else if (direction >= NET::TopLeft && direction <= NET::Left) {
        static const Position convert[] = {
            PositionTopLeft,
            PositionTop,
            PositionTopRight,
            PositionRight,
            PositionBottomRight,
            PositionBottom,
            PositionBottomLeft,
            PositionLeft
        };
        if (!isResizable() || isShade())
            return;
        if (isMoveResize())
            finishMoveResize(false);
        setMoveResizePointerButtonDown(true);
        setMoveOffset(QPoint(x_root - x(), y_root - y()));  // map from global
        setInvertedMoveOffset(rect().bottomRight() - moveOffset());
        setUnrestrictedMoveResize(false);
        setMoveResizePointerMode(convert[ direction ]);
        if (!startMoveResize())
            setMoveResizePointerButtonDown(false);
        updateCursor();
    } else if (direction == NET::KeyboardMove) {
        // ignore mouse coordinates given in the message, mouse position is used by the moving algorithm
        Cursor::setPos(geometry().center());
        performMouseCommand(Options::MouseUnrestrictedMove, geometry().center());
    } else if (direction == NET::KeyboardSize) {
        // ignore mouse coordinates given in the message, mouse position is used by the resizing algorithm
        Cursor::setPos(geometry().bottomRight());
        performMouseCommand(Options::MouseUnrestrictedResize, geometry().bottomRight());
    }
}

void Client::keyPressEvent(uint key_code, xcb_timestamp_t time)
{
    updateUserTime(time);
    AbstractClient::keyPressEvent(key_code);
}

// ****************************************
// Unmanaged
// ****************************************

bool Unmanaged::windowEvent(xcb_generic_event_t *e)
{
    double old_opacity = opacity();
    NET::Properties dirtyProperties;
    NET::Properties2 dirtyProperties2;
    info->event(e, &dirtyProperties, &dirtyProperties2);   // pass through the NET stuff
    if (dirtyProperties2 & NET::WM2Opacity) {
        if (compositing()) {
            addRepaintFull();
            emit opacityChanged(this, old_opacity);
        }
    }
    if (dirtyProperties2 & NET::WM2OpaqueRegion) {
        getWmOpaqueRegion();
    }
    if (dirtyProperties2.testFlag(NET::WM2WindowRole)) {
        emit windowRoleChanged();
    }
    if (dirtyProperties2.testFlag(NET::WM2WindowClass)) {
        getResourceClass();
    }
    const uint8_t eventType = e->response_type & ~0x80;
    switch (eventType) {
    case XCB_DESTROY_NOTIFY:
        release(ReleaseReason::Destroyed);
        break;
    case XCB_UNMAP_NOTIFY:{
        workspace()->updateFocusMousePosition(Cursor::pos()); // may cause leave event

        // unmap notify might have been emitted due to a destroy notify
        // but unmap notify gets emitted before the destroy notify, nevertheless at this
        // point the window is already destroyed. This means any XCB request with the window
        // will cause an error.
        // To not run into these errors we try to wait for the destroy notify. For this we
        // generate a round trip to the X server and wait a very short time span before
        // handling the release.
        updateXTime();
        // using 1 msec to not just move it at the end of the event loop but add an very short
        // timespan to cover cases like unmap() followed by destroy(). The only other way to
        // ensure that the window is not destroyed when we do the release handling is to grab
        // the XServer which we do not want to do for an Unmanaged. The timespan of 1 msec is
        // short enough to not cause problems in the close window animations.
        // It's of course still possible that we miss the destroy in which case non-fatal
        // X errors are reported to the event loop and logged by Qt.
        QTimer::singleShot(1, this, SLOT(release()));
        break;
    }
    case XCB_CONFIGURE_NOTIFY:
        configureNotifyEvent(reinterpret_cast<xcb_configure_notify_event_t*>(e));
        break;
    case XCB_PROPERTY_NOTIFY:
        propertyNotifyEvent(reinterpret_cast<xcb_property_notify_event_t*>(e));
        break;
    case XCB_CLIENT_MESSAGE:
        clientMessageEvent(reinterpret_cast<xcb_client_message_event_t*>(e));
        break;
    default: {
        if (eventType == Xcb::Extensions::self()->shapeNotifyEvent()) {
            detectShape(window());
            addRepaintFull();
            addWorkspaceRepaint(geometry());  // in case shape change removes part of this window
            emit geometryShapeChanged(this, geometry());
        }
        if (eventType == Xcb::Extensions::self()->damageNotifyEvent())
            damageNotifyEvent();
        break;
    }
    }
    return false; // don't eat events, even our own unmanaged widgets are tracked
}

void Unmanaged::configureNotifyEvent(xcb_configure_notify_event_t *e)
{
    if (effects)
        static_cast<EffectsHandlerImpl*>(effects)->checkInputWindowStacking(); // keep them on top
    QRect newgeom(e->x, e->y, e->width, e->height);
    if (newgeom != geom) {
        addWorkspaceRepaint(visibleRect());  // damage old area
        QRect old = geom;
        geom = newgeom;
        emit geometryChanged(); // update shadow region
        addRepaintFull();
        if (old.size() != geom.size())
            discardWindowPixmap();
        emit geometryShapeChanged(this, old);
    }
}

// ****************************************
// Toplevel
// ****************************************

void Toplevel::propertyNotifyEvent(xcb_property_notify_event_t *e)
{
    if (e->window != window())
        return; // ignore frame/wrapper
    switch(e->atom) {
    default:
        if (e->atom == atoms->wm_client_leader)
            getWmClientLeader();
        else if (e->atom == atoms->kde_net_wm_shadow)
            getShadow();
        else if (e->atom == atoms->kde_skip_close_animation)
            getSkipCloseAnimation();
        break;
    }
}

void Toplevel::clientMessageEvent(xcb_client_message_event_t *e)
{
    if (e->type == atoms->wl_surface_id) {
        m_surfaceId = e->data.data32[0];
        if (auto w = waylandServer()) {
            if (auto s = KWayland::Server::SurfaceInterface::get(m_surfaceId, w->xWaylandConnection())) {
                setSurface(s);
            }
        }
        emit surfaceIdChanged(m_surfaceId);
    }
}

} // namespace
