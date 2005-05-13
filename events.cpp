/*****************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 1999, 2000 Matthias Ettrich <ettrich@kde.org>
Copyright (C) 2003 Lubos Lunak <l.lunak@kde.org>

You can Freely distribute this program under the GNU General Public
License. See the file "COPYING" for the exact licensing terms.
******************************************************************/

/*

 This file contains things relevant to handling incoming events.

*/

#include "client.h"
#include "workspace.h"
#include "atoms.h"
#include "tabbox.h"
#include "group.h"
#include "rules.h"

#include <qwhatsthis.h>
#include <kkeynative.h>
#include <qapplication.h>

#include <X11/extensions/shape.h>
#include <X11/Xatom.h>

extern Time qt_x_time;
extern Atom qt_window_role;

namespace KWinInternal
{

// ****************************************
// WinInfo
// ****************************************

WinInfo::WinInfo( Client * c, Display * display, Window window,
    Window rwin, const unsigned long pr[], int pr_size )
    : NETWinInfo( display, window, rwin, pr, pr_size, NET::WindowManager ), m_client( c )
    {
    }

void WinInfo::changeDesktop(int desktop)
    {
    m_client->workspace()->sendClientToDesktop( m_client, desktop, true );
    }

void WinInfo::changeState( unsigned long state, unsigned long mask )
    {
    mask &= ~NET::Sticky; // KWin doesn't support large desktops, ignore
    mask &= ~NET::Hidden; // clients are not allowed to change this directly
    state &= mask; // for safety, clear all other bits

    if(( mask & NET::FullScreen ) != 0 && ( state & NET::FullScreen ) == 0 )
        m_client->setFullScreen( false, false );
    if ( (mask & NET::Max) == NET::Max )
        m_client->setMaximize( state & NET::MaxVert, state & NET::MaxHoriz );
    else if ( mask & NET::MaxVert )
        m_client->setMaximize( state & NET::MaxVert, m_client->maximizeMode() & Client::MaximizeHorizontal );
    else if ( mask & NET::MaxHoriz )
        m_client->setMaximize( m_client->maximizeMode() & Client::MaximizeVertical, state & NET::MaxHoriz );

    if ( mask & NET::Shaded )
        m_client->setShade( state & NET::Shaded ? ShadeNormal : ShadeNone );
    if ( mask & NET::KeepAbove)
        m_client->setKeepAbove( (state & NET::KeepAbove) != 0 );
    if ( mask & NET::KeepBelow)
        m_client->setKeepBelow( (state & NET::KeepBelow) != 0 );
    if( mask & NET::SkipTaskbar )
        m_client->setSkipTaskbar( ( state & NET::SkipTaskbar ) != 0, true );
    if( mask & NET::SkipPager )
        m_client->setSkipPager( ( state & NET::SkipPager ) != 0 );
    if( mask & NET::DemandsAttention )
        m_client->demandAttention(( state & NET::DemandsAttention ) != 0 );
    if( mask & NET::Modal )
        m_client->setModal( ( state & NET::Modal ) != 0 );
    // unsetting fullscreen first, setting it last (because e.g. maximize works only for !isFullScreen() )
    if(( mask & NET::FullScreen ) != 0 && ( state & NET::FullScreen ) != 0 )
        m_client->setFullScreen( true, false );
    }


// ****************************************
// RootInfo
// ****************************************

RootInfo::RootInfo( Workspace* ws, Display *dpy, Window w, const char *name, unsigned long pr[], int pr_num, int scr )
    : NETRootInfo4( dpy, w, name, pr, pr_num, scr )
    {
    workspace = ws;
    }

void RootInfo::changeNumberOfDesktops(int n)
    {
    workspace->setNumberOfDesktops( n );
    }

void RootInfo::changeCurrentDesktop(int d)
    {
    workspace->setCurrentDesktop( d );
    }

void RootInfo::changeActiveWindow( Window w, NET::RequestSource src, Time timestamp, Window active_window )
    {
    if( Client* c = workspace->findClient( WindowMatchPredicate( w )))
        {
        if( timestamp == CurrentTime )
            timestamp = c->userTime();
        if( src != NET::FromApplication && src != FromTool )
            src = NET::FromTool;
        if( src == NET::FromTool )
            workspace->activateClient( c, true ); // force
        else // NET::FromApplication
            {
            Client* c2;
            if( workspace->allowClientActivation( c, timestamp ))
                workspace->activateClient( c );
            // if activation of the requestor's window would be allowed, allow activation too
            else if( active_window != None
                && ( c2 = workspace->findClient( WindowMatchPredicate( active_window ))) != NULL
                && workspace->allowClientActivation( c2,
                    timestampCompare( timestamp, c2->userTime() > 0 ? timestamp : c2->userTime())))
                workspace->activateClient( c );
            else
                c->demandAttention();
            }
        }
    }

void RootInfo::restackWindow( Window w, RequestSource src, Window above, int detail, Time timestamp )
    {
    if( Client* c = workspace->findClient( WindowMatchPredicate( w )))
        {
        if( timestamp == CurrentTime )
            timestamp = c->userTime();
        if( src != NET::FromApplication && src != FromTool )
            src = NET::FromTool;
        c->restackWindow( above, detail, src, timestamp, true );
        }
    }

void RootInfo::gotTakeActivity( Window w, Time timestamp, long flags )
    {
    if( Client* c = workspace->findClient( WindowMatchPredicate( w )))
        workspace->handleTakeActivity( c, timestamp, flags );
    }

void RootInfo::closeWindow(Window w)
    {
    Client* c = workspace->findClient( WindowMatchPredicate( w ));
    if ( c )
        c->closeWindow();
    }

void RootInfo::moveResize(Window w, int x_root, int y_root, unsigned long direction)
    {
    Client* c = workspace->findClient( WindowMatchPredicate( w ));
    if ( c )
        {
        updateXTime(); // otherwise grabbing may have old timestamp - this message should include timestamp
        c->NETMoveResize( x_root, y_root, (Direction)direction);
        }
    }

void RootInfo::moveResizeWindow(Window w, int flags, int x, int y, int width, int height )
    {
    Client* c = workspace->findClient( WindowMatchPredicate( w ));
    if ( c )
        c->NETMoveResizeWindow( flags, x, y, width, height );
    }

void RootInfo::gotPing( Window w, Time timestamp )
    {
    if( Client* c = workspace->findClient( WindowMatchPredicate( w )))
        c->gotPing( timestamp );
    }

void RootInfo::changeShowingDesktop( bool showing )
    {
    workspace->setShowingDesktop( showing );
    }

// ****************************************
// Workspace
// ****************************************

/*!
  Handles workspace specific XEvents
 */
bool Workspace::workspaceEvent( XEvent * e )
    {
    if ( mouse_emulation && (e->type == ButtonPress || e->type == ButtonRelease ) ) 
        {
        mouse_emulation = FALSE;
        XUngrabKeyboard( qt_xdisplay(), qt_x_time );
        }

    if ( e->type == PropertyNotify || e->type == ClientMessage ) 
        {
        if ( netCheck( e ) )
            return TRUE;
        }

    // events that should be handled before Clients can get them
    switch (e->type) 
        {
        case ButtonPress:
        case ButtonRelease:
            was_user_interaction = true;
        // fallthrough
        case MotionNotify:
            if ( tab_grab || control_grab )
                {
                tab_box->handleMouseEvent( e );
                return TRUE;
                }
            break;
        case KeyPress:
            {
            was_user_interaction = true;
            KKeyNative keyX( (XEvent*)e );
            uint keyQt = keyX.keyCodeQt();
            kdDebug(125) << "Workspace::keyPress( " << keyX.key().toString() << " )" << endl;
            if (movingClient)
                {
                movingClient->keyPressEvent(keyQt);
                return true;
                }
            if( tab_grab || control_grab )
                {
                tabBoxKeyPress( keyX );
                return true;
                }
            break;
            }
        case KeyRelease:
            was_user_interaction = true;
            if( tab_grab || control_grab )
                {
                tabBoxKeyRelease( e->xkey );
                return true;
                }
            break;
        };

    if( Client* c = findClient( WindowMatchPredicate( e->xany.window )))
        {
        if( c->windowEvent( e ))
            return true;
        }
    else if( Client* c = findClient( WrapperIdMatchPredicate( e->xany.window )))
        {
        if( c->windowEvent( e ))
            return true;
        }
    else if( Client* c = findClient( FrameIdMatchPredicate( e->xany.window )))
        {
        if( c->windowEvent( e ))
            return true;
        }
    else
        {
        Window special = findSpecialEventWindow( e );
        if( special != None )
            if( Client* c = findClient( WindowMatchPredicate( special )))
                {
                if( c->windowEvent( e ))
                    return true;
                }
        }
    if( movingClient != NULL && movingClient->moveResizeGrabWindow() == e->xany.window
        && ( e->type == MotionNotify || e->type == ButtonPress || e->type == ButtonRelease ))
        {
        if( movingClient->windowEvent( e ))
            return true;
        }

    switch (e->type) 
        {
        case CreateNotify:
            if ( e->xcreatewindow.parent == root &&
                 !QWidget::find( e->xcreatewindow.window) &&
                 !e->xcreatewindow.override_redirect )
            {
        // see comments for allowClientActivation()
            XChangeProperty(qt_xdisplay(), e->xcreatewindow.window,
                            atoms->kde_net_wm_user_creation_time, XA_CARDINAL,
                            32, PropModeReplace, (unsigned char *)&qt_x_time, 1);
            }
        break;

    case UnmapNotify:
            {
        // check for system tray windows
            if ( removeSystemTrayWin( e->xunmap.window, true ) ) 
                {
	    // If the system tray gets destroyed, the system tray
	    // icons automatically get unmapped, reparented and mapped
	    // again to the closest non-client ancestor due to
	    // QXEmbed's SaveSet feature. Unfortunatly with kicker
	    // this closest ancestor is not the root window, but our
	    // decoration, so we reparent explicitely back to the root
	    // window.
                XEvent ev;
                WId w = e->xunmap.window;
                if ( XCheckTypedWindowEvent (qt_xdisplay(), w,
                                             ReparentNotify, &ev) )
                    {
                    if ( ev.xreparent.parent != root ) 
                        {
                        XReparentWindow( qt_xdisplay(), w, root, 0, 0 );
                        addSystemTrayWin( w );
                        }
                    }
                return TRUE;
                }

            return ( e->xunmap.event != e->xunmap.window ); // hide wm typical event from Qt
            }
        case MapNotify:

            return ( e->xmap.event != e->xmap.window ); // hide wm typical event from Qt

        case ReparentNotify:
            {
        //do not confuse Qt with these events. After all, _we_ are the
        //window manager who does the reparenting.
            return TRUE;
            }
        case DestroyNotify:
            {
            if ( removeSystemTrayWin( e->xdestroywindow.window, false ) )
                return TRUE;
            return false;
            }
        case MapRequest:
            {
            updateXTime();

            // e->xmaprequest.window is different from e->xany.window
            // TODO this shouldn't be necessary now
            Client* c = findClient( WindowMatchPredicate( e->xmaprequest.window ));
            if ( !c ) 
                {
// don't check for the parent being the root window, this breaks when some app unmaps
// a window, changes something and immediately maps it back, without giving KWin
// a chance to reparent it back to root
// since KWin can get MapRequest only for root window children and
// children of WindowWrapper (=clients), the check is AFAIK useless anyway
// Note: Now the save-set support in Client::mapRequestEvent() actually requires that
// this code doesn't check the parent to be root.
//            if ( e->xmaprequest.parent == root ) { //###TODO store previously destroyed client ids
                if ( addSystemTrayWin( e->xmaprequest.window ) )
                    return TRUE;
                c = createClient( e->xmaprequest.window, false );
                if ( c != NULL && root != qt_xrootwin() ) 
                    { // TODO what is this?
                    // TODO may use QWidget::create
                    XReparentWindow( qt_xdisplay(), c->frameId(), root, 0, 0 );
                    }
                if( c == NULL ) // refused to manage, simply map it (most probably override redirect)
                    XMapRaised( qt_xdisplay(), e->xmaprequest.window );
                return true;
                }
            if ( c ) 
                {
                c->windowEvent( e );
                if ( !c->wantsTabFocus())
                    focus_chain.remove( c );  // TODO move focus_chain changes to functions
                return true;
                }
            break;
            }
        case EnterNotify:
            {
            if ( QWhatsThis::inWhatsThisMode() )
                {
                QWidget* w = QWidget::find( e->xcrossing.window );
                if ( w )
                    QWhatsThis::leaveWhatsThisMode();
                }
            if( electricBorder(e))
                return true;
            break;
            }
        case LeaveNotify:
            {
            if ( !QWhatsThis::inWhatsThisMode() )
                break;
            // TODO is this cliente ever found, given that client events are searched above?
            Client* c = findClient( FrameIdMatchPredicate( e->xcrossing.window ));
            if ( c && e->xcrossing.detail != NotifyInferior )
                QWhatsThis::leaveWhatsThisMode();
            break;
            }
        case ConfigureRequest:
            {
            if ( e->xconfigurerequest.parent == root ) 
                {
                XWindowChanges wc;
                unsigned int value_mask = 0;
                wc.border_width = 0;
                wc.x = e->xconfigurerequest.x;
                wc.y = e->xconfigurerequest.y;
                wc.width = e->xconfigurerequest.width;
                wc.height = e->xconfigurerequest.height;
                wc.sibling = None;
                wc.stack_mode = Above;
                value_mask = e->xconfigurerequest.value_mask | CWBorderWidth;
                XConfigureWindow( qt_xdisplay(), e->xconfigurerequest.window, value_mask, &wc );
                return true;
                }
            break;
            }
        case KeyPress:
            if ( mouse_emulation )
                return keyPressMouseEmulation( e->xkey );
            break;
        case KeyRelease:
            if ( mouse_emulation )
                return FALSE;
            break;
        case FocusIn:
            if( e->xfocus.window == rootWin()
                && ( e->xfocus.detail == NotifyDetailNone || e->xfocus.detail == NotifyPointerRoot ))
                {
                updateXTime(); // focusToNull() uses qt_x_time, which is old now (FocusIn has no timestamp)
                Window focus;
                int revert;
                XGetInputFocus( qt_xdisplay(), &focus, &revert );
                if( focus == None || focus == PointerRoot )
                    {
                    //kdWarning( 1212 ) << "X focus set to None/PointerRoot, reseting focus" << endl;
                    Client *c = mostRecentlyActivatedClient();
                    if( c != NULL )
                        requestFocus( c, true );
                    else if( activateNextClient( NULL ))
                        ; // ok, activated
                    else
                        focusToNull();
                    }
                }
            // fall through
        case FocusOut:
            return true; // always eat these, they would tell Qt that KWin is the active app
        case ClientMessage:
            if( electricBorder( e ))
                return true;
            break;
        default:
            break;
        }
    return FALSE;
    }

// Some events don't have the actual window which caused the event
// as e->xany.window (e.g. ConfigureRequest), but as some other
// field in the XEvent structure.
Window Workspace::findSpecialEventWindow( XEvent* e )
    {
    switch( e->type )
        {
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

/*!
  Handles client messages sent to the workspace
 */
bool Workspace::netCheck( XEvent* e )
    {
    unsigned int dirty = rootInfo->event( e );

    if ( dirty & NET::DesktopNames )
        saveDesktopSettings();

    return dirty != 0;
    }


// ****************************************
// Client
// ****************************************

/*!
  General handler for XEvents concerning the client window
 */
bool Client::windowEvent( XEvent* e )
    {
    if( e->xany.window == window()) // avoid doing stuff on frame or wrapper
        {
        unsigned long dirty[ 2 ];
        info->event( e, dirty, 2 ); // pass through the NET stuff

        if ( ( dirty[ WinInfo::PROTOCOLS ] & NET::WMName ) != 0 )
            fetchName();
        if ( ( dirty[ WinInfo::PROTOCOLS ] & NET::WMIconName ) != 0 )
            fetchIconicName();
        if ( ( dirty[ WinInfo::PROTOCOLS ] & NET::WMStrut ) != 0
            || ( dirty[ WinInfo::PROTOCOLS2 ] & NET::WM2ExtendedStrut ) != 0 )
            {
            if( isTopMenu())  // the fallback mode of KMenuBar may alter the strut
                checkWorkspacePosition();  // restore it
            workspace()->updateClientArea();
            }
        if ( ( dirty[ WinInfo::PROTOCOLS ] & NET::WMIcon) != 0 )
            getIcons();
        // Note there's a difference between userTime() and info->userTime()
        // info->userTime() is the value of the property, userTime() also includes
        // updates of the time done by KWin (ButtonPress on windowrapper etc.).
        if(( dirty[ WinInfo::PROTOCOLS2 ] & NET::WM2UserTime ) != 0 )
            {
            workspace()->setWasUserInteraction();
            updateUserTime( info->userTime());
            }
        if(( dirty[ WinInfo::PROTOCOLS2 ] & NET::WM2StartupId ) != 0 )
            startupIdChanged();
        }

// TODO move all focus handling stuff to separate file?
    switch (e->type) 
        {
        case UnmapNotify:
            unmapNotifyEvent( &e->xunmap );
            break;
        case DestroyNotify:
            destroyNotifyEvent( &e->xdestroywindow );
            break;
        case MapRequest:
            // this one may pass the event to workspace
            return mapRequestEvent( &e->xmaprequest );
        case ConfigureRequest:
            configureRequestEvent( &e->xconfigurerequest );
            break;
        case PropertyNotify:
            propertyNotifyEvent( &e->xproperty );
            break;
        case KeyPress:
            updateUserTime();
            workspace()->setWasUserInteraction();
            break;
        case ButtonPress:
            updateUserTime();
            workspace()->setWasUserInteraction();
            buttonPressEvent( e->xbutton.window, e->xbutton.button, e->xbutton.state,
                e->xbutton.x, e->xbutton.y, e->xbutton.x_root, e->xbutton.y_root );
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
            buttonReleaseEvent( e->xbutton.window, e->xbutton.button, e->xbutton.state,
                e->xbutton.x, e->xbutton.y, e->xbutton.x_root, e->xbutton.y_root );
            break;
        case MotionNotify:
            motionNotifyEvent( e->xmotion.window, e->xmotion.state,
                e->xmotion.x, e->xmotion.y, e->xmotion.x_root, e->xmotion.y_root );
            break;
        case EnterNotify:
            enterNotifyEvent( &e->xcrossing );
            // MotionNotify is guaranteed to be generated only if the mouse
            // move start and ends in the window; for cases when it only
            // starts or only ends there, Enter/LeaveNotify are generated.
            // Fake a MotionEvent in such cases to make handle of mouse
            // events simpler (Qt does that too).
            motionNotifyEvent( e->xcrossing.window, e->xcrossing.state,
                e->xcrossing.x, e->xcrossing.y, e->xcrossing.x_root, e->xcrossing.y_root );
            break;
        case LeaveNotify:
            motionNotifyEvent( e->xcrossing.window, e->xcrossing.state,
                e->xcrossing.x, e->xcrossing.y, e->xcrossing.x_root, e->xcrossing.y_root );
            leaveNotifyEvent( &e->xcrossing );
            break;
        case FocusIn:
            focusInEvent( &e->xfocus );
            break;
        case FocusOut:
            focusOutEvent( &e->xfocus );
            break;
        case ReparentNotify:
            break;
        case ClientMessage:
            clientMessageEvent( &e->xclient );
            break;
        case ColormapChangeMask:
            if( e->xany.window == window())
            {
            cmap = e->xcolormap.colormap;
            if ( isActive() )
                workspace()->updateColormap();
            }
            break;
        case VisibilityNotify:
            visibilityNotifyEvent( &e->xvisibility );
            break;
        default:
            if( e->xany.window == window())
            {
            if( e->type == Shape::shapeEvent() )
                {
                is_shape = Shape::hasShape( window()); // workaround for #19644
                updateShape();
                }
            }
            break;
        }
    return true; // eat all events
    }

/*!
  Handles map requests of the client window
 */
bool Client::mapRequestEvent( XMapRequestEvent* e )
    {
    if( e->window != window())
        {
        // Special support for the save-set feature, which is a bit broken.
        // If there's a window from one client embedded in another one,
        // e.g. using XEMBED, and the embedder suddenly looses its X connection,
        // save-set will reparent the embedded window to its closest ancestor
        // that will remains. Unfortunately, with reparenting window managers,
        // this is not the root window, but the frame (or in KWin's case,
        // it's the wrapper for the client window). In this case,
        // the wrapper will get ReparentNotify for a window it won't know,
        // which will be ignored, and then it gets MapRequest, as save-set
        // always maps. Returning true here means that Workspace::workspaceEvent()
        // will handle this MapRequest and manage this window (i.e. act as if
        // it was reparented to root window).
        if( e->parent == wrapperId())
            return false;
        return true; // no messing with frame etc.
        }
    if( isTopMenu() && workspace()->managingTopMenus())
        return true; // kwin controls these
    switch ( mappingState() )
        {
        case WithdrawnState:
            assert( false ); // WMs are not supposed to manage clients in Withdrawn state,
//        manage();      // after initial mapping manage() is called from createClient()
            break;
        case IconicState:
	// also copied in clientMessage()
            if( isMinimized())
                unminimize();
            if( isShade())
                setShade( ShadeNone );
            if( !isOnCurrentDesktop())
                {
                if( workspace()->allowClientActivation( this ))
                    workspace()->activateClient( this );
                else
                    demandAttention();
                }
            break;
        case NormalState:
	    // TODO fake MapNotify?
            break;
        }
    return true;
    }

/*!
  Handles unmap notify events of the client window
 */
void Client::unmapNotifyEvent( XUnmapEvent* e )
    {
    if( e->window != window())
        return;
    if( e->event != wrapperId())
        { // most probably event from root window when initially reparenting
        bool ignore = true;
        if( e->event == workspace()->rootWin() && e->send_event )
            ignore = false; // XWithdrawWindow()
        if( ignore )
            return;
        }
    switch( mappingState())
        {
        case IconicState:
            releaseWindow();
          return;
        case NormalState:
            // maybe we will be destroyed soon. Check this first.
            XEvent ev;
            if( XCheckTypedWindowEvent (qt_xdisplay(), window(),
                DestroyNotify, &ev) ) // TODO I don't like this much
                {
                destroyClient(); // deletes this
                return;
                }
            releaseWindow();
          break;
    default:
        assert( false );
        }
    }

void Client::destroyNotifyEvent( XDestroyWindowEvent* e )
    {
    if( e->window != window())
        return;
    destroyClient();
    }
    
    
bool         blockAnimation = FALSE;

/*!
   Handles client messages for the client window
*/
void Client::clientMessageEvent( XClientMessageEvent* e )
    {
    if( e->window != window())
        return; // ignore frame/wrapper
    // WM_STATE
    if ( e->message_type == atoms->kde_wm_change_state )
        {
        if( isTopMenu() && workspace()->managingTopMenus())
            return; // kwin controls these
        if( e->data.l[ 1 ] )
            blockAnimation = true;
        if( e->data.l[ 0 ] == IconicState )
            minimize();
        else if( e->data.l[ 0 ] == NormalState )
            { // copied from mapRequest()
            if( isMinimized())
                unminimize();
            if( isShade())
                setShade( ShadeNone );
            if( !isOnCurrentDesktop())
                {
                if( workspace()->allowClientActivation( this ))
                    workspace()->activateClient( this );
                else
                    demandAttention();
                }
            }
        blockAnimation = false;
        }
    else if ( e->message_type == atoms->wm_change_state)
        {
        if( isTopMenu() && workspace()->managingTopMenus())
            return; // kwin controls these
        if ( e->data.l[0] == IconicState )
            minimize();
        return;
        }
    }


/*!
  Handles configure  requests of the client window
 */
void Client::configureRequestEvent( XConfigureRequestEvent* e )
    {
    if( e->window != window())
        return; // ignore frame/wrapper
    if ( isResize() || isMove())
        return; // we have better things to do right now

    if( fullscreen_mode == FullScreenNormal ) // refuse resizing of fullscreen windows
        { // but allow resizing fullscreen hacks in order to let them cancel fullscreen mode
        sendSyntheticConfigureNotify();
        return;
        }
    if( isSplash() // no manipulations with splashscreens either
        || isTopMenu()) // topmenus neither
        {
        sendSyntheticConfigureNotify();
        return;
        }

    if ( e->value_mask & CWBorderWidth ) 
        {
        // first, get rid of a window border
        XWindowChanges wc;
        unsigned int value_mask = 0;

        wc.border_width = 0;
        value_mask = CWBorderWidth;
        XConfigureWindow( qt_xdisplay(), window(), value_mask, & wc );
        }

    if( e->value_mask & ( CWX | CWY | CWHeight | CWWidth ))
        configureRequest( e->value_mask, e->x, e->y, e->width, e->height, 0, false );

    if ( e->value_mask & CWStackMode )
        restackWindow( e->above, e->detail, NET::FromApplication, userTime(), false );

    // TODO sending a synthetic configure notify always is fine, even in cases where
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
void Client::propertyNotifyEvent( XPropertyEvent* e )
    {
    if( e->window != window())
        return; // ignore frame/wrapper
    switch ( e->atom ) 
        {
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
            if ( e->atom == atoms->wm_protocols )
                getWindowProtocols();
            else if (e->atom == atoms->wm_client_leader )
                getWmClientLeader();
            else if( e->atom == qt_window_role )
                window_role = staticWindowRole( window());
            else if( e->atom == atoms->motif_wm_hints )
                getMotifHints();
            break;
        }
    }


void Client::enterNotifyEvent( XCrossingEvent* e )
    {
    if( e->window != frameId())
        return; // care only about entering the whole frame
    if( e->mode == NotifyNormal ||
         ( !options->focusPolicyIsReasonable() &&
             e->mode == NotifyUngrab ) ) 
        {

        if (options->shadeHover && isShade()) 
            {
            delete shadeHoverTimer;
            shadeHoverTimer = new QTimer( this );
            connect( shadeHoverTimer, SIGNAL( timeout() ), this, SLOT( shadeHover() ));
            shadeHoverTimer->start( options->shadeHoverInterval, TRUE );
            }

        if ( options->focusPolicy == Options::ClickToFocus )
            return;

        if ( options->autoRaise && !isDesktop() &&
             !isDock() && !isTopMenu() && workspace()->focusChangeEnabled() &&
             workspace()->topClientOnDesktop( workspace()->currentDesktop()) != this ) 
            {
            delete autoRaiseTimer;
            autoRaiseTimer = new QTimer( this );
            connect( autoRaiseTimer, SIGNAL( timeout() ), this, SLOT( autoRaise() ) );
            autoRaiseTimer->start( options->autoRaiseInterval, TRUE  );
            }

        if ( options->focusPolicy !=  Options::FocusStrictlyUnderMouse && ( isDesktop() || isDock() || isTopMenu() ) )
            return;
        if ( options->delayFocus )
            workspace()->requestDelayFocus( this );
        else
            workspace()->requestFocus( this );

        return;
        }
    }

void Client::leaveNotifyEvent( XCrossingEvent* e )
    {
    if( e->window != frameId())
        return; // care only about leaving the whole frame
    if ( e->mode == NotifyNormal ) 
        {
        if ( !buttonDown ) 
            {
            mode = PositionCenter;
            setCursor( arrowCursor );
            }
        bool lostMouse = !rect().contains( QPoint( e->x, e->y ) );
        // 'lostMouse' wouldn't work with e.g. B2 or Keramik, which have non-rectangular decorations
        // (i.e. the LeaveNotify event comes before leaving the rect and no LeaveNotify event
        // comes after leaving the rect) - so lets check if the pointer is really outside the window

        // TODO this still sucks if a window appears above this one - it should lose the mouse
        // if this window is another client, but not if it's a popup ... maybe after KDE3.1 :(
        // (repeat after me 'AARGHL!')
        if ( !lostMouse && e->detail != NotifyInferior ) 
            {
            int d1, d2, d3, d4;
            unsigned int d5;
            Window w, child;
            if( XQueryPointer( qt_xdisplay(), frameId(), &w, &child, &d1, &d2, &d3, &d4, &d5 ) == False
                || child == None )
                lostMouse = true; // really lost the mouse
            }
        if ( lostMouse ) 
            {
            cancelAutoRaise();
            workspace()->cancelDelayFocus();
            cancelShadeHover();
            if ( shade_mode == ShadeHover && !moveResizeMode && !buttonDown )
               setShade( ShadeNormal );
            }
        if ( options->focusPolicy == Options::FocusStrictlyUnderMouse )
            if ( isActive() && lostMouse )
                workspace()->requestFocus( 0 ) ;
        return;
        }
    }

#define XCapL KKeyNative::modXLock()
#define XNumL KKeyNative::modXNumLock()
#define XScrL KKeyNative::modXScrollLock()
void Client::grabButton( int modifier )
    {
    unsigned int mods[ 8 ] = 
        {
        0, XCapL, XNumL, XNumL | XCapL,
        XScrL, XScrL | XCapL,
        XScrL | XNumL, XScrL | XNumL | XCapL
        };
    for( int i = 0;
         i < 8;
         ++i )
        XGrabButton( qt_xdisplay(), AnyButton,
            modifier | mods[ i ],
            wrapperId(),  FALSE, ButtonPressMask,
            GrabModeSync, GrabModeAsync, None, None );
    }

void Client::ungrabButton( int modifier )
    {
    unsigned int mods[ 8 ] = 
        {
        0, XCapL, XNumL, XNumL | XCapL,
        XScrL, XScrL | XCapL,
        XScrL | XNumL, XScrL | XNumL | XCapL
        };
    for( int i = 0;
         i < 8;
         ++i )
        XUngrabButton( qt_xdisplay(), AnyButton,
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
    {                   // see Workspace::establishTabBoxGrab()
    if( isActive() && !workspace()->forcedGlobalMouseGrab())
        {
        // remove the grab for no modifiers only if the window
        // is unobscured or if the user doesn't want click raise
        if( !options->clickRaise || not_obscured )
            ungrabButton( None );
        else
            grabButton( None );
        ungrabButton( ShiftMask );
        ungrabButton( ControlMask );
        ungrabButton( ControlMask | ShiftMask );
        }
    else
        {
        XUngrabButton( qt_xdisplay(), AnyButton, AnyModifier, wrapperId());
        // simply grab all modifier combinations
        XGrabButton(qt_xdisplay(), AnyButton, AnyModifier, wrapperId(), FALSE,
            ButtonPressMask,
            GrabModeSync, GrabModeAsync,
            None, None );
        }
    }

int qtToX11Button( Qt::ButtonState button )
    {
    if( button == Qt::LeftButton )
        return Button1;
    else if( button == Qt::MidButton )
        return Button2;
    else if( button == Qt::RightButton )
        return Button3;
    return AnyButton;
    }
    
int qtToX11State( Qt::ButtonState state )
    {
    int ret = 0;
    if( state & Qt::LeftButton )
        ret |= Button1Mask;
    if( state & Qt::MidButton )
        ret |= Button2Mask;
    if( state & Qt::RightButton )
        ret |= Button3Mask;
    if( state & Qt::ShiftButton )
        ret |= ShiftMask;
    if( state & Qt::ControlButton )
        ret |= ControlMask;
    if( state & Qt::AltButton )
        ret |= KKeyNative::modX(KKey::ALT);
    if( state & Qt::MetaButton )
        ret |= KKeyNative::modX(KKey::WIN);
    return ret;
    }

// Qt propagates mouse events up the widget hierachy, which means events
// for the decoration window cannot be (easily) intercepted as X11 events
bool Client::eventFilter( QObject* o, QEvent* e )
    {
    if( decoration == NULL
        || o != decoration->widget())
        return false;
    if( e->type() == QEvent::MouseButtonPress )
        {
        QMouseEvent* ev = static_cast< QMouseEvent* >( e );
        return buttonPressEvent( decorationId(), qtToX11Button( ev->button()), qtToX11State( ev->state()),
            ev->x(), ev->y(), ev->globalX(), ev->globalY() );
        }
    if( e->type() == QEvent::MouseButtonRelease )
        {
        QMouseEvent* ev = static_cast< QMouseEvent* >( e );
        return buttonReleaseEvent( decorationId(), qtToX11Button( ev->button()), qtToX11State( ev->state()),
            ev->x(), ev->y(), ev->globalX(), ev->globalY() );
        }
    if( e->type() == QEvent::MouseMove ) // FRAME i fake z enter/leave?
        {
        QMouseEvent* ev = static_cast< QMouseEvent* >( e );
        return motionNotifyEvent( decorationId(), qtToX11State( ev->state()),
            ev->x(), ev->y(), ev->globalX(), ev->globalY() );
        }
    if( e->type() == QEvent::Wheel )
        {
        QWheelEvent* ev = static_cast< QWheelEvent* >( e );
        bool r = buttonPressEvent( decorationId(), ev->delta() > 0 ? Button4 : Button5, qtToX11State( ev->state()),
            ev->x(), ev->y(), ev->globalX(), ev->globalY() );
        r = r || buttonReleaseEvent( decorationId(), ev->delta() > 0 ? Button4 : Button5, qtToX11State( ev->state()),
            ev->x(), ev->y(), ev->globalX(), ev->globalY() );
        return r;
        }
    if( e->type() == QEvent::Resize )
        {
        QResizeEvent* ev = static_cast< QResizeEvent* >( e );
        // Filter out resize events that inform about size different than frame size.
        // This will ensure that decoration->width() etc. and decoration->widget()->width() will be in sync.
        // These events only seem to be delayed events from initial resizing before show() was called
        // on the decoration widget.
        if( ev->size() != size())
            return true;
        }
    return false;
    }

// return value matters only when filtering events before decoration gets them
bool Client::buttonPressEvent( Window w, int button, int state, int x, int y, int x_root, int y_root )
    {
    if (buttonDown)
        {
        if( w == wrapperId())
            XAllowEvents(qt_xdisplay(), SyncPointer, CurrentTime ); //qt_x_time);
        return true;
        }

    if( w == wrapperId() || w == frameId() || w == decorationId())
        { // FRAME neco s tohohle by se melo zpracovat, nez to dostane dekorace
        updateUserTime();
        workspace()->setWasUserInteraction();
        uint keyModX = (options->keyCmdAllModKey() == Qt::Key_Meta) ?
            KKeyNative::modX(KKey::WIN) :
            KKeyNative::modX(KKey::ALT);
        bool bModKeyHeld = keyModX != 0 && ( state & KKeyNative::accelModMaskX()) == keyModX;

        if( isSplash()
            && button == Button1 && !bModKeyHeld )
            { // hide splashwindow if the user clicks on it
            hideClient( true );
            if( w == wrapperId())
                    XAllowEvents(qt_xdisplay(), SyncPointer, CurrentTime ); //qt_x_time);
            return true;
            }

        Options::MouseCommand com = Options::MouseNothing;
        bool was_action = false;
        bool perform_handled = false;
        if ( bModKeyHeld )
            {
            was_action = true;
            switch (button) 
                {
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
                    com = options->operationWindowMouseWheel( button == Button4 ? 120 : -120 );
                    break;
                }
            }
        else
            { // inactive inner window
            if( !isActive() && w == wrapperId())
                {
                was_action = true;
                perform_handled = true;
                switch (button) 
                    {
                    case Button1:
                        com = options->commandWindow1();
                        break;
                    case Button2:
                        com = options->commandWindow2();
                        break;
                    case Button3:
                        com = options->commandWindow3();
                        break;
                    default:
                        com = Options::MouseActivateAndPassClick;
                    }
                }
            // active inner window
            if( isActive() && w == wrapperId()
                && options->clickRaise && button < 4 ) // exclude wheel
                {
                com = Options::MouseActivateRaiseAndPassClick;
                was_action = true;
                perform_handled = true;
                }
            }
        if( was_action )
            {
            bool replay = performMouseCommand( com, QPoint( x_root, y_root), perform_handled );

            if ( isSpecialWindow())
                replay = TRUE;

            if( w == wrapperId()) // these can come only from a grab
                XAllowEvents(qt_xdisplay(), replay? ReplayPointer : SyncPointer, CurrentTime ); //qt_x_time);
            return true;
            }
        }

    if( w == wrapperId()) // these can come only from a grab
        {
        XAllowEvents(qt_xdisplay(), ReplayPointer, CurrentTime ); //qt_x_time);
        return true;
        }
    if( w == decorationId())
        return false; // don't eat decoration events
    if( w == frameId())
        processDecorationButtonPress( button, state, x, y, x_root, y_root );
    return true;
    }


// this function processes button press events only after decoration decides not to handle them,
// unlike buttonPressEvent(), which (when the window is decoration) filters events before decoration gets them
void Client::processDecorationButtonPress( int button, int /*state*/, int x, int y, int x_root, int y_root )
    {
    Options::MouseCommand com = Options::MouseNothing;
    bool active = isActive();
    if ( !wantsInput() ) // we cannot be active, use it anyway
        active = TRUE;

    if ( button == Button1 )
        com = active ? options->commandActiveTitlebar1() : options->commandInactiveTitlebar1();
    else if ( button == Button2 )
        com = active ? options->commandActiveTitlebar2() : options->commandInactiveTitlebar2();
    else if ( button == Button3 )
        com = active ? options->commandActiveTitlebar3() : options->commandInactiveTitlebar3();
    if( button == Button1
        && com != Options::MouseOperationsMenu // actions where it's not possible to get the matching
        && com != Options::MouseMinimize )  // mouse release event
        {
        mode = mousePosition( QPoint( x, y ));
        buttonDown = TRUE;
        moveOffset = QPoint( x, y );
        invertedMoveOffset = rect().bottomRight() - moveOffset;
        unrestrictedMoveResize = false;
        setCursor( mode ); // update to sizeAllCursor if about to move
        }
    performMouseCommand( com, QPoint( x_root, y_root ));
    }

// called from decoration
void Client::processMousePressEvent( QMouseEvent* e )
    {
    if( e->type() != QEvent::MouseButtonPress )
        {
        kdWarning() << "processMousePressEvent()" << endl;
        return;
        }
    int button;
    switch( e->button())
        {
        case LeftButton:
            button = Button1;
          break;
        case MidButton:
            button = Button2;
          break;
        case RightButton:
            button = Button3;
          break;
        default:
            return;
        }
    processDecorationButtonPress( button, e->state(), e->x(), e->y(), e->globalX(), e->globalY());
    }

// return value matters only when filtering events before decoration gets them
bool Client::buttonReleaseEvent( Window w, int /*button*/, int state, int x, int y, int x_root, int y_root )
    {
    if( w == decorationId() && !buttonDown)
        return false;
    if( w == wrapperId())
        {
        XAllowEvents(qt_xdisplay(), SyncPointer, CurrentTime ); //qt_x_time);
        return true;
        }
    if( w != frameId() && w != decorationId() && w != moveResizeGrabWindow())
        return true;
    x = this->x(); // translate from grab window to local coords
    y = this->y();
    if ( (state & ( Button1Mask & Button2Mask & Button3Mask )) == 0 )
        {
        buttonDown = FALSE;
        if ( moveResizeMode ) 
            {
            finishMoveResize( false );
            // mouse position is still relative to old Client position, adjust it
            QPoint mousepos( x_root - x, y_root - y );
            mode = mousePosition( mousepos );
            }
        setCursor( mode );
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
static Bool motion_predicate( Display*, XEvent* ev, XPointer )
{
    if( ev->type == MotionNotify )
        {
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
    if( next_motion_time != CurrentTime
        && timestampCompare( qt_x_time, next_motion_time ) < 0 )
        return true;
    was_motion = false;
    XSync( qt_xdisplay(), False ); // this helps to discard more MotionNotify events
    XEvent dummy;
    XCheckIfEvent( qt_xdisplay(), &dummy, motion_predicate, NULL );
    return was_motion;
    }

// return value matters only when filtering events before decoration gets them
bool Client::motionNotifyEvent( Window w, int /*state*/, int x, int y, int x_root, int y_root )
    {
    if( w != frameId() && w != decorationId() && w != moveResizeGrabWindow())
        return true; // care only about the whole frame
    if ( !buttonDown ) 
        {
        Position newmode = mousePosition( QPoint( x, y ));
        if( newmode != mode )
            setCursor( newmode );
        mode = newmode;
        // reset the timestamp for the optimization, otherwise with long passivity
        // the option in waitingMotionEvent() may be always true
        next_motion_time = CurrentTime;
        return false;
        }
    if( w == moveResizeGrabWindow())
        {
        x = this->x(); // translate from grab window to local coords
        y = this->y();
        }
    if( !waitingMotionEvent())
        handleMoveResize( x, y, x_root, y_root );
    return true;
    }
    
void Client::focusInEvent( XFocusInEvent* e )
    {
    if( e->window != window())
        return; // only window gets focus
    if ( e->mode == NotifyUngrab )
        return; // we don't care
    if ( e->detail == NotifyPointer )
        return;  // we don't care
    if( !isShown( false ) || !isOnCurrentDesktop()) // we unmapped it, but it got focus meanwhile ->
        return;            // activateNextClient() already transferred focus elsewhere
    // check if this client is in should_get_focus list or if activation is allowed
    bool activate =  workspace()->allowClientActivation( this, -1U, true );
    workspace()->gotFocusIn( this ); // remove from should_get_focus list
    if( activate )
        setActive( TRUE );
    else
        {
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
static Bool predicate_follows_focusin( Display*, XEvent* e, XPointer arg )
    {
    if( follows_focusin || follows_focusin_failed )
        return False;
    Client* c = ( Client* ) arg;
    if( e->type == FocusIn && c->workspace()->findClient( WindowMatchPredicate( e->xfocus.window )))
        { // found FocusIn
        follows_focusin = true;
        return False;
        }
    // events that may be in the queue before the FocusIn event that's being
    // searched for
    if( e->type == FocusIn || e->type == FocusOut || e->type == KeymapNotify )
        return False;
    follows_focusin_failed = true; // a different event - stop search
    return False;
    }

static bool check_follows_focusin( Client* c )
    {
    follows_focusin = follows_focusin_failed = false;
    XEvent dummy;
    // XCheckIfEvent() is used to make the search non-blocking, the predicate
    // always returns False, so nothing is removed from the events queue.
    // XPeekIfEvent() would block.
    XCheckIfEvent( qt_xdisplay(), &dummy, predicate_follows_focusin, (XPointer)c );
    return follows_focusin;
    }


void Client::focusOutEvent( XFocusOutEvent* e )
    {
    if( e->window != window())
        return; // only window gets focus
    if ( e->mode == NotifyGrab )
        return; // we don't care
    if ( isShade() )
        return; // here neither
    if ( e->detail != NotifyNonlinear
        && e->detail != NotifyNonlinearVirtual )
        // SELI check all this
        return; // hack for motif apps like netscape
    if ( QApplication::activePopupWidget() )
        return;
    if( !check_follows_focusin( this ))
        setActive( FALSE );
    }

void Client::visibilityNotifyEvent( XVisibilityEvent * e)
    {
    if( e->window != frameId())
        return; // care only about the whole frame
    bool new_not_obscured = e->state == VisibilityUnobscured;
    if( not_obscured == new_not_obscured )
        return;
    not_obscured = new_not_obscured;
    updateMouseGrab();
    }

// performs _NET_WM_MOVERESIZE
void Client::NETMoveResize( int x_root, int y_root, NET::Direction direction )
    {
    if( direction == NET::Move )
        performMouseCommand( Options::MouseMove, QPoint( x_root, y_root ));
    else if( direction >= NET::TopLeft && direction <= NET::Left ) 
        {
        static const Position convert[] =
            {
            PositionTopLeft,
            PositionTop,
            PositionTopRight,
            PositionRight,
            PositionBottomRight,
            PositionBottom,
            PositionBottomLeft,
            PositionLeft
            };
        if(!isResizable() || isShade())
            return;
        if( moveResizeMode )
            finishMoveResize( false );
        buttonDown = TRUE;
        moveOffset = QPoint( x_root - x(), y_root - y()); // map from global
        invertedMoveOffset = rect().bottomRight() - moveOffset;
        unrestrictedMoveResize = false;
        mode = convert[ direction ];
        setCursor( mode );
        if( !startMoveResize())
            {
            buttonDown = false;
            setCursor( mode );
            }
        }
    else if( direction == NET::KeyboardMove )
        { // ignore mouse coordinates given in the message, mouse position is used by the moving algorithm
        QCursor::setPos( geometry().center() );
        performMouseCommand( Options::MouseUnrestrictedMove, geometry().center());
        }
    else if( direction == NET::KeyboardSize )
        { // ignore mouse coordinates given in the message, mouse position is used by the resizing algorithm
        QCursor::setPos( geometry().bottomRight());
        performMouseCommand( Options::MouseUnrestrictedResize, geometry().bottomRight());
        }
    }

void Client::keyPressEvent( uint key_code )
    {
    updateUserTime();
    if ( !isMove() && !isResize() )
        return;
    bool is_control = key_code & Qt::CTRL;
    bool is_alt = key_code & Qt::ALT;
    key_code = key_code & 0xffff;
    int delta = is_control?1:is_alt?32:8;
    QPoint pos = QCursor::pos();
    switch ( key_code ) 
        {
        case Key_Left:
            pos.rx() -= delta;
            break;
        case Key_Right:
            pos.rx() += delta;
            break;
        case Key_Up:
            pos.ry() -= delta;
            break;
        case Key_Down:
            pos.ry() += delta;
            break;
        case Key_Space:
        case Key_Return:
        case Key_Enter:
            finishMoveResize( false );
            buttonDown = FALSE;
            setCursor( mode );
            break;
        case Key_Escape:
            finishMoveResize( true );
            buttonDown = FALSE;
            setCursor( mode );
            break;
        default:
            return;
        }
    QCursor::setPos( pos );
    }

// ****************************************
// Group
// ****************************************

bool Group::groupEvent( XEvent* e )
    {
    unsigned long dirty[ 2 ];
    leader_info->event( e, dirty, 2 ); // pass through the NET stuff
    if ( ( dirty[ WinInfo::PROTOCOLS ] & NET::WMIcon) != 0 )
        getIcons();
    if(( dirty[ WinInfo::PROTOCOLS2 ] & NET::WM2StartupId ) != 0 )
        startupIdChanged();
    return false;
    }


} // namespace
