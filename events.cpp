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

#include <config-X11.h>

#include "client.h"
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

#include <QWhatsThis>
#include <QApplication>
#include <QtGui/QDesktopWidget>

#include <kkeyserver.h>

#include <X11/extensions/shape.h>
#include <X11/extensions/Xrandr.h>
#include <X11/Xatom.h>
#include <QX11Info>

#include "composite.h"

namespace KWin
{

extern int currentRefreshRate();

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
    m_client->workspace()->sendClientToDesktop(m_client, desktop, true);
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

// ****************************************
// RootInfo
// ****************************************

RootInfo::RootInfo(Workspace* ws, Display *dpy, Window w, const char *name, unsigned long pr[], int pr_num, int scr)
    : NETRootInfo(dpy, w, name, pr, pr_num, scr)
{
    workspace = ws;
}

void RootInfo::changeNumberOfDesktops(int n)
{
    workspace->setNumberOfDesktops(n);
}

void RootInfo::changeCurrentDesktop(int d)
{
    workspace->setCurrentDesktop(d);
}

void RootInfo::changeActiveWindow(Window w, NET::RequestSource src, Time timestamp, Window active_window)
{
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
    if (Client* c = workspace->findClient(WindowMatchPredicate(w))) {
        if (timestamp == CurrentTime)
            timestamp = c->userTime();
        if (src != NET::FromApplication && src != FromTool)
            src = NET::FromTool;
        c->restackWindow(above, detail, src, timestamp, true);
    }
}

void RootInfo::gotTakeActivity(Window w, Time timestamp, long flags)
{
    if (Client* c = workspace->findClient(WindowMatchPredicate(w)))
        workspace->handleTakeActivity(c, timestamp, flags);
}

void RootInfo::closeWindow(Window w)
{
    Client* c = workspace->findClient(WindowMatchPredicate(w));
    if (c)
        c->closeWindow();
}

void RootInfo::moveResize(Window w, int x_root, int y_root, unsigned long direction)
{
    Client* c = workspace->findClient(WindowMatchPredicate(w));
    if (c) {
        updateXTime(); // otherwise grabbing may have old timestamp - this message should include timestamp
        c->NETMoveResize(x_root, y_root, (Direction)direction);
    }
}

void RootInfo::moveResizeWindow(Window w, int flags, int x, int y, int width, int height)
{
    Client* c = workspace->findClient(WindowMatchPredicate(w));
    if (c)
        c->NETMoveResizeWindow(flags, x, y, width, height);
}

void RootInfo::gotPing(Window w, Time timestamp)
{
    if (Client* c = workspace->findClient(WindowMatchPredicate(w)))
        c->gotPing(timestamp);
}

void RootInfo::changeShowingDesktop(bool showing)
{
    workspace->setShowingDesktop(showing);
}

// ****************************************
// Workspace
// ****************************************

/*!
  Handles workspace specific XEvents
 */
bool Workspace::workspaceEvent(XEvent * e)
{
    if (effects && static_cast< EffectsHandlerImpl* >(effects)->hasKeyboardGrab()
            && (e->type == KeyPress || e->type == KeyRelease))
        return false; // let Qt process it, it'll be intercepted again in eventFilter()

    if (e->type == PropertyNotify || e->type == ClientMessage) {
        unsigned long dirty[ NETRootInfo::PROPERTIES_SIZE ];
        rootInfo->event(e, dirty, NETRootInfo::PROPERTIES_SIZE);
        if (dirty[ NETRootInfo::PROTOCOLS ] & NET::DesktopNames)
            saveDesktopSettings();
        if (dirty[ NETRootInfo::PROTOCOLS2 ] & NET::WM2DesktopLayout)
            updateDesktopLayout();
    }

    // events that should be handled before Clients can get them
    switch(e->type) {
    case ButtonPress:
    case ButtonRelease:
        was_user_interaction = true;
        // fallthrough
    case MotionNotify:
#ifdef KWIN_BUILD_TABBOX
        if (tabBox()->isGrabbed()) {
            return tab_box->handleMouseEvent(e);
        }
#endif
        if (effects && static_cast<EffectsHandlerImpl*>(effects)->checkInputWindowEvent(e))
            return true;
        break;
    case KeyPress: {
        was_user_interaction = true;
        int keyQt;
        KKeyServer::xEventToQt(e, &keyQt);
//            kDebug(125) << "Workspace::keyPress( " << keyQt << " )";
        if (movingClient) {
            movingClient->keyPressEvent(keyQt);
            return true;
        }
#ifdef KWIN_BUILD_TABBOX
        if (tabBox()->isGrabbed()) {
            tabBox()->keyPress(keyQt);
            return true;
        }
#endif
        break;
    }
    case KeyRelease:
        was_user_interaction = true;
#ifdef KWIN_BUILD_TABBOX
        if (tabBox()->isGrabbed()) {
            tabBox()->keyRelease(e->xkey);
            return true;
        }
#endif
        break;
    case ConfigureNotify:
        if (e->xconfigure.event == rootWindow())
            x_stacking_dirty = true;
        break;
    };

    if (Client* c = findClient(WindowMatchPredicate(e->xany.window))) {
        if (c->windowEvent(e))
            return true;
    } else if (Client* c = findClient(WrapperIdMatchPredicate(e->xany.window))) {
        if (c->windowEvent(e))
            return true;
    } else if (Client* c = findClient(FrameIdMatchPredicate(e->xany.window))) {
        if (c->windowEvent(e))
            return true;
    } else if (Client *c = findClient(InputIdMatchPredicate(e->xany.window))) {
        if (c->windowEvent(e))
            return true;
    } else if (Unmanaged* c = findUnmanaged(WindowMatchPredicate(e->xany.window))) {
        if (c->windowEvent(e))
            return true;
    } else {
        Window special = findSpecialEventWindow(e);
        if (special != None)
            if (Client* c = findClient(WindowMatchPredicate(special))) {
                if (c->windowEvent(e))
                    return true;
            }

        // We want to pass root window property events to effects
        if (e->type == PropertyNotify && e->xany.window == rootWindow()) {
            XPropertyEvent* re = &e->xproperty;
            emit propertyNotify(re->atom);
        }
    }
    if (movingClient != NULL && movingClient->moveResizeGrabWindow() == e->xany.window
            && (e->type == MotionNotify || e->type == ButtonPress || e->type == ButtonRelease)) {
        if (movingClient->windowEvent(e))
            return true;
    }

    switch(e->type) {
    case CreateNotify:
        if (e->xcreatewindow.parent == rootWindow() &&
                !QWidget::find(e->xcreatewindow.window) &&
                !e->xcreatewindow.override_redirect) {
            // see comments for allowClientActivation()
            Time t = xTime();
            XChangeProperty(display(), e->xcreatewindow.window,
                            atoms->kde_net_wm_user_creation_time, XA_CARDINAL,
                            32, PropModeReplace, (unsigned char *)&t, 1);
        }
        break;

    case UnmapNotify: {
        return (e->xunmap.event != e->xunmap.window);   // hide wm typical event from Qt
    }
    case ReparentNotify: {
        //do not confuse Qt with these events. After all, _we_ are the
        //window manager who does the reparenting.
        return true;
    }
    case DestroyNotify: {
        return false;
    }
    case MapRequest: {
        updateXTime();

        if (Client* c = findClient(WindowMatchPredicate(e->xmaprequest.window))) {
            // e->xmaprequest.window is different from e->xany.window
            // TODO this shouldn't be necessary now
            c->windowEvent(e);
            updateFocusChains(c, FocusChainUpdate);
        } else if ( true /*|| e->xmaprequest.parent != root */ ) {
            // NOTICE don't check for the parent being the root window, this breaks when some app unmaps
            // a window, changes something and immediately maps it back, without giving KWin
            // a chance to reparent it back to root
            // since KWin can get MapRequest only for root window children and
            // children of WindowWrapper (=clients), the check is AFAIK useless anyway
            // NOTICE: The save-set support in Client::mapRequestEvent() actually requires that
            // this code doesn't check the parent to be root.
            if (!createClient(e->xmaprequest.window, false))
                XMapRaised(display(), e->xmaprequest.window);
        }
        return true;
    }
    case MapNotify: {
        if (e->xmap.override_redirect) {
            Unmanaged* c = findUnmanaged(WindowMatchPredicate(e->xmap.window));
            if (c == NULL)
                c = createUnmanaged(e->xmap.window);
            if (c)
                return c->windowEvent(e);
        }
        return (e->xmap.event != e->xmap.window);   // hide wm typical event from Qt
    }

    case EnterNotify: {
        if (QWhatsThis::inWhatsThisMode()) {
            QWidget* w = QWidget::find(e->xcrossing.window);
            if (w)
                QWhatsThis::leaveWhatsThisMode();
        }
#ifdef KWIN_BUILD_SCREENEDGES
        if (m_screenEdge.isEntered(e))
            return true;
#endif
        break;
    }
    case LeaveNotify: {
        if (!QWhatsThis::inWhatsThisMode())
            break;
        // TODO is this cliente ever found, given that client events are searched above?
        Client* c = findClient(FrameIdMatchPredicate(e->xcrossing.window));
        if (c && e->xcrossing.detail != NotifyInferior)
            QWhatsThis::leaveWhatsThisMode();
        break;
    }
    case ConfigureRequest: {
        if (e->xconfigurerequest.parent == rootWindow()) {
            XWindowChanges wc;
            wc.border_width = e->xconfigurerequest.border_width;
            wc.x = e->xconfigurerequest.x;
            wc.y = e->xconfigurerequest.y;
            wc.width = e->xconfigurerequest.width;
            wc.height = e->xconfigurerequest.height;
            wc.sibling = None;
            wc.stack_mode = Above;
            unsigned int value_mask = e->xconfigurerequest.value_mask
                                      & (CWX | CWY | CWWidth | CWHeight | CWBorderWidth);
            XConfigureWindow(display(), e->xconfigurerequest.window, value_mask, &wc);
            return true;
        }
        break;
    }
    case FocusIn:
        if (e->xfocus.window == rootWindow()
                && (e->xfocus.detail == NotifyDetailNone || e->xfocus.detail == NotifyPointerRoot)) {
            updateXTime(); // focusToNull() uses xTime(), which is old now (FocusIn has no timestamp)
            Window focus;
            int revert;
            XGetInputFocus(display(), &focus, &revert);
            if (focus == None || focus == PointerRoot) {
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
        // fall through
    case FocusOut:
        return true; // always eat these, they would tell Qt that KWin is the active app
    case ClientMessage:
#ifdef KWIN_BUILD_SCREENEDGES
        if (m_screenEdge.isEntered(e))
            return true;
#endif
        break;
    case Expose:
        if (compositing()
                && (e->xexpose.window == rootWindow()   // root window needs repainting
                    || (m_compositor->overlayWindow() != None && e->xexpose.window == m_compositor->overlayWindow()))) { // overlay needs repainting
            if (m_compositor) {
                m_compositor->addRepaint(e->xexpose.x, e->xexpose.y, e->xexpose.width, e->xexpose.height);
            }
        }
        break;
    case VisibilityNotify:
        if (compositing() && m_compositor->overlayWindow() != None && e->xvisibility.window == m_compositor->overlayWindow()) {
            bool was_visible = m_compositor->isOverlayWindowVisible();
            m_compositor->setOverlayWindowVisibility((e->xvisibility.state != VisibilityFullyObscured));
            if (!was_visible && m_compositor->isOverlayWindowVisible()) {
                // hack for #154825
                if (m_compositor) {
                    m_compositor->addRepaintFull();
                    QTimer::singleShot(2000, m_compositor, SLOT(addRepaintFull()));
                }
            }
            if (m_compositor) {
                m_compositor->checkCompositeTimer();
            }
        }
        break;
    default:
        if (e->type == Extensions::randrNotifyEvent() && Extensions::randrAvailable()) {
            XRRUpdateConfiguration(e);
            if (compositing() && m_compositor) {
                // desktopResized() should take care of when the size or
                // shape of the desktop has changed, but we also want to
                // catch refresh rate changes
                if (m_compositor->xrrRefreshRate() != currentRefreshRate())
                    m_compositor->setCompositeResetTimer(0);
            }

        } else if (e->type == Extensions::syncAlarmNotifyEvent() && Extensions::syncAvailable()) {
#ifdef HAVE_XSYNC
            foreach (Client * c, clients)
                c->syncEvent(reinterpret_cast< XSyncAlarmNotifyEvent* >(e));
            foreach (Client * c, desktops)
                c->syncEvent(reinterpret_cast< XSyncAlarmNotifyEvent* >(e));
#endif
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

// Some events don't have the actual window which caused the event
// as e->xany.window (e.g. ConfigureRequest), but as some other
// field in the XEvent structure.
Window Workspace::findSpecialEventWindow(XEvent* e)
{
    switch(e->type) {
    case CreateNotify:
        return e->xcreatewindow.window;
    case DestroyNotify:
        return e->xdestroywindow.window;
    case UnmapNotify:
        return e->xunmap.window;
    case MapNotify:
        return e->xmap.window;
    case MapRequest:
        return e->xmaprequest.window;
    case ReparentNotify:
        return e->xreparent.window;
    case ConfigureNotify:
        return e->xconfigure.window;
    case GravityNotify:
        return e->xgravity.window;
    case ConfigureRequest:
        return e->xconfigurerequest.window;
    case CirculateNotify:
        return e->xcirculate.window;
    case CirculateRequest:
        return e->xcirculaterequest.window;
    default:
        return None;
    };
}

// ****************************************
// Client
// ****************************************

/*!
  General handler for XEvents concerning the client window
 */
bool Client::windowEvent(XEvent* e)
{
    if (e->xany.window == window()) { // avoid doing stuff on frame or wrapper
        unsigned long dirty[ 2 ];
        double old_opacity = opacity();
        info->event(e, dirty, 2);   // pass through the NET stuff

        if ((dirty[ WinInfo::PROTOCOLS ] & NET::WMName) != 0)
            fetchName();
        if ((dirty[ WinInfo::PROTOCOLS ] & NET::WMIconName) != 0)
            fetchIconicName();
        if ((dirty[ WinInfo::PROTOCOLS ] & NET::WMStrut) != 0
                || (dirty[ WinInfo::PROTOCOLS2 ] & NET::WM2ExtendedStrut) != 0) {
            workspace()->updateClientArea();
        }
        if ((dirty[ WinInfo::PROTOCOLS ] & NET::WMIcon) != 0)
            getIcons();
        // Note there's a difference between userTime() and info->userTime()
        // info->userTime() is the value of the property, userTime() also includes
        // updates of the time done by KWin (ButtonPress on windowrapper etc.).
        if ((dirty[ WinInfo::PROTOCOLS2 ] & NET::WM2UserTime) != 0) {
            workspace()->setWasUserInteraction();
            updateUserTime(info->userTime());
        }
        if ((dirty[ WinInfo::PROTOCOLS2 ] & NET::WM2StartupId) != 0)
            startupIdChanged();
        if (dirty[ WinInfo::PROTOCOLS ] & NET::WMIconGeometry) {
            if (demandAttentionKNotifyTimer != NULL)
                demandAttentionKNotify();
        }
        if (dirty[ WinInfo::PROTOCOLS2 ] & NET::WM2Opacity) {
            if (compositing()) {
                addRepaintFull();
                emit opacityChanged(this, old_opacity);
            } else {
                // forward to the frame if there's possibly another compositing manager running
                NETWinInfo2 i(display(), frameId(), rootWindow(), 0);
                i.setOpacity(info->opacity());
            }
        }
        if (dirty[ WinInfo::PROTOCOLS2 ] & NET::WM2FrameOverlap) {
            // ### Inform the decoration
        }
    }

    switch(e->type) {
    case UnmapNotify:
        unmapNotifyEvent(&e->xunmap);
        break;
    case DestroyNotify:
        destroyNotifyEvent(&e->xdestroywindow);
        break;
    case MapRequest:
        // this one may pass the event to workspace
        return mapRequestEvent(&e->xmaprequest);
    case ConfigureRequest:
        configureRequestEvent(&e->xconfigurerequest);
        break;
    case PropertyNotify:
        propertyNotifyEvent(&e->xproperty);
        break;
    case KeyPress:
        updateUserTime();
        workspace()->setWasUserInteraction();
        break;
    case ButtonPress:
        updateUserTime();
        workspace()->setWasUserInteraction();
        buttonPressEvent(e->xbutton.window, e->xbutton.button, e->xbutton.state,
                         e->xbutton.x, e->xbutton.y, e->xbutton.x_root, e->xbutton.y_root);
        break;
    case KeyRelease:
        // don't update user time on releases
        // e.g. if the user presses Alt+F2, the Alt release
        // would appear as user input to the currently active window
        break;
    case ButtonRelease:
        // don't update user time on releases
        // e.g. if the user presses Alt+F2, the Alt release
        // would appear as user input to the currently active window
        buttonReleaseEvent(e->xbutton.window, e->xbutton.button, e->xbutton.state,
                           e->xbutton.x, e->xbutton.y, e->xbutton.x_root, e->xbutton.y_root);
        break;
    case MotionNotify:
        motionNotifyEvent(e->xmotion.window, e->xmotion.state,
                          e->xmotion.x, e->xmotion.y, e->xmotion.x_root, e->xmotion.y_root);
        workspace()->updateFocusMousePosition(QPoint(e->xmotion.x_root, e->xmotion.y_root));
        break;
    case EnterNotify:
        enterNotifyEvent(&e->xcrossing);
        // MotionNotify is guaranteed to be generated only if the mouse
        // move start and ends in the window; for cases when it only
        // starts or only ends there, Enter/LeaveNotify are generated.
        // Fake a MotionEvent in such cases to make handle of mouse
        // events simpler (Qt does that too).
        motionNotifyEvent(e->xcrossing.window, e->xcrossing.state,
                          e->xcrossing.x, e->xcrossing.y, e->xcrossing.x_root, e->xcrossing.y_root);
        workspace()->updateFocusMousePosition(QPoint(e->xcrossing.x_root, e->xcrossing.y_root));
        break;
    case LeaveNotify:
        motionNotifyEvent(e->xcrossing.window, e->xcrossing.state,
                          e->xcrossing.x, e->xcrossing.y, e->xcrossing.x_root, e->xcrossing.y_root);
        leaveNotifyEvent(&e->xcrossing);
        // not here, it'd break following enter notify handling
        // workspace()->updateFocusMousePosition( QPoint( e->xcrossing.x_root, e->xcrossing.y_root ));
        break;
    case FocusIn:
        focusInEvent(&e->xfocus);
        break;
    case FocusOut:
        focusOutEvent(&e->xfocus);
        break;
    case ReparentNotify:
        break;
    case ClientMessage:
        clientMessageEvent(&e->xclient);
        break;
    case ColormapChangeMask:
        if (e->xany.window == window()) {
            cmap = e->xcolormap.colormap;
            if (isActive())
                workspace()->updateColormap();
        }
        break;
    default:
        if (e->xany.window == window()) {
            if (e->type == Extensions::shapeNotifyEvent()) {
                detectShape(window());  // workaround for #19644
                updateShape();
            }
        }
        if (e->xany.window == frameId()) {
            if (e->type == Extensions::damageNotifyEvent())
                damageNotifyEvent(reinterpret_cast< XDamageNotifyEvent* >(e));
        }
        break;
    }
    return true; // eat all events
}

/*!
  Handles map requests of the client window
 */
bool Client::mapRequestEvent(XMapRequestEvent* e)
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
void Client::unmapNotifyEvent(XUnmapEvent* e)
{
    if (e->window != window())
        return;
    if (e->event != wrapperId()) {
        // most probably event from root window when initially reparenting
        bool ignore = true;
        if (e->event == rootWindow() && e->send_event)
            ignore = false; // XWithdrawWindow()
        if (ignore)
            return;
    }
    releaseWindow();
}

void Client::destroyNotifyEvent(XDestroyWindowEvent* e)
{
    if (e->window != window())
        return;
    destroyClient();
}


/*!
   Handles client messages for the client window
*/
void Client::clientMessageEvent(XClientMessageEvent* e)
{
    if (e->window != window())
        return; // ignore frame/wrapper
    // WM_STATE
    if (e->message_type == atoms->kde_wm_change_state) {
        bool avoid_animation = (e->data.l[ 1 ]);
        if (e->data.l[ 0 ] == IconicState)
            minimize();
        else if (e->data.l[ 0 ] == NormalState) {
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
    } else if (e->message_type == atoms->wm_change_state) {
        if (e->data.l[0] == IconicState)
            minimize();
        return;
    }
}


/*!
  Handles configure  requests of the client window
 */
void Client::configureRequestEvent(XConfigureRequestEvent* e)
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

    if (e->value_mask & CWBorderWidth) {
        // first, get rid of a window border
        XWindowChanges wc;
        unsigned int value_mask = 0;

        wc.border_width = 0;
        value_mask = CWBorderWidth;
        XConfigureWindow(display(), window(), value_mask, & wc);
    }

    if (e->value_mask & (CWX | CWY | CWHeight | CWWidth))
        configureRequest(e->value_mask, e->x, e->y, e->width, e->height, 0, false);

    if (e->value_mask & CWStackMode)
        restackWindow(e->above, e->detail, NET::FromApplication, userTime(), false);

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
void Client::propertyNotifyEvent(XPropertyEvent* e)
{
    Toplevel::propertyNotifyEvent(e);
    if (e->window != window())
        return; // ignore frame/wrapper
    switch(e->atom) {
    case XA_WM_NORMAL_HINTS:
        getWmNormalHints();
        break;
    case XA_WM_NAME:
        fetchName();
        break;
    case XA_WM_ICON_NAME:
        fetchIconicName();
        break;
    case XA_WM_TRANSIENT_FOR:
        readTransient();
        break;
    case XA_WM_HINTS:
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
        break;
    }
}


void Client::enterNotifyEvent(XCrossingEvent* e)
{
    if (e->window != frameId())
        return; // care only about entering the whole frame

#define MOUSE_DRIVEN_FOCUS (!options->focusPolicyIsReasonable() || \
                            (options->focusPolicy() == Options::FocusFollowsMouse && options->isNextFocusPrefersMouse()))
    if (e->mode == NotifyNormal || (e->mode == NotifyUngrab && MOUSE_DRIVEN_FOCUS)) {

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

        if (options->isAutoRaise() && !isDesktop() &&
                !isDock() && workspace()->focusChangeEnabled() &&
                workspace()->topClientOnDesktop(workspace()->currentDesktop(),
                                                options->isSeparateScreenFocus() ? screen() : -1) != this) {
            delete autoRaiseTimer;
            autoRaiseTimer = new QTimer(this);
            connect(autoRaiseTimer, SIGNAL(timeout()), this, SLOT(autoRaise()));
            autoRaiseTimer->setSingleShot(true);
            autoRaiseTimer->start(options->autoRaiseInterval());
        }

        QPoint currentPos(e->x_root, e->y_root);
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

void Client::leaveNotifyEvent(XCrossingEvent* e)
{
    if (e->window != frameId())
        return; // care only about leaving the whole frame
    if (e->mode == NotifyNormal) {
        if (!buttonDown) {
            mode = PositionCenter;
            updateCursor();
        }
        bool lostMouse = !rect().contains(QPoint(e->x, e->y));
        // 'lostMouse' wouldn't work with e.g. B2 or Keramik, which have non-rectangular decorations
        // (i.e. the LeaveNotify event comes before leaving the rect and no LeaveNotify event
        // comes after leaving the rect) - so lets check if the pointer is really outside the window

        // TODO this still sucks if a window appears above this one - it should lose the mouse
        // if this window is another client, but not if it's a popup ... maybe after KDE3.1 :(
        // (repeat after me 'AARGHL!')
        if (!lostMouse && e->detail != NotifyInferior) {
            int d1, d2, d3, d4;
            unsigned int d5;
            Window w, child;
            if (XQueryPointer(display(), frameId(), &w, &child, &d1, &d2, &d3, &d4, &d5) == False
                    || child == None)
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
        XGrabButton(display(), AnyButton,
                    modifier | mods[ i ],
                    wrapperId(), false, ButtonPressMask,
                    GrabModeSync, GrabModeAsync, None, None);
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
        XUngrabButton(display(), AnyButton,
                      modifier | mods[ i ], wrapperId());
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
        XUngrabButton(display(), AnyButton, AnyModifier, wrapperId());
        // keep grab for the simple click without modifiers if needed (see below)
        bool not_obscured = workspace()->topClientOnDesktop(workspace()->currentDesktop(), -1, true, false) == this;
        if (!(!options->isClickRaise() || not_obscured))
            grabButton(None);
        return;
    }
    if (isActive() && !workspace()->forcedGlobalMouseGrab()) { // see Workspace::establishTabBoxGrab()
        // first grab all modifier combinations
        XGrabButton(display(), AnyButton, AnyModifier, wrapperId(), false,
                    ButtonPressMask,
                    GrabModeSync, GrabModeAsync,
                    None, None);
        // remove the grab for no modifiers only if the window
        // is unobscured or if the user doesn't want click raise
        // (it is unobscured if it the topmost in the unconstrained stacking order, i.e. it is
        // the most recently raised window)
        bool not_obscured = workspace()->topClientOnDesktop(workspace()->currentDesktop(), -1, true, false) == this;
        if (!options->isClickRaise() || not_obscured)
            ungrabButton(None);
        else
            grabButton(None);
        ungrabButton(ShiftMask);
        ungrabButton(ControlMask);
        ungrabButton(ControlMask | ShiftMask);
    } else {
        XUngrabButton(display(), AnyButton, AnyModifier, wrapperId());
        // simply grab all modifier combinations
        XGrabButton(display(), AnyButton, AnyModifier, wrapperId(), false,
                    ButtonPressMask,
                    GrabModeSync, GrabModeAsync,
                    None, None);
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
    return false;
}

static bool modKeyDown(int state) {
    const uint keyModX = (options->keyCmdAllModKey() == Qt::Key_Meta) ?
                                                    KKeyServer::modXMeta() : KKeyServer::modXAlt();
    return keyModX  && (state & KKeyServer::accelModMaskX()) == keyModX;
}


// return value matters only when filtering events before decoration gets them
bool Client::buttonPressEvent(Window w, int button, int state, int x, int y, int x_root, int y_root)
{
    if (buttonDown) {
        if (w == wrapperId())
            XAllowEvents(display(), SyncPointer, CurrentTime);  //xTime());
        return true;
    }

    if (w == wrapperId() || w == frameId() || w == decorationId() || w == inputId()) {
        // FRAME neco s tohohle by se melo zpracovat, nez to dostane dekorace
        updateUserTime();
        workspace()->setWasUserInteraction();
        const bool bModKeyHeld = modKeyDown(state);

        if (isSplash()
                && button == Button1 && !bModKeyHeld) {
            // hide splashwindow if the user clicks on it
            hideClient(true);
            if (w == wrapperId())
                XAllowEvents(display(), SyncPointer, CurrentTime);  //xTime());
            return true;
        }

        Options::MouseCommand com = Options::MouseNothing;
        bool was_action = false;
        bool perform_handled = false;
        if (bModKeyHeld) {
            was_action = true;
            switch(button) {
            case Button1:
                com = options->commandAll1();
                break;
            case Button2:
                com = options->commandAll2();
                break;
            case Button3:
                com = options->commandAll3();
                break;
            case Button4:
            case Button5:
                com = options->operationWindowMouseWheel(button == Button4 ? 120 : -120);
                break;
            }
        } else {
            // inactive inner window
            if (!isActive() && w == wrapperId() && button < 6) {
                was_action = true;
                perform_handled = true;
                switch(button) {
                case Button1:
                    com = options->commandWindow1();
                    break;
                case Button2:
                    com = options->commandWindow2();
                    break;
                case Button3:
                    com = options->commandWindow3();
                    break;
                case Button4:
                case Button5:
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
                XAllowEvents(display(), replay ? ReplayPointer : SyncPointer, CurrentTime); //xTime());
            return true;
        }
    }

    if (w == wrapperId()) { // these can come only from a grab
        XAllowEvents(display(), ReplayPointer, CurrentTime);  //xTime());
        return true;
    }
    if (w == decorationId() || w == inputId()) {
        if (w == inputId()) {
            x = x_root - geometry().x() + padding_left;
            y = y_root - geometry().y() + padding_top;
        }
        if (dynamic_cast<KDecorationUnstable*>(decoration))
            // New API processes core events FIRST and only passes unused ones to the decoration
            return processDecorationButtonPress(button, state, x, y, x_root, y_root, true);
        else
            return false; // Don't eat old API decoration events
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

    if (button == Button1)
        com = active ? options->commandActiveTitlebar1() : options->commandInactiveTitlebar1();
    else if (button == Button2)
        com = active ? options->commandActiveTitlebar2() : options->commandInactiveTitlebar2();
    else if (button == Button3)
        com = active ? options->commandActiveTitlebar3() : options->commandInactiveTitlebar3();
    if (button == Button1
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
        kWarning(1212) << "processMousePressEvent()" ;
        return;
    }
    int button;
    switch(e->button()) {
    case Qt::LeftButton:
        button = Button1;
        break;
    case Qt::MidButton:
        button = Button2;
        break;
    case Qt::RightButton:
        button = Button3;
        break;
    default:
        return;
    }
    processDecorationButtonPress(button, e->buttons(), e->x(), e->y(), e->globalX(), e->globalY());
}

// return value matters only when filtering events before decoration gets them
bool Client::buttonReleaseEvent(Window w, int /*button*/, int state, int x, int y, int x_root, int y_root)
{
    if (w == decorationId() && !buttonDown)
        return false;
    if (w == wrapperId()) {
        XAllowEvents(display(), SyncPointer, CurrentTime);  //xTime());
        return true;
    }
    if (w != frameId() && w != decorationId() && w != inputId() && w != moveResizeGrabWindow())
        return true;
    x = this->x(); // translate from grab window to local coords
    y = this->y();
    if ((state & (Button1Mask & Button2Mask & Button3Mask)) == 0) {
        buttonDown = false;
        stopDelayedMoveResize();
        if (moveResizeMode) {
            finishMoveResize(false);
            // mouse position is still relative to old Client position, adjust it
            QPoint mousepos(x_root - x + padding_left, y_root - y + padding_top);
            mode = mousePosition(mousepos);
        } else if (workspace()->decorationSupportsTabbing())
            return false;
        updateCursor();
    }
    return true;
}

static bool was_motion = false;
static Time next_motion_time = CurrentTime;
// Check whole incoming X queue for MotionNotify events
// checking whole queue is done by always returning False in the predicate.
// If there are more MotionNotify events in the queue, all until the last
// one may be safely discarded (if a ButtonRelease event comes, a MotionNotify
// will be faked from it, so there's no need to check other events).
// This helps avoiding being overloaded by being flooded from many events
// from the XServer.
static Bool motion_predicate(Display*, XEvent* ev, XPointer)
{
    if (ev->type == MotionNotify) {
        was_motion = true;
        next_motion_time = ev->xmotion.time;  // for setting time
    }
    return False;
}

static bool waitingMotionEvent()
{
// The queue doesn't need to be checked until the X timestamp
// of processes events reaches the timestamp of the last suitable
// MotionNotify event in the queue.
    if (next_motion_time != CurrentTime
            && timestampCompare(xTime(), next_motion_time) < 0)
        return true;
    was_motion = false;
    XSync(display(), False);   // this helps to discard more MotionNotify events
    XEvent dummy;
    XCheckIfEvent(display(), &dummy, motion_predicate, NULL);
    return was_motion;
}

// Checks if the mouse cursor is near the edge of the screen and if so activates quick tiling or maximization
void Client::checkQuickTilingMaximizationZones(int xroot, int yroot)
{

    QuickTileMode mode = QuickTileNone;
    for (int i=0; i<QApplication::desktop()->screenCount(); ++i) {

        const QRect &area = QApplication::desktop()->screenGeometry(i);
        if (!area.contains(QPoint(xroot, yroot)))
            continue;

        if (options->electricBorderTiling()) {
        if (xroot <= area.x() + 20)
            mode |= QuickTileLeft;
        else if (xroot >= area.x() + area.width() - 20)
            mode |= QuickTileRight;
        }

        if (mode != QuickTileNone) {
            if (yroot <= area.y() + area.height() / 4)
                mode |= QuickTileTop;
            else if (yroot >= area.y() + area.height() - area.height() / 4)
                mode |= QuickTileBottom;
        } else if (options->electricBorderMaximize() && yroot <= area.y() + 5 && isMaximizable())
            mode = QuickTileMaximize;
        break; // no point in checking other screens to contain this... "point"...
    }
    setElectricBorderMode(mode);
    setElectricBorderMaximizing(mode != QuickTileNone);
}

// return value matters only when filtering events before decoration gets them
bool Client::motionNotifyEvent(Window w, int state, int x, int y, int x_root, int y_root)
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
        // reset the timestamp for the optimization, otherwise with long passivity
        // the option in waitingMotionEvent() may be always true
        next_motion_time = CurrentTime;
        return false;
    }
    if (w == moveResizeGrabWindow()) {
        x = this->x(); // translate from grab window to local coords
        y = this->y();
    }
    if (!waitingMotionEvent()) {
        handleMoveResize(x, y, x_root, y_root);
        if (isMove() && isResizable())
            checkQuickTilingMaximizationZones(x_root, y_root);
    }
    return true;
}

void Client::focusInEvent(XFocusInEvent* e)
{
    if (e->window != window())
        return; // only window gets focus
    if (e->mode == NotifyUngrab)
        return; // we don't care
    if (e->detail == NotifyPointer)
        return;  // we don't care
    if (!isShown(false) || !isOnCurrentDesktop())    // we unmapped it, but it got focus meanwhile ->
        return;            // activateNextClient() already transferred focus elsewhere
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
// Therefore, the events queue is checked, whether it contains the matching
// FocusIn event, and if yes, deactivation of the previous client will
// be skipped, as activation of the new one will automatically deactivate
// previously active client.
static bool follows_focusin = false;
static bool follows_focusin_failed = false;
static Bool predicate_follows_focusin(Display*, XEvent* e, XPointer arg)
{
    if (follows_focusin || follows_focusin_failed)
        return False;
    Client* c = (Client*) arg;
    if (e->type == FocusIn && c->workspace()->findClient(WindowMatchPredicate(e->xfocus.window))) {
        // found FocusIn
        follows_focusin = true;
        return False;
    }
    // events that may be in the queue before the FocusIn event that's being
    // searched for
    if (e->type == FocusIn || e->type == FocusOut || e->type == KeymapNotify)
        return False;
    follows_focusin_failed = true; // a different event - stop search
    return False;
}

static bool check_follows_focusin(Client* c)
{
    follows_focusin = follows_focusin_failed = false;
    XEvent dummy;
    // XCheckIfEvent() is used to make the search non-blocking, the predicate
    // always returns False, so nothing is removed from the events queue.
    // XPeekIfEvent() would block.
    XCheckIfEvent(display(), &dummy, predicate_follows_focusin, (XPointer)c);
    return follows_focusin;
}


void Client::focusOutEvent(XFocusOutEvent* e)
{
    if (e->window != window())
        return; // only window gets focus
    if (e->mode == NotifyGrab)
        return; // we don't care
    if (isShade())
        return; // here neither
    if (e->detail != NotifyNonlinear
            && e->detail != NotifyNonlinearVirtual)
        // SELI check all this
        return; // hack for motif apps like netscape
    if (QApplication::activePopupWidget())
        return;
    if (!check_follows_focusin(this))
        setActive(false);
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
        QCursor::setPos(geometry().center());
        performMouseCommand(Options::MouseUnrestrictedMove, geometry().center());
    } else if (direction == NET::KeyboardSize) {
        // ignore mouse coordinates given in the message, mouse position is used by the resizing algorithm
        QCursor::setPos(geometry().bottomRight());
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
    QCursor::setPos(pos);
}

#ifdef HAVE_XSYNC
void Client::syncEvent(XSyncAlarmNotifyEvent* e)
{
    if (e->alarm == syncRequest.alarm && XSyncValueEqual(e->counter_value, syncRequest.value)) {
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
#endif

// ****************************************
// Unmanaged
// ****************************************

bool Unmanaged::windowEvent(XEvent* e)
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
    switch(e->type) {
    case UnmapNotify:
        workspace()->updateFocusMousePosition(QCursor::pos());
        unmapNotifyEvent(&e->xunmap);
        break;
    case MapNotify:
        mapNotifyEvent(&e->xmap);
        break;
    case ConfigureNotify:
        configureNotifyEvent(&e->xconfigure);
        break;
    case PropertyNotify:
        propertyNotifyEvent(&e->xproperty);
        break;
    default: {
        if (e->type == Extensions::shapeNotifyEvent()) {
            detectShape(window());
            addRepaintFull();
            addWorkspaceRepaint(geometry());  // in case shape change removes part of this window
            emit geometryShapeChanged(this, geometry());
        }
        if (e->type == Extensions::damageNotifyEvent())
            damageNotifyEvent(reinterpret_cast< XDamageNotifyEvent* >(e));
        break;
    }
    }
    return false; // don't eat events, even our own unmanaged widgets are tracked
}

void Unmanaged::mapNotifyEvent(XMapEvent*)
{
}

void Unmanaged::unmapNotifyEvent(XUnmapEvent*)
{
    release();
}

void Unmanaged::configureNotifyEvent(XConfigureEvent* e)
{
    if (effects)
        static_cast<EffectsHandlerImpl*>(effects)->checkInputWindowStacking(); // keep them on top
    QRect newgeom(e->x, e->y, e->width, e->height);
    if (newgeom != geom) {
        addWorkspaceRepaint(visibleRect());  // damage old area
        QRect old = geom;
        geom = newgeom;
        addRepaintFull();
        if (old.size() != geom.size())
            discardWindowPixmap();
        emit geometryShapeChanged(this, old);
    }
}

// ****************************************
// Toplevel
// ****************************************

void Toplevel::propertyNotifyEvent(XPropertyEvent* e)
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
        break;
    }
    emit propertyNotify(this, e->atom);
}

// ****************************************
// Group
// ****************************************

bool Group::groupEvent(XEvent* e)
{
    unsigned long dirty[ 2 ];
    leader_info->event(e, dirty, 2);   // pass through the NET stuff
    if ((dirty[ WinInfo::PROTOCOLS2 ] & NET::WM2StartupId) != 0)
        startupIdChanged();
    return false;
}


} // namespace
