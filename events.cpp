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

#include <qwhatsthis.h>
#include <kkeynative.h>
#include <qapplication.h>

#include <X11/extensions/shape.h>

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
        m_client->setShade( state & NET::Shaded ? Client::ShadeNormal : Client::ShadeNone );
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
    : NETRootInfo2( dpy, w, name, pr, pr_num, scr )
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

void RootInfo::changeActiveWindow( Window w, NET::RequestSource src, Time timestamp )
    {
    if( Client* c = workspace->findClient( WindowMatchPredicate( w )))
        {
        if( src == NET::FromUnknown )
            src = NET::FromTool; // KWIN_FOCUS, use qt_x_time as timestamp?
        if( src == NET::FromTool )
            workspace->activateClient( c );
        else // NET::FromApplication
            {
            if( workspace->allowClientActivation( c, timestamp ))
                workspace->activateClient( c );
            else
                c->demandAttention();
            }
        }
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
        kwin_updateTime(); // otherwise grabbing may have old timestamp - this message should include timestamp
        c->NETMoveResize( x_root, y_root, (Direction)direction);
        }
    }

void RootInfo::gotPing( Window w, Time timestamp )
    {
    if( Client* c = workspace->findClient( WindowMatchPredicate( w )))
        c->gotPing( timestamp );
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
        c->windowEvent( e );
        return true;
        }
    if( Client* c = findClient( WrapperIdMatchPredicate( e->xany.window )))
        {
        c->windowEvent( e );
        return true;
        }
    if( Client* c = findClient( FrameIdMatchPredicate( e->xany.window )))
        {
        c->windowEvent( e );
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
        // this is special due to
        // SubstructureNotifyMask. e->xany.window is the window the
        // event is reported to. Take care not to confuse Qt.
            Client* c = findClient( WindowMatchPredicate( e->xunmap.window ));

            if ( c )
                {
                c->windowEvent( e );
                return true;
                }

        // check for system tray windows
            if ( removeSystemTrayWin( e->xunmap.window ) ) 
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
            Client* c = findClient( WindowMatchPredicate( e->xreparent.window ));
            if ( c )
                (void) c->windowEvent( e );

        //do not confuse Qt with these events. After all, _we_ are the
        //window manager who does the reparenting.
            return TRUE;
            }
        case DestroyNotify:
            {
            if ( removeSystemTrayWin( e->xdestroywindow.window ) )
                return TRUE;
            Client* c = findClient( WindowMatchPredicate( e->xdestroywindow.window ));
            if( c != NULL )
                {
                c->destroyClient();
                return true;
                }
            return false;
            }
        case MapRequest:
            {
            updateXTime();

            Client* c = findClient( WindowMatchPredicate( e->xmaprequest.window ));
            if ( !c ) 
                {
// don't check for the parent being the root window, this breaks when some app unmaps
// a window, changes something and immediately maps it back, without giving KWin
// a chance to reparent it back to root
// since KWin can get MapRequest only for root window children and
// children of WindowWrapper (=clients), the check is AFAIK useless anyway
//            if ( e->xmaprequest.parent == root ) { //###TODO store previously destroyed client ids
                if ( addSystemTrayWin( e->xmaprequest.window ) )
                    return TRUE;
                c = createClient( e->xmaprequest.window, false );
                if ( c != NULL && root != qt_xrootwin() ) 
                    { // TODO what is this?
                    // TODO may use QWidget:.create
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
            if ( QWhatsThis::inWhatsThisMode() )
            {
            QWidget* w = QWidget::find( e->xcrossing.window );
            if ( w )
                QWhatsThis::leaveWhatsThisMode();
            }

        if (electric_have_borders &&
           (e->xcrossing.window == electric_top_border ||
            e->xcrossing.window == electric_left_border ||
            e->xcrossing.window == electric_bottom_border ||
            e->xcrossing.window == electric_right_border))
            {
            // the user entered an electric border
            electricBorder(e);
            }
        break;
    case LeaveNotify:
            {
            if ( !QWhatsThis::inWhatsThisMode() )
                break;
            Client* c = findClient( FrameIdMatchPredicate( e->xcrossing.window ));
            if ( c && e->xcrossing.detail != NotifyInferior )
                QWhatsThis::leaveWhatsThisMode();
            break;
            }
        case ConfigureRequest:
            {
            Client* c = findClient( WindowMatchPredicate( e->xconfigurerequest.window ));
            if ( c )
                {
                c->windowEvent( e );
                return true;
                }
            else if ( e->xconfigurerequest.parent == root ) 
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
                XConfigureWindow( qt_xdisplay(), e->xconfigurerequest.window, value_mask, & wc );

                return TRUE;
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
        case FocusOut:
            return true; // always eat these, they would tell Qt that KWin is the active app
        default:
            if  ( e->type == Shape::shapeEvent() ) 
            {
            Client* c = findClient( WindowMatchPredicate( ((XShapeEvent *)e)->window ));
            if ( c )
                c->updateShape();
            }
        break;
        }
    return FALSE;
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
void Client::windowEvent( XEvent* e )
    {
    if( e->xany.window == window()) // avoid doing stuff on frame or wrapper
        {
        unsigned long dirty[ 2 ];
        info->event( e, dirty, 2 ); // pass through the NET stuff

        if ( ( dirty[ WinInfo::PROTOCOLS ] & NET::WMName ) != 0 )
            fetchName();
        if ( ( dirty[ WinInfo::PROTOCOLS ] & NET::WMIconName ) != 0 )
            fetchIconicName();
        if ( ( dirty[ WinInfo::PROTOCOLS ] & NET::WMStrut ) != 0 )
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
            return unmapNotifyEvent( &e->xunmap );
        case MapRequest:
                return mapRequestEvent( &e->xmaprequest );
        case ConfigureRequest:
                return configureRequestEvent( &e->xconfigurerequest );
        case PropertyNotify:
            return propertyNotifyEvent( &e->xproperty );
        case KeyPress:
            updateUserTime();
            workspace()->setWasUserInteraction();
            break;
        case ButtonPress:
            updateUserTime();
            workspace()->setWasUserInteraction();
            buttonPressEvent( e->xbutton.window, e->xbutton.button, e->xbutton.state,
                e->xbutton.x, e->xbutton.y, e->xbutton.x_root, e->xbutton.y_root );
            return;
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
            return;
        case MotionNotify:
            motionNotifyEvent( e->xbutton.window, e->xbutton.state,
                e->xbutton.x, e->xbutton.y, e->xbutton.x_root, e->xbutton.y_root );
            return;
        case EnterNotify:
            enterNotifyEvent( &e->xcrossing );
            // MotionNotify is guaranteed to be generated only if the mouse
            // move start and ends in the window; for cases when it only
            // starts or only ends there, Enter/LeaveNotify are generated.
            // Fake a MotionEvent in such cases to make handle of mouse
            // events simpler (Qt does that too).
            motionNotifyEvent( e->xcrossing.window, e->xcrossing.state,
                e->xcrossing.x, e->xcrossing.y, e->xcrossing.x_root, e->xcrossing.y_root );
            return;
        case LeaveNotify:
            motionNotifyEvent( e->xcrossing.window, e->xcrossing.state,
                e->xcrossing.x, e->xcrossing.y, e->xcrossing.x_root, e->xcrossing.y_root );
            return leaveNotifyEvent( &e->xcrossing );
        case FocusIn:
            return focusInEvent( &e->xfocus );
        case FocusOut:
            return focusOutEvent( &e->xfocus );
        case ReparentNotify:
            break;
        case ClientMessage:
            return clientMessageEvent( &e->xclient );
        case ColormapChangeMask:
            if( e->xany.window == window())
            {
            cmap = e->xcolormap.colormap;
            if ( isActive() )
                workspace()->updateColormap();
            }
        break;
      case VisibilityNotify:
          return visibilityNotifyEvent( &e->xvisibility );
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
    }

/*!
  Handles map requests of the client window
 */
void Client::mapRequestEvent( XMapRequestEvent* e )
    {
    if( e->window != window())
        return; // no messing with frame etc.
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
            else if( isShade())
                setShade( ShadeNone );
            else // it's on another virtual desktop
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
        if( e->data.l[ 1 ] )
            blockAnimation = true;
        if( e->data.l[ 0 ] == IconicState )
            minimize();
        else if( e->data.l[ 0 ] == NormalState )
            { // copied from mapRequest()
            if( isMinimized())
                unminimize();
            else if( isShade())
                setShade( ShadeNone );
            else // it's on another virtual desktop
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
    if ( isResize() )
        return; // we have better things to do right now

    if( isFullScreen() // refuse resizing of fullscreen windows
        || isSplash() // no manipulations with splashscreens either
        || isTopMenu()) // topmenus neither
        {
        sendSyntheticConfigureNotify();
        return;
        }

    if ( isShade() ) // SELI SHADE
        setShade( ShadeNone );

    // compress configure requests
    XEvent otherEvent;
    while (XCheckTypedWindowEvent (qt_xdisplay(), window(),
                                   ConfigureRequest, &otherEvent) ) 
        {
        if (otherEvent.xconfigurerequest.value_mask == e->value_mask)
            *e = otherEvent.xconfigurerequest;
        else 
            {
            XPutBackEvent(qt_xdisplay(), &otherEvent);
            break;
            }
        }

    bool stacking = e->value_mask & CWStackMode;
    int stack_mode = e->detail;

    if ( e->value_mask & CWBorderWidth ) 
        {
        // first, get rid of a window border
        XWindowChanges wc;
        unsigned int value_mask = 0;

        wc.border_width = 0;
        value_mask = CWBorderWidth;
        XConfigureWindow( qt_xdisplay(), window(), value_mask, & wc );
        }

    if ( e->value_mask & (CWX | CWY ) ) 
        {
        int ox = 0;
        int oy = 0;
        // GRAVITATE
        int gravity = NorthWestGravity;
        if ( xSizeHint.flags & PWinGravity)
            gravity = xSizeHint.win_gravity;
        if ( gravity == StaticGravity ) 
            { // only with StaticGravity according to ICCCM 4.1.5
            ox = clientPos().x();
            oy = clientPos().y();
            }

        int nx = x() + ox;
        int ny = y() + oy;

        if ( e->value_mask & CWX )
            nx = e->x;
        if ( e->value_mask & CWY )
            ny = e->y;


        // clever workaround for applications like xv that want to set
        // the location to the current location but miscalculate the
        // frame size due to kwin being a double-reparenting window
        // manager
        if ( ox == 0 && oy == 0 &&
             nx == x() + clientPos().x() &&
             ny == y() + clientPos().y() ) 
            {
            nx = x();
            ny = y();
            }


        QPoint np( nx-ox, ny-oy);
#if 0
        if ( windowType() == NET::Normal && may_move ) 
            {
            // crap for broken netscape
            QRect area = workspace()->clientArea();
            if ( !area.contains( np ) && width() < area.width()  &&
                 height() < area.height() ) 
                {
                if ( np.x() < area.x() )
                    np.rx() = area.x();
                if ( np.y() < area.y() )
                    np.ry() = area.y();
                }
            }
#endif

        if ( maximizeMode() != MaximizeFull )
            {
            resetMaximize();
            move( np );
            }
        }

    if ( e->value_mask & (CWWidth | CWHeight ) ) 
        {
        int nw = clientSize().width();
        int nh = clientSize().height();
        if ( e->value_mask & CWWidth )
            nw = e->width;
        if ( e->value_mask & CWHeight )
            nh = e->height;
        QSize ns = sizeForClientSize( QSize( nw, nh ) );

        //QRect area = workspace()->clientArea();
        if ( maximizeMode() != MaximizeRestore ) 
            {  //&& ( ns.width() < area.width() || ns.height() < area.height() ) ) {
            if( ns != size()) 
                { // don't restore if some app sets its own size again
                resetMaximize();
                resize( ns );
                }
            }
        else 
            {
            if ( ns == size() )
                return; // broken xemacs stuff (ediff)
            resize( ns );
            }
        }


    if ( stacking )
        {
        switch (stack_mode)
            {
            case Above:
            case TopIf:
                if( workspace()->allowClientActivation( this )) // not really activation,
                    workspace()->raiseClient( this );           // but it's the same, showing
                else                                            // unwanted window on top
                    workspace()->restackClientUnderActive( this ); // would be obtrusive
                break;
            case Below:
            case BottomIf:
                workspace()->lowerClient( this );
                break;
            case Opposite:
            default:
                break;
            }
        }

    // TODO sending a synthetic configure notify always is fine, even in cases where
    // the ICCCM doesn't require this - it can be though of as 'the WM decided to move
    // the window later'. Perhaps those unnecessary ones could be saved though.
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
                window_role = getStringProperty( window(), qt_window_role );
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
            mode = Nowhere;
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
            delete autoRaiseTimer;
            autoRaiseTimer = 0;
            delete shadeHoverTimer;
            shadeHoverTimer = 0;
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
        XScrL | XNumL, XScrL | XNumL | XCapL };
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
        XScrL | XNumL, XScrL | XNumL | XCapL };
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
    {
    if( isActive() )
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
        // simply grab all modifier combinations
        XGrabButton(qt_xdisplay(), AnyButton, AnyModifier, wrapperId(), FALSE,
            ButtonPressMask,
            GrabModeSync, GrabModeAsync,
            None, None );
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
    return false;
    }

// return value matters only when filtering events before decoration gets them
bool Client::buttonPressEvent( Window w, int button, int state, int x, int y, int x_root, int y_root )
    {
    if (buttonDown)
        {
        kdDebug( 1212 ) << "Double button press:" << ( w == decorationId())
            << ":" << ( w == frameId()) << ":" << ( w == wrapperId()) << endl;
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
        bool bModKeyHeld = ( state & KKeyNative::accelModMaskX()) == keyModX;

        if( isSplash()
            && button == Button1 && !bModKeyHeld )
            { // hide splashwindow if the user clicks on it
            hideClient( true );
            if( w == wrapperId())
                    XAllowEvents(qt_xdisplay(), SyncPointer, CurrentTime ); //qt_x_time);
            return true;
            }

        if ( isActive() && w == wrapperId()
             && ( options->clickRaise && !bModKeyHeld ) ) 
            {
            if ( button < 4 ) // exclude wheel
                autoRaise();
            }

        Options::MouseCommand com = Options::MouseNothing;
        bool was_action = false;
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
                }
            }
        else
            { // inactive inner window
            if( !isActive() && w == wrapperId())
                {
                was_action = true;
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
            }
        if( was_action )
            {
            bool replay = performMouseCommand( com, QPoint( x_root, y_root) );

            if ( isSpecialWindow() && !isOverride())
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
    if( com != Options::MouseOperationsMenu // actions where it's not possible to get the matching
        && com != Options::MouseMinimize )  // mouse release event
        {
// FRAME      mouseMoveEvent( e ); shouldn't be necessary
        buttonDown = TRUE;
        moveOffset = QPoint( x, y );
        invertedMoveOffset = rect().bottomRight() - moveOffset;
        unrestrictedMoveResize = false;
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
    if( w != frameId() && w != decorationId())
        return true;

    if ( (state & ( Button1Mask & Button2Mask & Button3Mask )) == 0 )
        {
        buttonDown = FALSE;
        if ( moveResizeMode ) 
            {
            finishMoveResize( false );
            // mouse position is still relative to old Client position, adjust it
            QPoint mousepos( x_root - x, y_root - y );
                mode = mousePosition( mousepos );
                setCursor( mode );
            }
        }
    return true;
    }

// return value matters only when filtering events before decoration gets them
bool Client::motionNotifyEvent( Window w, int /*state*/, int x, int y, int x_root, int y_root )
    {
    if( w != frameId() && w != decorationId())
        return true; // care only about the whole frame
    if ( !buttonDown ) 
        {
        MousePosition newmode = mousePosition( QPoint( x, y ));
        if( newmode != mode )
            setCursor( newmode );
        mode = newmode;
        return false;
        }

    if(( mode == Center && !isMovable())
        || ( mode != Center && ( isShade() || !isResizable())))
        return true;

    if ( !moveResizeMode ) 
        {
        QPoint p( QPoint( x, y ) - moveOffset );
        if (p.manhattanLength() >= 6)
            startMoveResize();
        else
            return true;
        }

    // ShadeHover or ShadeActive, ShadeNormal was already avoided above
    if ( mode != Center && shade_mode != ShadeNone )
        setShade( ShadeNone );

    QPoint globalPos( x_root, y_root );
    QRect desktopArea = workspace()->clientArea( globalPos );

    QPoint p = globalPos + invertedMoveOffset;

    QPoint pp = globalPos - moveOffset;

    if( !unrestrictedMoveResize )
        { // TODO this is broken
        int left_overlap = width() - border_left - 100;
        int right_overlap = width() - border_right - 100;
        int bottom_overlap = - border_top;
        p.setX( QMIN( desktopArea.right() + right_overlap, QMAX( desktopArea.left() - left_overlap, p.x())));
        p.setY( QMIN( desktopArea.bottom() + bottom_overlap, QMAX( desktopArea.top(), p.y())));
        pp.setX( QMIN( desktopArea.right() + right_overlap, QMAX( desktopArea.left() - left_overlap, pp.x())));
        pp.setY( QMIN( desktopArea.bottom() + bottom_overlap, QMAX( desktopArea.top(), pp.y())));
        }

    QSize mpsize( geometry().right() - pp.x() + 1, geometry().bottom() - pp.y() + 1 );
    mpsize = adjustedSize( mpsize );
    QPoint mp( geometry().right() - mpsize.width() + 1,
               geometry().bottom() - mpsize.height() + 1 );

    moveResizeGeom = geometry();
    switch ( mode ) 
        {
        case TopLeft2:
            moveResizeGeom =  QRect( mp, geometry().bottomRight() ) ;
            break;
        case BottomRight2:
            moveResizeGeom =  QRect( geometry().topLeft(), p ) ;
            break;
        case BottomLeft2:
            moveResizeGeom =  QRect( QPoint(mp.x(), geometry().y() ), QPoint( geometry().right(), p.y()) ) ;
            break;
        case TopRight2:
            moveResizeGeom =  QRect( QPoint(geometry().x(), mp.y() ), QPoint( p.x(), geometry().bottom()) ) ;
            break;
        case Top:
            moveResizeGeom =  QRect( QPoint( geometry().left(), mp.y() ), geometry().bottomRight() ) ;
            break;
        case Bottom:
            moveResizeGeom =  QRect( geometry().topLeft(), QPoint( geometry().right(), p.y() ) ) ;
            break;
        case Left:
            moveResizeGeom =  QRect( QPoint( mp.x(), geometry().top() ), geometry().bottomRight() ) ;
            break;
        case Right:
            moveResizeGeom =  QRect( geometry().topLeft(), QPoint( p.x(), geometry().bottom() ) ) ;
            break;
        case Center:
            moveResizeGeom.moveTopLeft( pp );
            break;
        default:
//fprintf(stderr, "KWin::mouseMoveEvent with mode = %d\n", mode);
            break;
        }

    const int marge = 5;

    // TODO move whole group when moving its leader or when the leader is not mapped?

    if ( isResize() && moveResizeGeom.size() != size() ) 
        {
        if (moveResizeGeom.bottom() < desktopArea.top()+marge)
            moveResizeGeom.setBottom(desktopArea.top()+marge);
        if (moveResizeGeom.top() > desktopArea.bottom()-marge)
            moveResizeGeom.setTop(desktopArea.bottom()-marge);
        if (moveResizeGeom.right() < desktopArea.left()+marge)
            moveResizeGeom.setRight(desktopArea.left()+marge);
        if (moveResizeGeom.left() > desktopArea.right()-marge)
            moveResizeGeom.setLeft(desktopArea.right()-marge);

        moveResizeGeom.setSize( adjustedSize( moveResizeGeom.size() ) );
        if  (options->resizeMode == Options::Opaque ) 
            {
            setGeometry( moveResizeGeom );
            positionGeometryTip();
            }
        else if ( options->resizeMode == Options::Transparent ) 
            {
            clearbound();  // it's necessary to move the geometry tip when there's no outline
            positionGeometryTip(); // shown, otherwise it would cause repaint problems in case
            drawbound( moveResizeGeom ); // they overlap; the paint event will come after this,
            }                               // so the geometry tip will be painted above the outline
        }
    else if ( isMove() && moveResizeGeom.topLeft() != geometry().topLeft() ) 
        {
        moveResizeGeom.moveTopLeft( workspace()->adjustClientPosition( this, moveResizeGeom.topLeft() ) );
        if (moveResizeGeom.bottom() < desktopArea.top()+marge)
            moveResizeGeom.moveBottomLeft( QPoint( moveResizeGeom.left(), desktopArea.top()+marge));
        if (moveResizeGeom.top() > desktopArea.bottom()-marge)
            moveResizeGeom.moveTopLeft( QPoint( moveResizeGeom.left(), desktopArea.bottom()-marge));
        if (moveResizeGeom.right() < desktopArea.left()+marge)
            moveResizeGeom.moveTopRight( QPoint( desktopArea.left()+marge, moveResizeGeom.top()));
        if (moveResizeGeom.left() > desktopArea.right()-marge)
            moveResizeGeom.moveTopLeft( QPoint( desktopArea.right()-marge, moveResizeGeom.top()));
        switch ( options->moveMode ) 
            {
            case Options::Opaque:
                move( moveResizeGeom.topLeft() );
                positionGeometryTip();
                break;
            case Options::Transparent:
                clearbound();
                positionGeometryTip();
                drawbound( moveResizeGeom );
                break;
            }
        }

    if ( isMove() )
      workspace()->clientMoved(globalPos, qt_x_time);
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
        static const MousePosition convert[] =
            {
            TopLeft2,
            Top,
            TopRight2,
            Right,
            BottomRight2,
            Bottom,
            BottomLeft2,
            Left
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
        startMoveResize();
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


} // namespace
