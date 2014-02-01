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
#include "decorations.h"
#include "focuschain.h"
#include "netinfo.h"
#include "workspace.h"
#include "atoms.h"
#ifdef KWIN_BUILD_TABBOX
#include "tabbox.h"
#endif
#include "group.h"
#include "overlaywindow.h"
#include "rules.h"
#include "unmanaged.h"
#include "useractions.h"
#include "effects.h"
#ifdef KWIN_BUILD_SCREENEDGES
#include "screenedge.h"
#endif
#include "screens.h"
#include "xcbutils.h"

#include <QApplication>
#include <QDebug>
#include <QWhatsThis>

#include <kkeyserver.h>

#include <xcb/sync.h>
#include <xcb/xcb_icccm.h>

#include "composite.h"
#include "killwindow.h"

namespace KWin
{

extern int currentRefreshRate();

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

/*!
  Handles workspace specific XCB event
 */
bool Workspace::workspaceEvent(xcb_generic_event_t *e)
{
    const uint8_t eventType = e->response_type & ~0x80;
    if (effects && static_cast< EffectsHandlerImpl* >(effects)->hasKeyboardGrab()
            && (eventType == XCB_KEY_PRESS || eventType == XCB_KEY_RELEASE))
        return false; // let Qt process it, it'll be intercepted again in eventFilter()

    if (!m_windowKiller.isNull() && m_windowKiller->isActive() && m_windowKiller->isResponsibleForEvent(eventType)) {
        m_windowKiller->processEvent(e);
        // filter out the event
        return true;
    }

    if (eventType == XCB_PROPERTY_NOTIFY || eventType == XCB_CLIENT_MESSAGE) {
        unsigned long dirty[ NETRootInfo::PROPERTIES_SIZE ];
        rootInfo()->event(e, dirty, NETRootInfo::PROPERTIES_SIZE);
        if (dirty[ NETRootInfo::PROTOCOLS ] & NET::DesktopNames)
            VirtualDesktopManager::self()->save();
        if (dirty[ NETRootInfo::PROTOCOLS2 ] & NET::WM2DesktopLayout)
            VirtualDesktopManager::self()->updateLayout();
    }

    // events that should be handled before Clients can get them
    switch (eventType) {
    case XCB_BUTTON_PRESS:
    case XCB_BUTTON_RELEASE: {
        was_user_interaction = true;
        auto *mouseEvent = reinterpret_cast<xcb_button_press_event_t*>(e);
#ifdef KWIN_BUILD_TABBOX
        if (TabBox::TabBox::self()->isGrabbed()) {
            return TabBox::TabBox::self()->handleMouseEvent(mouseEvent);
        }
#endif
        if (effects && static_cast<EffectsHandlerImpl*>(effects)->checkInputWindowEvent(mouseEvent)) {
            return true;
        }
        break;
    }
    case XCB_MOTION_NOTIFY: {
        auto *mouseEvent = reinterpret_cast<xcb_motion_notify_event_t*>(e);
        const QPoint rootPos(mouseEvent->root_x, mouseEvent->root_y);
#ifdef KWIN_BUILD_TABBOX
        if (TabBox::TabBox::self()->isGrabbed()) {
#ifdef KWIN_BUILD_SCREENEDGES
            ScreenEdges::self()->check(rootPos, QDateTime::fromMSecsSinceEpoch(xTime()), true);
#endif
            return TabBox::TabBox::self()->handleMouseEvent(mouseEvent);
        }
#endif
        if (effects && static_cast<EffectsHandlerImpl*>(effects)->checkInputWindowEvent(mouseEvent)) {
            return true;
        }
#ifdef KWIN_BUILD_SCREENEDGES
        if (QWidget::mouseGrabber()) {
            ScreenEdges::self()->check(rootPos, QDateTime::fromMSecsSinceEpoch(xTime()), true);
        } else {
            ScreenEdges::self()->check(rootPos, QDateTime::fromMSecsSinceEpoch(mouseEvent->time));
        }
#endif
        break;
    }
    case XCB_KEY_PRESS: {
        was_user_interaction = true;
        int keyQt;
        KKeyServer::xcbKeyPressEventToQt(reinterpret_cast<xcb_key_press_event_t*>(e), &keyQt);
//            qDebug() << "Workspace::keyPress( " << keyQt << " )";
        if (movingClient) {
            movingClient->keyPressEvent(keyQt);
            return true;
        }
#ifdef KWIN_BUILD_TABBOX
        if (TabBox::TabBox::self()->isGrabbed()) {
            TabBox::TabBox::self()->keyPress(keyQt);
            return true;
        }
#endif
        break;
    }
    case XCB_KEY_RELEASE:
        was_user_interaction = true;
#ifdef KWIN_BUILD_TABBOX
        if (TabBox::TabBox::self()->isGrabbed()) {
            TabBox::TabBox::self()->keyRelease(reinterpret_cast<xcb_key_release_event_t*>(e));
            return true;
        }
#endif
        break;
    case XCB_CONFIGURE_NOTIFY:
        if (reinterpret_cast<xcb_configure_notify_event_t*>(e)->event == rootWindow())
            x_stacking_dirty = true;
        break;
    };

    const xcb_window_t eventWindow = findEventWindow(e);
    if (eventWindow != XCB_WINDOW_NONE) {
        if (Client* c = findClient(WindowMatchPredicate(eventWindow))) {
            if (c->windowEvent(e))
                return true;
        } else if (Client* c = findClient(WrapperIdMatchPredicate(eventWindow))) {
            if (c->windowEvent(e))
                return true;
        } else if (Client* c = findClient(FrameIdMatchPredicate(eventWindow))) {
            if (c->windowEvent(e))
                return true;
        } else if (Client *c = findClient(InputIdMatchPredicate(eventWindow))) {
            if (c->windowEvent(e))
                return true;
        } else if (Unmanaged* c = findUnmanaged(WindowMatchPredicate(eventWindow))) {
            if (c->windowEvent(e))
                return true;
        } else {
            // We want to pass root window property events to effects
            if (eventType == XCB_PROPERTY_NOTIFY) {
                auto *event = reinterpret_cast<xcb_property_notify_event_t*>(e);
                if (event->window == rootWindow()) {
                    emit propertyNotify(event->atom);
                }
            }
        }
    }
    if (movingClient) {
        if (eventType == XCB_BUTTON_PRESS || eventType == XCB_BUTTON_RELEASE) {
            if (movingClient->moveResizeGrabWindow() == reinterpret_cast<xcb_button_press_event_t*>(e)->event && movingClient->windowEvent(e)) {
                // we need to pass the button release event to the decoration, otherwise Qt still thinks the button is pressed.
                return eventType == XCB_BUTTON_PRESS;
            }
        } else if (eventType == XCB_MOTION_NOTIFY) {
            if (movingClient->moveResizeGrabWindow() == reinterpret_cast<xcb_motion_notify_event_t*>(e)->event && movingClient->windowEvent(e)) {
                return true;
            }
        }
    }

    switch (eventType) {
    case XCB_CREATE_NOTIFY: {
        const auto *event = reinterpret_cast<xcb_create_notify_event_t*>(e);
        if (event->parent == rootWindow() &&
                !QWidget::find(event->window) &&
                !event->override_redirect) {
            // see comments for allowClientActivation()
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
    case XCB_DESTROY_NOTIFY: {
        return false;
    }
    case XCB_MAP_REQUEST: {
        updateXTime();

        const auto *event = reinterpret_cast<xcb_map_request_event_t*>(e);
        if (Client* c = findClient(WindowMatchPredicate(event->window))) {
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
            Unmanaged* c = findUnmanaged(WindowMatchPredicate(event->window));
            if (c == NULL)
                c = createUnmanaged(event->window);
            if (c)
                return c->windowEvent(e);
        }
        return (event->event != event->window);   // hide wm typical event from Qt
    }

    case XCB_ENTER_NOTIFY: {
        if (QWhatsThis::inWhatsThisMode()) {
            QWidget* w = QWidget::find(reinterpret_cast<xcb_enter_notify_event_t*>(e)->event);
            if (w)
                QWhatsThis::leaveWhatsThisMode();
        }
#ifdef KWIN_BUILD_SCREENEDGES
        if (ScreenEdges::self()->isEntered(reinterpret_cast<xcb_enter_notify_event_t*>(e)))
            return true;
#endif
        break;
    }
    case XCB_LEAVE_NOTIFY: {
        if (!QWhatsThis::inWhatsThisMode())
            break;
        // TODO is this cliente ever found, given that client events are searched above?
        const auto *event = reinterpret_cast<xcb_leave_notify_event_t*>(e);
        Client* c = findClient(FrameIdMatchPredicate(event->event));
        if (c && event->detail != XCB_NOTIFY_DETAIL_INFERIOR)
            QWhatsThis::leaveWhatsThisMode();
        break;
    }
    case XCB_CONFIGURE_REQUEST: {
        const auto *event = reinterpret_cast<xcb_configure_request_event_t*>(e);
        if (event->parent == rootWindow()) {
            // TODO: this should be ported to xcb
            XWindowChanges wc;
            wc.border_width = event->border_width;
            wc.x = event->x;
            wc.y = event->y;
            wc.width = event->width;
            wc.height = event->height;
            wc.sibling = XCB_WINDOW_NONE;
            wc.stack_mode = XCB_STACK_MODE_ABOVE;
            unsigned int value_mask = event->value_mask
                                      & (XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y | XCB_CONFIG_WINDOW_WIDTH | XCB_CONFIG_WINDOW_HEIGHT | XCB_CONFIG_WINDOW_BORDER_WIDTH);
            XConfigureWindow(display(), event->window, value_mask, &wc);
            return true;
        }
        break;
    }
    case XCB_FOCUS_IN: {
        const auto *event = reinterpret_cast<xcb_focus_in_event_t*>(e);
        if (event->event == rootWindow()
                && (event->detail == XCB_NOTIFY_DETAIL_NONE || event->detail == XCB_NOTIFY_DETAIL_POINTER_ROOT)) {
            Xcb::CurrentInput currentInput;
            updateXTime(); // focusToNull() uses xTime(), which is old now (FocusIn has no timestamp)
            if (!currentInput.isNull() && (currentInput->focus == XCB_WINDOW_NONE || currentInput->focus == XCB_INPUT_FOCUS_POINTER_ROOT)) {
                //kWarning( 1212 ) << "X focus set to None/PointerRoot, reseting focus" ;
                Client *c = mostRecentlyActivatedClient();
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
    case XCB_CLIENT_MESSAGE:
#ifdef KWIN_BUILD_SCREENEDGES
        if (ScreenEdges::self()->isEntered(reinterpret_cast<xcb_client_message_event_t*>(e)))
            return true;
#endif
        break;
    case XCB_EXPOSE: {
        const auto *event = reinterpret_cast<xcb_expose_event_t*>(e);
        if (compositing()
                && (event->window == rootWindow()   // root window needs repainting
                    || (m_compositor->overlayWindow() != XCB_WINDOW_NONE && event->window == m_compositor->overlayWindow()))) { // overlay needs repainting
            m_compositor->addRepaint(event->x, event->y, event->width, event->height);
        }
        break;
    }
    case XCB_VISIBILITY_NOTIFY: {
        const auto *event = reinterpret_cast<xcb_visibility_notify_event_t*>(e);
        if (compositing() && m_compositor->overlayWindow() != XCB_WINDOW_NONE && event->window == m_compositor->overlayWindow()) {
            bool was_visible = m_compositor->isOverlayWindowVisible();
            m_compositor->setOverlayWindowVisibility((event->state != XCB_VISIBILITY_FULLY_OBSCURED));
            if (!was_visible && m_compositor->isOverlayWindowVisible()) {
                // hack for #154825
                m_compositor->addRepaintFull();
                QTimer::singleShot(2000, m_compositor, SLOT(addRepaintFull()));
            }
            m_compositor->scheduleRepaint();
        }
        break;
    }
    default:
        if (eventType == Xcb::Extensions::self()->randrNotifyEvent() && Xcb::Extensions::self()->isRandrAvailable()) {
            auto *event = reinterpret_cast<xcb_randr_screen_change_notify_event_t*>(e);
            xcb_screen_t *screen = defaultScreen();
            if (event->rotation & (XCB_RANDR_ROTATION_ROTATE_90 | XCB_RANDR_ROTATION_ROTATE_270)) {
                screen->width_in_pixels = event->height;
                screen->height_in_pixels = event->width;
                screen->width_in_millimeters = event->mheight;
                screen->height_in_millimeters = event->mwidth;
            } else {
                screen->width_in_pixels = event->width;
                screen->height_in_pixels = event->height;
                screen->width_in_millimeters = event->mwidth;
                screen->height_in_millimeters = event->mheight;
            }
            if (compositing()) {
                // desktopResized() should take care of when the size or
                // shape of the desktop has changed, but we also want to
                // catch refresh rate changes
                if (m_compositor->xrrRefreshRate() != currentRefreshRate())
                    m_compositor->setCompositeResetTimer(0);
            }

        } else if (eventType == Xcb::Extensions::self()->syncAlarmNotifyEvent() && Xcb::Extensions::self()->isSyncAvailable()) {
            for (Client *c : clients)
                c->syncEvent(reinterpret_cast< xcb_sync_alarm_notify_event_t* >(e));
            for (Client *c : desktops)
                c->syncEvent(reinterpret_cast< xcb_sync_alarm_notify_event_t* >(e));
        } else if (eventType == Xcb::Extensions::self()->fixesCursorNotifyEvent() && Xcb::Extensions::self()->isFixesAvailable()) {
            Cursor::self()->notifyCursorChanged(reinterpret_cast<xcb_xfixes_cursor_notify_event_t*>(e)->cursor_serial);
        }
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

/*!
  General handler for XEvents concerning the client window
 */
bool Client::windowEvent(xcb_generic_event_t *e)
{
    if (findEventWindow(e) == window()) { // avoid doing stuff on frame or wrapper
        unsigned long dirty[ 2 ];
        double old_opacity = opacity();
        info->event(e, dirty, 2);   // pass through the NET stuff

        if ((dirty[ NETWinInfo::PROTOCOLS ] & NET::WMName) != 0)
            fetchName();
        if ((dirty[ NETWinInfo::PROTOCOLS ] & NET::WMIconName) != 0)
            fetchIconicName();
        if ((dirty[ NETWinInfo::PROTOCOLS ] & NET::WMStrut) != 0
                || (dirty[ NETWinInfo::PROTOCOLS2 ] & NET::WM2ExtendedStrut) != 0) {
            workspace()->updateClientArea();
        }
        if ((dirty[ NETWinInfo::PROTOCOLS ] & NET::WMIcon) != 0)
            getIcons();
        // Note there's a difference between userTime() and info->userTime()
        // info->userTime() is the value of the property, userTime() also includes
        // updates of the time done by KWin (ButtonPress on windowrapper etc.).
        if ((dirty[ NETWinInfo::PROTOCOLS2 ] & NET::WM2UserTime) != 0) {
            workspace()->setWasUserInteraction();
            updateUserTime(info->userTime());
        }
        if ((dirty[ NETWinInfo::PROTOCOLS2 ] & NET::WM2StartupId) != 0)
            startupIdChanged();
        if (dirty[ NETWinInfo::PROTOCOLS2 ] & NET::WM2Opacity) {
            if (compositing()) {
                addRepaintFull();
                emit opacityChanged(this, old_opacity);
            } else {
                // forward to the frame if there's possibly another compositing manager running
                NETWinInfo i(connection(), frameId(), rootWindow(), 0);
                i.setOpacity(info->opacity());
            }
        }
        if (dirty[ NETWinInfo::PROTOCOLS2 ] & NET::WM2FrameOverlap) {
            // ### Inform the decoration
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
        updateUserTime();
        workspace()->setWasUserInteraction();
        break;
    case XCB_BUTTON_PRESS: {
        const auto *event = reinterpret_cast<xcb_button_press_event_t*>(e);
        updateUserTime();
        workspace()->setWasUserInteraction();
        buttonPressEvent(event->event, event->detail, event->state,
                         event->event_x, event->event_y, event->root_x, event->root_y);
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

/*!
  Handles map requests of the client window
 */
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

/*!
  Handles unmap notify events of the client window
 */
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


/*!
   Handles client messages for the client window
*/
void Client::clientMessageEvent(xcb_client_message_event_t *e)
{
    if (e->window != window())
        return; // ignore frame/wrapper
    // WM_STATE
    if (e->type == atoms->kde_wm_change_state) {
        bool avoid_animation = (e->data.data32[ 1 ]);
        if (e->data.data32[ 0 ] == XCB_ICCCM_WM_STATE_ICONIC)
            minimize();
        else if (e->data.data32[ 0 ] == XCB_ICCCM_WM_STATE_NORMAL) {
            // copied from mapRequest()
            if (isMinimized())
                unminimize(avoid_animation);
            if (isShade())
                setShade(ShadeNone);
            if (!isOnCurrentDesktop()) {
                if (workspace()->allowClientActivation(this))
                    workspace()->activateClient(this);
                else
                    demandAttention();
            }
        }
    } else if (e->type == atoms->wm_change_state) {
        if (e->data.data32[0] == XCB_ICCCM_WM_STATE_ICONIC)
            minimize();
        return;
    }
}


/*!
  Handles configure  requests of the client window
 */
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


/*!
  Handles property changes of the client window
 */
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
        getWMHints();
        getIcons(); // because KWin::icon() uses WMHints as fallback
        break;
    default:
        if (e->atom == atoms->wm_protocols)
            getWindowProtocols();
        else if (e->atom == atoms->motif_wm_hints)
            getMotifHints();
        else if (e->atom == atoms->net_wm_sync_request_counter)
            getSyncCounter();
        else if (e->atom == atoms->activities)
            checkActivities();
        else if (e->atom == atoms->kde_net_wm_block_compositing)
            updateCompositeBlocking(true);
        else if (e->atom == atoms->kde_first_in_window_list)
            updateFirstInTabBox();
        else if (e->atom == atoms->kde_color_sheme)
            updateColorScheme();
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

        if (options->focusPolicy() == Options::ClickToFocus || workspace()->userActionsMenu()->isShown())
            return;

        QPoint currentPos(e->root_x, e->root_y);
        if (options->isAutoRaise() && !isDesktop() &&
                !isDock() && workspace()->focusChangeEnabled() &&
                currentPos != workspace()->focusMousePosition() &&
                workspace()->topClientOnDesktop(VirtualDesktopManager::self()->current(),
                                                options->isSeparateScreenFocus() ? screen() : -1) != this) {
            delete autoRaiseTimer;
            autoRaiseTimer = new QTimer(this);
            connect(autoRaiseTimer, SIGNAL(timeout()), this, SLOT(autoRaise()));
            autoRaiseTimer->setSingleShot(true);
            autoRaiseTimer->start(options->autoRaiseInterval());
        }

        if (isDesktop() || isDock())
            return;
        // for FocusFollowsMouse, change focus only if the mouse has actually been moved, not if the focus
        // change came because of window changes (e.g. closing a window) - #92290
        if (options->focusPolicy() != Options::FocusFollowsMouse
                || currentPos != workspace()->focusMousePosition()) {
                workspace()->requestDelayFocus(this);
        }
        return;
    }
}

void Client::leaveNotifyEvent(xcb_leave_notify_event_t *e)
{
    if (e->event != frameId())
        return; // care only about leaving the whole frame
    if (e->mode == XCB_NOTIFY_MODE_NORMAL) {
        if (!buttonDown) {
            mode = PositionCenter;
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
            // TODO: port to XCB
            int d1, d2, d3, d4;
            unsigned int d5;
            Window w, child;
            if (XQueryPointer(display(), frameId(), &w, &child, &d1, &d2, &d3, &d4, &d5) == False
                    || child == XCB_WINDOW_NONE)
                lostMouse = true; // really lost the mouse
        }
        if (lostMouse) {
            cancelAutoRaise();
            workspace()->cancelDelayFocus();
            cancelShadeHoverTimer();
            if (shade_mode == ShadeHover && !moveResizeMode && !buttonDown) {
                shadeHoverTimer = new QTimer(this);
                connect(shadeHoverTimer, SIGNAL(timeout()), this, SLOT(shadeUnhover()));
                shadeHoverTimer->setSingleShot(true);
                shadeHoverTimer->start(options->shadeHoverInterval());
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

/*
  Releases the passive grab for some modifier combinations when a
  window becomes active. This helps broken X programs that
  missinterpret LeaveNotify events in grab mode to work properly
  (Motif, AWT, Tk, ...)
 */
void Client::updateMouseGrab()
{
    if (workspace()->globalShortcutsDisabled()) {
        m_wrapper.ungrabButton();
        // keep grab for the simple click without modifiers if needed (see below)
        bool not_obscured = workspace()->topClientOnDesktop(VirtualDesktopManager::self()->current(), -1, true, false) == this;
        if (!(!options->isClickRaise() || not_obscured))
            grabButton(None);
        return;
    }
    if (isActive() && !workspace()->forcedGlobalMouseGrab()) { // see Workspace::establishTabBoxGrab()
        // first grab all modifier combinations
        m_wrapper.grabButton(XCB_GRAB_MODE_SYNC, XCB_GRAB_MODE_ASYNC);
        // remove the grab for no modifiers only if the window
        // is unobscured or if the user doesn't want click raise
        // (it is unobscured if it the topmost in the unconstrained stacking order, i.e. it is
        // the most recently raised window)
        bool not_obscured = workspace()->topClientOnDesktop(VirtualDesktopManager::self()->current(), -1, true, false) == this;
        if (!options->isClickRaise() || not_obscured)
            ungrabButton(None);
        else
            grabButton(None);
        ungrabButton(ShiftMask);
        ungrabButton(ControlMask);
        ungrabButton(ControlMask | ShiftMask);
    } else {
        m_wrapper.ungrabButton();
        // simply grab all modifier combinations
        m_wrapper.grabButton(XCB_GRAB_MODE_SYNC, XCB_GRAB_MODE_ASYNC);
    }
}

// Qt propagates mouse events up the widget hierachy, which means events
// for the decoration window cannot be (easily) intercepted as X11 events
bool Client::eventFilter(QObject* o, QEvent* e)
{
    if (decoration == NULL
            || o != decoration->widget())
        return false;
    if (e->type() == QEvent::MouseButtonPress) {
        QMouseEvent* ev = static_cast< QMouseEvent* >(e);
        return buttonPressEvent(decorationId(), qtToX11Button(ev->button()), qtToX11State(ev->buttons(), ev->modifiers()),
                                ev->x(), ev->y(), ev->globalX(), ev->globalY());
    }
    if (e->type() == QEvent::MouseButtonRelease) {
        QMouseEvent* ev = static_cast< QMouseEvent* >(e);
        return buttonReleaseEvent(decorationId(), qtToX11Button(ev->button()), qtToX11State(ev->buttons(), ev->modifiers()),
                                  ev->x(), ev->y(), ev->globalX(), ev->globalY());
    }
    if (e->type() == QEvent::MouseMove) { // FRAME i fake z enter/leave?
        QMouseEvent* ev = static_cast< QMouseEvent* >(e);
        return motionNotifyEvent(decorationId(), qtToX11State(ev->buttons(), ev->modifiers()),
                                 ev->x(), ev->y(), ev->globalX(), ev->globalY());
    }
    if (e->type() == QEvent::Wheel) {
        QWheelEvent* ev = static_cast< QWheelEvent* >(e);
        bool r = buttonPressEvent(decorationId(), ev->delta() > 0 ? Button4 : Button5, qtToX11State(ev->buttons(), ev->modifiers()),
                                  ev->x(), ev->y(), ev->globalX(), ev->globalY());
        r = r || buttonReleaseEvent(decorationId(), ev->delta() > 0 ? Button4 : Button5, qtToX11State(ev->buttons(), ev->modifiers()),
                                    ev->x(), ev->y(), ev->globalX(), ev->globalY());
        return r;
    }
    if (e->type() == QEvent::Resize) {
        QResizeEvent* ev = static_cast< QResizeEvent* >(e);
        // Filter out resize events that inform about size different than frame size.
        // This will ensure that decoration->width() etc. and decoration->widget()->width() will be in sync.
        // These events only seem to be delayed events from initial resizing before show() was called
        // on the decoration widget.
        if (ev->size() != (size() + QSize(padding_left + padding_right, padding_top + padding_bottom)))
            return true;
        // HACK: Avoid decoration redraw delays. On resize Qt sets WA_WStateConfigPending
        // which delays all painting until a matching ConfigureNotify event comes.
        // But this process itself is the window manager, so it's not needed
        // to wait for that event, the geometry is known.
        // Note that if Qt in the future changes how this flag is handled and what it
        // triggers then this may potentionally break things. See mainly QETWidget::translateConfigEvent().
        decoration->widget()->setAttribute(Qt::WA_WState_ConfigPending, false);
        decoration->widget()->update();
        return false;
    }
    if (e->type() == QEvent::ZOrderChange) {
        // when the user actions menu is closed by clicking on the window decoration (which is not unlikely)
        // Qt will raise the decoration window and thus the decoration window is above the actual Client
        // see QWidgetWindow::handleMouseEvent (qtbase/src/widgets/kernel/qwidgetwindow.cpp)
        // when this happens also a ZOrderChange event is sent, we intercept all of them and make sure that
        // the window is lowest in stack again.
        Xcb::lowerWindow(decorationId());
    }
    return false;
}

static bool modKeyDown(int state) {
    const uint keyModX = (options->keyCmdAllModKey() == Qt::Key_Meta) ?
                                                    KKeyServer::modXMeta() : KKeyServer::modXAlt();
    return keyModX  && (state & KKeyServer::accelModMaskX()) == keyModX;
}


// return value matters only when filtering events before decoration gets them
bool Client::buttonPressEvent(xcb_window_t w, int button, int state, int x, int y, int x_root, int y_root)
{
    if (buttonDown) {
        if (w == wrapperId())
            xcb_allow_events(connection(), XCB_ALLOW_SYNC_POINTER, XCB_TIME_CURRENT_TIME);  //xTime());
        return true;
    }

    if (w == wrapperId() || w == frameId() || w == decorationId() || w == inputId()) {
        // FRAME neco s tohohle by se melo zpracovat, nez to dostane dekorace
        updateUserTime();
        workspace()->setWasUserInteraction();
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
        bool perform_handled = false;
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
            // inactive inner window
            if (!isActive() && w == wrapperId() && button < 6) {
                was_action = true;
                perform_handled = true;
                switch(button) {
                case XCB_BUTTON_INDEX_1:
                    com = options->commandWindow1();
                    break;
                case XCB_BUTTON_INDEX_2:
                    com = options->commandWindow2();
                    break;
                case XCB_BUTTON_INDEX_3:
                    com = options->commandWindow3();
                    break;
                case XCB_BUTTON_INDEX_4:
                case XCB_BUTTON_INDEX_5:
                    com = options->commandWindowWheel();
                    break;
                }
            }
            // active inner window
            if (isActive() && w == wrapperId()
                    && options->isClickRaise() && button < 4) { // exclude wheel
                com = Options::MouseActivateRaiseAndPassClick;
                was_action = true;
                perform_handled = true;
            }
        }
        if (was_action) {
            bool replay = performMouseCommand(com, QPoint(x_root, y_root), perform_handled);

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
        x = x_root - geometry().x() + padding_left;
        y = y_root - geometry().y() + padding_top;
        // New API processes core events FIRST and only passes unused ones to the decoration
        return processDecorationButtonPress(button, state, x, y, x_root, y_root, true);
    }
    if (w == decorationId()) {
        // New API processes core events FIRST and only passes unused ones to the decoration
        return processDecorationButtonPress(button, state, x, y, x_root, y_root, true);
    }
    if (w == frameId())
        processDecorationButtonPress(button, state, x, y, x_root, y_root);
    return true;
}


// this function processes button press events only after decoration decides not to handle them,
// unlike buttonPressEvent(), which (when the window is decoration) filters events before decoration gets them
bool Client::processDecorationButtonPress(int button, int /*state*/, int x, int y, int x_root, int y_root,
        bool ignoreMenu)
{
    Options::MouseCommand com = Options::MouseNothing;
    bool active = isActive();
    if (!wantsInput())    // we cannot be active, use it anyway
        active = true;

    if (button == XCB_BUTTON_INDEX_1)
        com = active ? options->commandActiveTitlebar1() : options->commandInactiveTitlebar1();
    else if (button == XCB_BUTTON_INDEX_2)
        com = active ? options->commandActiveTitlebar2() : options->commandInactiveTitlebar2();
    else if (button == XCB_BUTTON_INDEX_3)
        com = active ? options->commandActiveTitlebar3() : options->commandInactiveTitlebar3();
    if (button == XCB_BUTTON_INDEX_1
            && com != Options::MouseOperationsMenu // actions where it's not possible to get the matching
            && com != Options::MouseMinimize  // mouse release event
            && com != Options::MouseDragTab) {
        mode = mousePosition(QPoint(x, y));
        buttonDown = true;
        moveOffset = QPoint(x - padding_left, y - padding_top);
        invertedMoveOffset = rect().bottomRight() - moveOffset;
        unrestrictedMoveResize = false;
        startDelayedMoveResize();
        updateCursor();
    }
    // In the new API the decoration may process the menu action to display an inactive tab's menu.
    // If the event is unhandled then the core will create one for the active window in the group.
    if (!ignoreMenu || com != Options::MouseOperationsMenu)
        performMouseCommand(com, QPoint(x_root, y_root));
    return !( // Return events that should be passed to the decoration in the new API
               com == Options::MouseRaise ||
               com == Options::MouseOperationsMenu ||
               com == Options::MouseActivateAndRaise ||
               com == Options::MouseActivate ||
               com == Options::MouseActivateRaiseAndPassClick ||
               com == Options::MouseActivateAndPassClick ||
               com == Options::MouseDragTab ||
               com == Options::MouseNothing);
}

// called from decoration
void Client::processMousePressEvent(QMouseEvent* e)
{
    if (e->type() != QEvent::MouseButtonPress) {
        qWarning() << "processMousePressEvent()" ;
        return;
    }
    int button;
    switch(e->button()) {
    case Qt::LeftButton:
        button = XCB_BUTTON_INDEX_1;
        break;
    case Qt::MidButton:
        button = XCB_BUTTON_INDEX_2;
        break;
    case Qt::RightButton:
        button = XCB_BUTTON_INDEX_3;
        break;
    default:
        return;
    }
    processDecorationButtonPress(button, e->buttons(), e->x(), e->y(), e->globalX(), e->globalY());
}

// return value matters only when filtering events before decoration gets them
bool Client::buttonReleaseEvent(xcb_window_t w, int button, int state, int x, int y, int x_root, int y_root)
{
    if (w == decorationId() && !buttonDown)
        return false;
    if (w == wrapperId()) {
        xcb_allow_events(connection(), XCB_ALLOW_SYNC_POINTER, XCB_TIME_CURRENT_TIME);  //xTime());
        return true;
    }
    if (w != frameId() && w != decorationId() && w != inputId() && w != moveResizeGrabWindow())
        return true;
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
        buttonDown = false;
        stopDelayedMoveResize();
        if (moveResizeMode) {
            finishMoveResize(false);
            // mouse position is still relative to old Client position, adjust it
            QPoint mousepos(x_root - x + padding_left, y_root - y + padding_top);
            mode = mousePosition(mousepos);
        } else if (decorationPlugin()->supportsTabbing())
            return false;
        updateCursor();
    }
    return true;
}

// Checks if the mouse cursor is near the edge of the screen and if so activates quick tiling or maximization
void Client::checkQuickTilingMaximizationZones(int xroot, int yroot)
{

    QuickTileMode mode = QuickTileNone;
    for (int i=0; i<screens()->count(); ++i) {

        if (!screens()->geometry(i).contains(QPoint(xroot, yroot)))
            continue;

        QRect area = workspace()->clientArea(MaximizeArea, QPoint(xroot, yroot), desktop());
        if (options->electricBorderTiling()) {
        if (xroot <= area.x() + 20)
            mode |= QuickTileLeft;
        else if (xroot >= area.x() + area.width() - 20)
            mode |= QuickTileRight;
        }

        if (mode != QuickTileNone) {
            if (yroot <= area.y() + area.height() * options->electricBorderCornerRatio())
                mode |= QuickTileTop;
            else if (yroot >= area.y() + area.height() - area.height()  * options->electricBorderCornerRatio())
                mode |= QuickTileBottom;
        } else if (options->electricBorderMaximize() && yroot <= area.y() + 5 && isMaximizable())
            mode = QuickTileMaximize;
        break; // no point in checking other screens to contain this... "point"...
    }
    setElectricBorderMode(mode);
    setElectricBorderMaximizing(mode != QuickTileNone);
}

// return value matters only when filtering events before decoration gets them
bool Client::motionNotifyEvent(xcb_window_t w, int state, int x, int y, int x_root, int y_root)
{
    if (w != frameId() && w != decorationId() && w != inputId() && w != moveResizeGrabWindow())
        return true; // care only about the whole frame
    if (!buttonDown) {
        QPoint mousePos(x, y);
        if (w == frameId())
            mousePos += QPoint(padding_left, padding_top);
        if (w == inputId()) {
            int x = x_root - geometry().x() + padding_left;
            int y = y_root - geometry().y() + padding_top;
            mousePos = QPoint(x, y);
        }
        Position newmode = modKeyDown(state) ? PositionCenter : mousePosition(mousePos);
        if (newmode != mode) {
            mode = newmode;
            updateCursor();
        }
        return false;
    }
    if (w == moveResizeGrabWindow()) {
        x = this->x(); // translate from grab window to local coords
        y = this->y();
    }

    const QRect oldGeo = geometry();
    handleMoveResize(x, y, x_root, y_root);
    if (!isFullScreen() && isMove()) {
        if (quick_tile_mode != QuickTileNone && oldGeo != geometry()) {
            GeometryUpdatesBlocker blocker(this);
            setQuickTileMode(QuickTileNone);
            moveOffset = QPoint(double(moveOffset.x()) / double(oldGeo.width()) * double(geom_restore.width()),
                                double(moveOffset.y()) / double(oldGeo.height()) * double(geom_restore.height()));
            moveResizeGeom = geom_restore;
            handleMoveResize(x, y, x_root, y_root); // fix position
        } else if (quick_tile_mode == QuickTileNone && isResizable()) {
            checkQuickTilingMaximizationZones(x_root, y_root);
        }
    }
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
    if (direction == NET::Move)
        performMouseCommand(Options::MouseMove, QPoint(x_root, y_root));
    else if (moveResizeMode && direction == NET::MoveResizeCancel) {
        finishMoveResize(true);
        buttonDown = false;
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
        if (moveResizeMode)
            finishMoveResize(false);
        buttonDown = true;
        moveOffset = QPoint(x_root - x(), y_root - y());  // map from global
        invertedMoveOffset = rect().bottomRight() - moveOffset;
        unrestrictedMoveResize = false;
        mode = convert[ direction ];
        if (!startMoveResize())
            buttonDown = false;
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

void Client::keyPressEvent(uint key_code)
{
    updateUserTime();
    if (!isMove() && !isResize())
        return;
    bool is_control = key_code & Qt::CTRL;
    bool is_alt = key_code & Qt::ALT;
    key_code = key_code & ~Qt::KeyboardModifierMask;
    int delta = is_control ? 1 : is_alt ? 32 : 8;
    QPoint pos = cursorPos();
    switch(key_code) {
    case Qt::Key_Left:
        pos.rx() -= delta;
        break;
    case Qt::Key_Right:
        pos.rx() += delta;
        break;
    case Qt::Key_Up:
        pos.ry() -= delta;
        break;
    case Qt::Key_Down:
        pos.ry() += delta;
        break;
    case Qt::Key_Space:
    case Qt::Key_Return:
    case Qt::Key_Enter:
        finishMoveResize(false);
        buttonDown = false;
        updateCursor();
        break;
    case Qt::Key_Escape:
        finishMoveResize(true);
        buttonDown = false;
        updateCursor();
        break;
    default:
        return;
    }
    Cursor::setPos(pos);
}

void Client::syncEvent(xcb_sync_alarm_notify_event_t* e)
{
    if (e->alarm == syncRequest.alarm && e->counter_value.hi == syncRequest.value.hi && e->counter_value.lo == syncRequest.value.lo) {
        setReadyForPainting();
        syncRequest.isPending = false;
        if (syncRequest.failsafeTimeout)
            syncRequest.failsafeTimeout->stop();
        if (isResize()) {
            if (syncRequest.timeout)
                syncRequest.timeout->stop();
            performMoveResize();
        } else // setReadyForPainting does as well, but there's a small chance for resize syncs after the resize ended
            addRepaintFull();
    }
}

// ****************************************
// Unmanaged
// ****************************************

bool Unmanaged::windowEvent(xcb_generic_event_t *e)
{
    double old_opacity = opacity();
    unsigned long dirty[ 2 ];
    info->event(e, dirty, 2);   // pass through the NET stuff
    if (dirty[ NETWinInfo::PROTOCOLS2 ] & NET::WM2Opacity) {
        if (compositing()) {
            addRepaintFull();
            emit opacityChanged(this, old_opacity);
        }
    }
    const uint8_t eventType = e->response_type & ~0x80;
    switch (eventType) {
    case XCB_UNMAP_NOTIFY:
        workspace()->updateFocusMousePosition(Cursor::pos());
        release();
        break;
    case XCB_CONFIGURE_NOTIFY:
        configureNotifyEvent(reinterpret_cast<xcb_configure_notify_event_t*>(e));
        break;
    case XCB_PROPERTY_NOTIFY:
        propertyNotifyEvent(reinterpret_cast<xcb_property_notify_event_t*>(e));
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
        else if (e->atom == atoms->wm_window_role)
            getWindowRole();
        else if (e->atom == atoms->kde_net_wm_shadow)
            getShadow();
        else if (e->atom == atoms->net_wm_opaque_region)
            getWmOpaqueRegion();
        else if (e->atom == atoms->kde_skip_close_animation)
            getSkipCloseAnimation();
        break;
    }
    emit propertyNotify(this, e->atom);
}


} // namespace
