/*****************************************************************
kwin - the KDE window manager

Copyright (C) 1999, 2000 Matthias Ettrich <ettrich@kde.org>
******************************************************************/
#define QT_CLEAN_NAMESPACE
#include <klocale.h>
#include <kapp.h>
#include <kdebug.h>
#include <dcopclient.h>
#include <qapplication.h>
#include <qcursor.h>
#include <qbitmap.h>
#include <qimage.h>
#include <qwmatrix.h>
#include <qlayout.h>
#include <qpainter.h>
#include <qwhatsthis.h>
#include <qdatetime.h>
#include <qtimer.h>
#include <kwin.h>
#include <netwm.h>
#include "workspace.h"
#include "client.h"
#include "events.h"
#include "atoms.h"
#include <X11/X.h>
#include <X11/Xos.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <X11/extensions/shape.h>

extern Atom qt_wm_state;
extern Time kwin_time;

static bool resizeHorizontalDirectionFixed = FALSE;
static bool resizeVerticalDirectionFixed = FALSE;
static bool blockAnimation = FALSE;


static QRect* visible_bound = 0;

// NET WM Protocol handler class
class WinInfo : public NETWinInfo {
public:
    WinInfo(::Client * c, Display * display, Window window,
	    Window rwin, unsigned long pr )
	: NETWinInfo( display, window, rwin, pr, NET::WindowManager ) {
      m_client = c;
    }

    virtual void changeDesktop(int desktop) {
	if ( desktop == NETWinInfo::OnAllDesktops )
	    m_client->setSticky( TRUE );
	else
	    m_client->workspace()->sendClientToDesktop( m_client, desktop );
    }
    virtual void changeState( unsigned long state, unsigned long mask ) {
	// state : kwin.h says: possible values are or'ed combinations of NET::Modal,
	// NET::Sticky, NET::MaxVert, NET::MaxHoriz, NET::Shaded, NET::SkipTaskbar

	state &= mask; // for safety, clear all other bits
	
	if ( mask & NET::Sticky )
	    m_client->setSticky( state & NET::Sticky );
	if ( mask & NET::Shaded )
	    m_client->setShade( state & NET::Shaded );

	if ( mask & NET::Max ) {
	    if ( (state & NET::Max) == NET::Max )
		m_client->maximize( Client::MaximizeFull );
	    else if ( state & NET::MaxVert )
		m_client->maximize( Client::MaximizeVertical );
	    else if ( state & NET::MaxHoriz )
		m_client->maximize( Client::MaximizeHorizontal );
	    else
		m_client->maximize( Client::MaximizeRestore );
	}
	
	if ( mask & NET::StaysOnTop ) {
	    m_client->setStaysOnTop( state & NET::StaysOnTop  );
	    m_client->workspace()->raiseClient( m_client );
	}
    }
private:
    ::Client * m_client;
};


void Client::drawbound( const QRect& geom )
{
    if ( visible_bound )
	*visible_bound = geom;
    else
	visible_bound = new QRect( geom );
    QPainter p ( workspace()->desktopWidget() );
    p.setPen( QPen( Qt::white, 5 ) );
    p.setRasterOp( Qt::XorROP );
    p.drawRect( geom );
}

void Client::clearbound()
{
    if ( !visible_bound )
	return;
    drawbound( *visible_bound );
    delete visible_bound;
    visible_bound = 0;
}

void Client::updateShape()
{
    if ( shape() )
	XShapeCombineShape(qt_xdisplay(), winId(), ShapeBounding,
			   windowWrapper()->x(), windowWrapper()->y(),
			   window(), ShapeBounding, ShapeSet);
    else
	XShapeCombineMask( qt_xdisplay(), winId(), ShapeBounding, 0, 0,
			   None, ShapeSet);
}

static void sendClientMessage(Window w, Atom a, long x){
  XEvent ev;
  long mask;

  memset(&ev, 0, sizeof(ev));
  ev.xclient.type = ClientMessage;
  ev.xclient.window = w;
  ev.xclient.message_type = a;
  ev.xclient.format = 32;
  ev.xclient.data.l[0] = x;
  ev.xclient.data.l[1] = kwin_time;
  mask = 0L;
  if (w == qt_xrootwin())
    mask = SubstructureRedirectMask;        /* magic! */
  XSendEvent(qt_xdisplay(), w, False, mask, &ev);
}



/*!
  \class WindowWrapper client.h

  \brief The WindowWrapper class encapsulates a client's managed
  window.

  There's not much to know about this class, it's completley handled by
  the abstract class Client. You get access to the window wrapper with
  Client::windowWrapper(). The big advantage of WindowWrapper is,
  that you can use just like a normal QWidget, although it encapsulates
  an X window that belongs to another application.

  In particular, this means adding a client's windowWrapper() to a
  QLayout for the geometry management. See StdClient for an example
  on how to do this.

 */


WindowWrapper::WindowWrapper( WId w, Client *parent, const char* name)
    : QWidget( parent, name )
{
    win = w;

    setMouseTracking( TRUE );

    setBackgroundColor( colorGroup().background() );

    // we don't want the window to be destroyed when we are destroyed
    XAddToSaveSet(qt_xdisplay(), win );

    // no need to be mapped at this point
    XUnmapWindow( qt_xdisplay(), win );

    // set the border width to 0
    XWindowChanges wc;
    wc.border_width = 0;
    XConfigureWindow( qt_xdisplay(), win, CWBorderWidth, &wc );

    // overwrite Qt-defaults because we need SubstructureNotifyMask
    XSelectInput( qt_xdisplay(), winId(),
		  KeyPressMask | KeyReleaseMask |
		  ButtonPressMask | ButtonReleaseMask |
		  KeymapStateMask |
		  ButtonMotionMask |
		  PointerMotionMask | // need this, too!
		  EnterWindowMask | LeaveWindowMask |
		  FocusChangeMask |
		  ExposureMask |
		  StructureNotifyMask |
		  SubstructureRedirectMask |
		  SubstructureNotifyMask
		  );

    XSelectInput( qt_xdisplay(), w,
		  FocusChangeMask |
		  PropertyChangeMask
		  );

    // install a passive grab to catch mouse button events
    XGrabButton(qt_xdisplay(), AnyButton, AnyModifier, winId(), FALSE,
		ButtonPressMask,
		GrabModeSync, GrabModeAsync,
		None, None );

    reparented = FALSE;
}

WindowWrapper::~WindowWrapper()
{
    releaseWindow();
}




static void ungrabButton( WId winId, int modifier )
{
    XUngrabButton( qt_xdisplay(), AnyButton, modifier, winId );
    XUngrabButton( qt_xdisplay(), AnyButton, modifier | LockMask, winId );
}

/*!
  Called by the client to notify the window wrapper when activation
  state changes.

  Releases the passive grab for some modifier combinations when a
  window becomes active. This helps broken X programs that
  missinterpret LeaveNotify events in grab mode to work properly
  (Motif, AWT, Tk, ...)
 */
void WindowWrapper::setActive( bool active )
{
    if ( active ) {
	if ( options->focusPolicy == Options::ClickToFocus ||  !options->clickRaise )
	    ungrabButton( winId(),  None );
	ungrabButton( winId(),  ShiftMask );
	ungrabButton( winId(),  ControlMask );
	ungrabButton( winId(),  ControlMask | ShiftMask );
    } else {
	XGrabButton(qt_xdisplay(), AnyButton, AnyModifier, winId(), FALSE,
		    ButtonPressMask,
		    GrabModeSync, GrabModeAsync,
		    None, None );
    }
}

QSize WindowWrapper::sizeHint() const
{
    return size();
}

QSizePolicy WindowWrapper::sizePolicy() const
{
    return QSizePolicy( QSizePolicy::Preferred, QSizePolicy::Preferred );
}


void WindowWrapper::resizeEvent( QResizeEvent * )
{
    if ( win && reparented ) {
	XMoveResizeWindow( qt_xdisplay(), win,
			   0, 0, width(), height() );
	if ( ((Client*)parentWidget())->shape() )
	    ((Client*)parentWidget())->updateShape();
    }
}

void WindowWrapper::showEvent( QShowEvent* )
{
    if ( win ) {
	if ( !reparented ) {
	    // get the window. We do it this late in order to
	    // guarantee that our geometry is final. This allows
	    // toolkits to guess the proper frame geometry when
	    // processing the ReparentNotify event from X.
	    XReparentWindow( qt_xdisplay(), win, winId(), 0, 0 );
	    reparented = TRUE;
	}
	XMoveResizeWindow( qt_xdisplay(), win,
			   0, 0, width(), height() );
	XMapRaised( qt_xdisplay(), win );
    }
}
void WindowWrapper::hideEvent( QHideEvent* )
{
    if ( win )
 	XUnmapWindow( qt_xdisplay(), win );
}

void WindowWrapper::invalidateWindow()
{
    win = 0;
}

/*!
  Releases the window. The client has done its job and the window is still existing.
 */
void WindowWrapper::releaseWindow()
{
    if ( win ) {
	if ( reparented ) {
	    XReparentWindow( qt_xdisplay(), win,
			     ((Client*)parentWidget())->workspace()->rootWin(),
			     parentWidget()->x(),
			     parentWidget()->y() );
	}

	XRemoveFromSaveSet(qt_xdisplay(), win );
	invalidateWindow();
    }
}




bool WindowWrapper::x11Event( XEvent * e)
{
    switch ( e->type ) {
    case ButtonPress:
	{
	    if ( ((Client*)parentWidget())->isActive()
		 && ( options->focusPolicy != Options::ClickToFocus &&  options->clickRaise ) ) {
		((Client*)parentWidget())->autoRaise();
		ungrabButton( winId(),  None );
	    }
	
	    bool mod1 = (e->xbutton.state & Mod1Mask) == Mod1Mask;
	    Options::MouseCommand com = Options::MouseNothing;
	    if ( mod1){
		switch (e->xbutton.button) {
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
	    } else {
		switch (e->xbutton.button) {
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
	    bool replay = ( (Client*)parentWidget() )->performMouseCommand( com,
			    QPoint( e->xbutton.x_root, e->xbutton.y_root) );

	    XAllowEvents(qt_xdisplay(), replay? ReplayPointer : SyncPointer, kwin_time);
	    return TRUE;
	}
	break;
    case ButtonRelease:
	XAllowEvents(qt_xdisplay(), SyncPointer, kwin_time);
	break;
    default:
	break;
    }
    return FALSE;
}



/*!
  \class Client client.h

  \brief The Client class encapsulates a window decoration frame.

*/

/*!
  Creates a client on workspace \a ws for window \a w.
 */
Client::Client( Workspace *ws, WId w, QWidget *parent, const char *name, WFlags f )
    : QWidget( parent, name, f | WStyle_Customize | WStyle_NoBorder )
{
    wspace = ws;
    win = w;
    autoRaiseTimer = 0;

    unsigned long properties =
	NET::WMDesktop |
	NET::WMState |
	NET::WMWindowType |
	NET::WMStrut |
	NET::WMName |
	NET::WMIconGeometry
	;

    info = new WinInfo( this, qt_xdisplay(), win, qt_xrootwin(), properties );

    XWindowAttributes attr;
    if (XGetWindowAttributes(qt_xdisplay(), win, &attr)){
	original_geometry.setRect(attr.x, attr.y, attr.width, attr.height );
    }
    mapped = 0;
    wwrap = new WindowWrapper( w, this );
    wwrap->installEventFilter( this );

    // set the initial mapping state
    setMappingState( WithdrawnState );
    desk = -1; // and no desktop yet

    mode = Nowhere;
    buttonDown = FALSE;
    moveResizeMode = FALSE;
    setMouseTracking( TRUE );

    active = FALSE;
    shaded = FALSE;
    transient_for = None;
    passive_focus = FALSE;
    is_shape = FALSE;
    is_sticky = FALSE;
    stays_on_top = FALSE;
    may_move = TRUE;

    getWMHints();
    getWindowProtocols();
    getWmNormalHints(); // get xSizeHint
    fetchName();

    Window ww;
    if ( !XGetTransientForHint( qt_xdisplay(), (Window) win, &ww ) )
	transient_for = None;
    else
    	transient_for = (WId) ww;

    if ( mainClient()->isSticky() )
	setSticky( TRUE );


    // should we open this window on a certain desktop?
    if ( info->desktop() == NETWinInfo::OnAllDesktops )
	setSticky( TRUE );
    else if ( info->desktop() )
	desk= info->desktop(); // window had the initial desktop property!


    // window wants to stay on top?
    stays_on_top = ( info->state() & NET::StaysOnTop) != 0;




    // if this window is transient, ensure that it is opened on the
    // same window as its parent.  this is necessary when an application
    // starts up on a different desktop than is currently displayed
    //
    if ( isTransient() )
	desk = mainClient()->desktop();

}

/*!
  Destructor, nothing special.
 */
Client::~Client()
{
    releaseWindow();
    if (workspace()->activeClient() == this)
       workspace()->setFocusChangeEnabled(true); // Safety

    delete info;
}


/*!
  Manages the clients. This means handling the very first maprequest:
  reparenting, initial geometry, initial state, placement, etc.
 */
void Client::manage( bool isMapped )
{

    if (layout())
      layout()->setResizeMode( QLayout::Minimum );
    QRect geom( original_geometry );
    bool placementDone = FALSE;

    SessionInfo* session = workspace()->takeSessionInfo( this );
    if ( session )
	geom.setRect( session->x, session->y, session->width, session->height );


    if ( isMapped  || session )
	placementDone = TRUE;
    else {
	if ( (xSizeHint.flags & PPosition) || (xSizeHint.flags & USPosition) ) {
	    // support for obsolete hints
	    if ( xSizeHint.x != 0 && geom.x() == 0 )
		geom.setRect( xSizeHint.x, geom.y(), geom.width(), geom.height() );
	    if ( xSizeHint.y != 0 && geom.y() == 0 )
		geom.setRect( geom.x(), xSizeHint.y, geom.width(), geom.height() );

	    if ( (xSizeHint.flags & USPosition) == 0 ) {
		QRect area = workspace()->clientArea();
		if ( !area.contains( geom.topLeft() ) ) {
		    int tx = geom.x();
		    int ty = geom.y();
		    if ( tx >= 0 && tx < area.x() )
			tx = area.x();
		    if ( ty >= 0 && ty < area.y() )
			ty = area.y();
		    geom.moveTopLeft( QPoint( tx, ty ) );
		}
	    }
	    
	    placementDone = TRUE;
	}
 	if ( (xSizeHint.flags & USSize) || (xSizeHint.flags & PSize) ) {
	    // keep in mind that we now actually have a size :-)
 	}
  	if (xSizeHint.flags & PMaxSize)
  	    geom.setSize( geom.size().boundedTo( QSize(xSizeHint.max_width, xSizeHint.max_height ) ) );
  	if (xSizeHint.flags & PMinSize)
  	    geom.setSize( geom.size().expandedTo( QSize(xSizeHint.min_width, xSizeHint.min_height ) ) );
    }

    windowWrapper()->resize( geom.size() );
    // the clever activate() trick is necessary
    activateLayout();
    resize ( sizeForWindowSize( geom.size() ) );
    activateLayout();
    

    move( geom.x(), geom.y() );
    gravitate( FALSE );

    if ( !placementDone ) {
	workspace()->doPlacement( this );
	placementDone = TRUE;
    }

    if ( (is_shape = Shape::hasShape( win )) ) {
	updateShape();
    }

    // initial state
    int state = NormalState;
    if ( session ) {
	if ( session->iconified )
	    state = IconicState;
    } else {
	// find out the initial state. Several possibilities exist
	XWMHints * hints = XGetWMHints(qt_xdisplay(), win );
	if (hints && (hints->flags & StateHint))
	    state = hints->initial_state;
	if (hints)
	    XFree(hints);
    }

    // initial desktop placement - note we don't clobber desk if it is
    // set to some value, in case the initial desktop code in the
    // constructor has already set a value for us
    if ( session  ) {
	desk = session->desktop;
    } else if ( desk <= 0 ) {
	// assume window wants to be visible on the current desktop
	desk =  workspace()->currentDesktop();
    }

    info->setDesktop( desk );


    setMappingState( state );
    if ( state == NormalState && isOnDesktop( workspace()->currentDesktop() ) ) {
	Events::raise( isTransient() ? Events::TransNew : Events::New );
	workspace()->raiseClient( this ); // ensure constrains
	show();
	if ( options->focusPolicyIsReasonable() && wantsTabFocus() )
	    workspace()->requestFocus( this );
    }

    // other settings from the previous session
    if ( session ) {
	setSticky( session->sticky );
    }

    delete session;

    workspace()->updateClientArea();
}


/*!
  Gets the client's normal WM hints and reconfigures itself respectively.
 */
void Client::getWmNormalHints()
{
    // TODO keep in mind changing of fix size! if !isWithdrawn()!
    long msize;
    if (XGetWMNormalHints(qt_xdisplay(), win, &xSizeHint, &msize) == 0 )
	xSizeHint.flags = 0;
}

/*!
  Fetches the window's caption (WM_NAME property). It will be
  stored in the client's caption().
 */
void Client::fetchName()
{
//####     QString s = KWM::title( win );
    QString s;

    char* c = 0;
    if ( XFetchName( qt_xdisplay(), win, &c ) != 0 ) {
	s = QString::fromLocal8Bit( c );
	XFree( c );
    }

    if ( s != caption() ) {
	setCaption( "" );
	if (workspace()->hasCaption( s ) ){
	    int i = 2;
	    QString s2;
	    do {
		s2 = s + " <" + QString::number(i) + ">";
		i++;
	    } while (workspace()->hasCaption(s2) );
	    s = s2;
	}
	setCaption( s );
	
	info->setVisibleName( s.utf8() );

	if ( !isWithdrawn() )
	    captionChange( caption() );
    }
}

/*!
  Sets the client window's mapping state. Possible values are
  WithdrawnState, IconicState, NormalState.
 */
void Client::setMappingState(int s){
    if ( !win)
	return;
  unsigned long data[2];
  data[0] = (unsigned long) s;
  data[1] = (unsigned long) None;

  state = s;
  XChangeProperty(qt_xdisplay(), win, qt_wm_state, qt_wm_state, 32,
		  PropModeReplace, (unsigned char *)data, 2);
}


/*!
  General handler for XEvents concerning the client window
 */
bool Client::windowEvent( XEvent * e)
{

    unsigned int dirty = info->event( e ); // pass through the NET stuff

    dirty = 0; // shut up, compiler

    switch (e->type) {
    case UnmapNotify:
	if ( e->xunmap.window == winId() ) {
	    mapped = 0;
	    return FALSE;
	}
	return unmapNotify( e->xunmap );
    case MapNotify:
	if ( e->xmap.window == winId() ) {
	    mapped = 1;
	    return FALSE;
	}
	break;
    case MapRequest:
	return mapRequest( e->xmaprequest );
    case ConfigureRequest:
	return configureRequest( e->xconfigurerequest );
    case PropertyNotify:
	return propertyNotify( e->xproperty );
    case ButtonPress:
    case ButtonRelease:
	break;
    case FocusIn:
	if ( e->xfocus.mode == NotifyUngrab )
	    break; // we don't care
	if ( e->xfocus.detail == NotifyPointer )
	    break;  // we don't care
	setActive( TRUE );
	break;
    case FocusOut:
	if ( e->xfocus.mode == NotifyGrab )
	    break; // we don't care
	if ( isShade() )
	    break; // we neither
	if ( e->xfocus.detail != NotifyNonlinear )
	    return TRUE; // hack for motif apps like netscape
	setActive( FALSE );
	break;
    case ReparentNotify:
	break;
    case ClientMessage:
	return clientMessage( e->xclient );
    default:
	break;
    }

    return TRUE; // we accept everything :-)
}


/*!
  Handles map requests of the client window
 */
bool Client::mapRequest( XMapRequestEvent& /* e */  )
{
    switch ( mappingState() ) {
    case WithdrawnState:
	manage();
	break;
    case IconicState:
	// only show window if we're on current desktop
	if ( isOnDesktop( workspace()->currentDesktop() ) )
	    show();
	else
	    setMappingState( NormalState );
	break;
    case NormalState:
	// only show window if we're on current desktop
	if ( isOnDesktop( workspace()->currentDesktop() ) )
	    show(); // for safety
	break;
    }

    return TRUE;
}


/*!
  Handles unmap notify events of the client window
 */
bool Client::unmapNotify( XUnmapEvent& e )
{

    if ( e.event != windowWrapper()->winId() && !e.send_event )
      return TRUE;

    switch ( mappingState() ) {
    case IconicState:
	// only react on sent events, all others are produced by us
	if ( e.send_event )
	    withdraw();
	break;
    case NormalState:
  	if ( ( !mapped || !windowWrapper()->isVisibleTo( this )) && !e.send_event )
  	    return TRUE; // this event was produced by us as well

	// maybe we will be destroyed soon. Check this first.
	XEvent ev;
	if  ( XCheckTypedWindowEvent (qt_xdisplay(), windowWrapper()->winId(),
				      DestroyNotify, &ev) ){
	    workspace()->destroyClient( this );
	    return TRUE;
	}
	// fall through
    case WithdrawnState: // however that has been possible....
	withdraw();
	break;
    }
    return TRUE;
}

/*!
  Withdraws the window and destroys the client afterwards
 */
void Client::withdraw()
{
    Events::raise( isTransient() ? Events::TransDelete : Events::Delete );
    setMappingState( WithdrawnState );
//###     KWM::moveToDesktop( win, -1 ); // compatibility
    releaseWindow();
    workspace()->destroyClient( this );
}

/*!
  Handles configure  requests of the client window
 */
bool Client::configureRequest( XConfigureRequestEvent& e )
{
    if ( isResize() )
	return TRUE; // we have better things to do right now

    if ( isShade() )
	setShade( FALSE );

      // compress configure requests
    XEvent otherEvent;
    while (XCheckTypedWindowEvent (qt_xdisplay(), win,
				   ConfigureRequest, &otherEvent) ) {
	if (otherEvent.xconfigurerequest.value_mask == e.value_mask)
	    e = otherEvent.xconfigurerequest;
	else {
	    XPutBackEvent(qt_xdisplay(), &otherEvent);
	    break;
	}
    }

    bool stacking = e.value_mask & CWStackMode;
    int stack_mode = e.detail;

    if ( e.value_mask & CWBorderWidth ) {
	// first, get rid of a window border
	XWindowChanges wc;
	unsigned int value_mask = 0;

	wc.border_width = 0;
	value_mask = CWBorderWidth;
	XConfigureWindow( qt_xdisplay(), win, value_mask, & wc );
    }

    if ( e.value_mask & (CWX | CWY ) ) {
	int nx = x() + windowWrapper()->x();
	int ny = y() + windowWrapper()->y();
	if ( e.value_mask & CWX )
	    nx = e.x;
	if ( e.value_mask & CWY )
	    ny = e.y;
	move( nx - windowWrapper()->x(), ny - windowWrapper()->y() );
    }

    if ( e.value_mask & (CWWidth | CWHeight ) ) {
	int nw = windowWrapper()->width();
	int nh = windowWrapper()->height();
	if ( e.value_mask & CWWidth )
	    nw = e.width;
	if ( e.value_mask & CWHeight )
	    nh = e.height;
	resize( sizeForWindowSize( QSize( nw, nh ) ) );
    }


    if ( stacking ){
	switch (stack_mode){
	case Above:
	case TopIf:
	    workspace()->raiseClient( this );
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

    sendSynteticConfigureNotify();
    return TRUE;
}


/*!
  Handles property changes of the client window
 */
bool Client::propertyNotify( XPropertyEvent& e )
{
    switch ( e.atom ) {
    case XA_WM_NORMAL_HINTS:
	getWmNormalHints();
	break;
    case XA_WM_NAME:
	fetchName();
	break;
    case XA_WM_TRANSIENT_FOR:
    	Window ww;
	if ( !XGetTransientForHint( qt_xdisplay(), (Window) win, &ww ) )
	    transient_for = None;
	else
	    transient_for = (WId) ww;
	break;
    case XA_WM_HINTS:
	getWMHints();
	break;
    default:
	if ( e.atom == atoms->wm_protocols )
	    getWindowProtocols();

	break;
    }
    return TRUE;
}


/*!
   Handles client messages for the client window
*/
bool Client::clientMessage( XClientMessageEvent& e )
{

    if ( e.message_type == atoms->kde_wm_change_state ) {
	if ( e.data.l[0] == IconicState && isNormal() ) {
	    if ( e.data.l[1] )
		blockAnimation = TRUE;
	    iconify();
	} else if ( e.data.l[1] == NormalState && isIconified() ) {
	    if ( e.data.l[1] )
		blockAnimation = TRUE;
	    // only show window if we're on current desktop
	    if ( isOnDesktop( workspace()->currentDesktop() ) )
		show();
	    else
		setMappingState( NormalState );
	}
	blockAnimation = FALSE;
    } else if ( e.message_type == atoms->wm_change_state) {
	if ( e.data.l[0] == IconicState && isNormal() )
	    iconify();
	return TRUE;
    }

    return FALSE;
}


/*!
  Auxiliary function to inform the client about the current window
  configuration.

 */
void Client::sendSynteticConfigureNotify()
{
    XConfigureEvent c;
    c.type = ConfigureNotify;
    c.event = win;
    c.window = win;
    c.x = x() + windowWrapper()->x();
    c.y = y() + windowWrapper()->y();
    c.width = windowWrapper()->width();
    c.height = windowWrapper()->height();
    c.border_width = 0;
    XSendEvent( qt_xdisplay(), c.event, TRUE, NoEventMask, (XEvent*)&c );

    // inform clients about the frame geometry
    NETStrut strut;
    QRect wr = windowWrapper()->geometry();
    QRect mr = rect();
    strut.left = wr.left();
    strut.right = mr.right() - wr.right();
    strut.top = wr.top();
    strut.bottom = mr.bottom() - wr.bottom();
    info->setKDEFrameStrut( strut );
}



/*!
  Adjust the frame size \a frame according to he window's size hints.
 */
QSize Client::adjustedSize( const QSize& frame) const
{
    // first, get the window size for the given frame size s

    QSize wsize( frame.width() - ( width() - wwrap->width() ),
	     frame.height() - ( height() - wwrap->height() ) );

    return sizeForWindowSize( wsize );
}

/*!
  Calculate the appropriate frame size for the given window size \a
  wsize.

  \a wsize is adapted according to the window's size hints (minimum,
  maximum and incremental size changes).

 */
QSize Client::sizeForWindowSize( const QSize& wsize, bool ignore_height) const
{
    int w = wsize.width();
    int h = wsize.height();
    if (w<1) w = 1;
    if (h<1) h = 1;

    int bw = 0;
    int bh = 0;

    if (xSizeHint.flags & PBaseSize) {
	bw = xSizeHint.base_width;
	bh = xSizeHint.base_height;
	if (w < xSizeHint.base_width)
	    w = xSizeHint.base_width;
	if (h < xSizeHint.base_height)
	    h = xSizeHint.base_height;
    }

    if (xSizeHint.flags & PResizeInc) {
	if (  xSizeHint.width_inc > 0 ) {
	    int sx = (w - bw) / xSizeHint.width_inc;
	    w = bw + sx * xSizeHint.width_inc;
	}
	if (  xSizeHint.height_inc > 0 ) {
	    int sy = (h - bh) / xSizeHint.height_inc;
	    h = bh + sy * xSizeHint.height_inc;
	}
    }

    if (xSizeHint.flags & PMaxSize) {
 	w = QMIN( xSizeHint.max_width, w );
	h = QMIN( xSizeHint.max_height, h );
    }
    if (xSizeHint.flags & PMinSize) {
 	w = QMAX( xSizeHint.min_width, w );
	h = QMAX( xSizeHint.min_height, h );
    }

    w = QMAX( minimumWidth(), w );
    h = QMAX( minimumHeight(), h );

    int ww = wwrap->width();
    int wh = 0;
    if ( !wwrap->testWState( WState_ForceHide ) )
 	wh = wwrap->height();

    if ( ignore_height && wsize.height() == 0 )
	h = 0;

    return QSize( width() - ww + w,  height()-wh+h );
}


/*!
  Returns whether the window is resizable or has a fixed size.
 */
bool Client::isResizable() const
{
    if ( ( xSizeHint.flags & PMaxSize) == 0 || (xSizeHint.flags & PMinSize ) == 0 )
	return TRUE;
    return ( xSizeHint.min_width != xSizeHint.max_width  ) ||
	  ( xSizeHint.min_height != xSizeHint.max_height  );
}


/*!
  Reimplemented to provide move/resize
 */
void Client::mousePressEvent( QMouseEvent * e)
{
    if (buttonDown) return;
    if (e->state() & AltButton)
    {
	Options::MouseCommand com = Options::MouseNothing;
	if ( e->button() == LeftButton ) {
	    com = options->commandAll1();
	}
	else if (e->button() == MidButton) {
	    com = options->commandAll2();
	}
	else if (e->button() == RightButton) {
	    com = options->commandAll3();
	}
	else {
	    return;
	}
	performMouseCommand( com, e->globalPos());
    }
    else {
        if ( e->button() == LeftButton ) {
	    if ( options->focusPolicyIsReasonable() )
	        workspace()->requestFocus( this );
	    workspace()->raiseClient( this );
	    mouseMoveEvent( e );
	    buttonDown = TRUE;
	    moveOffset = e->pos();
	    invertedMoveOffset = rect().bottomRight() - e->pos();
        }
        else if ( e->button() == MidButton ) {
            workspace()->lowerClient( this );
        }
        else if ( e->button() == RightButton ) {
	    if ( isActive() & ( options->focusPolicy != Options::ClickToFocus &&  options->clickRaise ) )
		autoRaise();
	    workspace()->clientPopup( this )->popup( e->globalPos() );
        }
    }
}

/*!
  Reimplemented to provide move/resize
 */
void Client::mouseReleaseEvent( QMouseEvent * e)
{
    if ( (e->stateAfter() & MouseButtonMask) == 0 ) {
	buttonDown = FALSE;
	if ( moveResizeMode ) {
	    clearbound();
	    if ( ( isMove() && options->moveMode != Options::Opaque )
		 || ( isResize() && options->resizeMode != Options::Opaque ) )
		XUngrabServer( qt_xdisplay() );
	    setGeometry( geom );
	    Events::raise( isResize() ? Events::ResizeEnd : Events::MoveEnd );
	    moveResizeMode = FALSE;
            workspace()->setFocusChangeEnabled(true);
	    releaseMouse();
	    releaseKeyboard();
	}
    }
}


/*!
 */
void Client::resizeEvent( QResizeEvent * e)
{
    QWidget::resizeEvent( e );
}


/*!
  Reimplemented to provide move/resize
 */
void Client::mouseMoveEvent( QMouseEvent * e)
{
    if ( !buttonDown ) {
	mode = mousePosition( e->pos() );
	setMouseCursor( mode );
        geom = geometry();
	return;
    }

    if ( !isMovable()) return;

    if ( !moveResizeMode )
    {
	QPoint p( e->pos() - moveOffset );
	if (p.manhattanLength() >= 6) {
	    moveResizeMode = TRUE;
	    workspace()->setFocusChangeEnabled(false);
	    Events::raise( isResize() ? Events::ResizeStart : Events::MoveStart );
	    grabMouse( cursor() ); // to keep the right cursor
	    if ( ( isMove() && options->moveMode != Options::Opaque )
		 || ( isResize() && options->resizeMode != Options::Opaque ) )
		XGrabServer( qt_xdisplay() );
	}
	else
	    return;
    }

    if ( mode !=  Center && shaded ) {
	wwrap->show();
	workspace()->requestFocus( this );
	shaded = FALSE;
    }

    QPoint globalPos = e->globalPos(); // pos() + geometry().topLeft();


    QPoint p = globalPos + invertedMoveOffset;

    QPoint pp = globalPos - moveOffset;

    QSize mpsize( geometry().right() - pp.x() + 1, geometry().bottom() - pp.y() + 1 );
    mpsize = adjustedSize( mpsize );
    QPoint mp( geometry().right() - mpsize.width() + 1,
	       geometry().bottom() - mpsize.height() + 1 );

    geom = geometry();
    switch ( mode ) {
    case TopLeft:
	geom =  QRect( mp, geometry().bottomRight() ) ;
	break;
    case BottomRight:
	geom =  QRect( geometry().topLeft(), p ) ;
	break;
    case BottomLeft:
	geom =  QRect( QPoint(mp.x(), geometry().y() ), QPoint( geometry().right(), p.y()) ) ;
	break;
    case TopRight:
	geom =  QRect( QPoint(geometry().x(), mp.y() ), QPoint( p.x(), geometry().bottom()) ) ;
	break;
    case Top:
	geom =  QRect( QPoint( geometry().left(), mp.y() ), geometry().bottomRight() ) ;
	break;
    case Bottom:
	geom =  QRect( geometry().topLeft(), QPoint( geometry().right(), p.y() ) ) ;
	break;
    case Left:
	geom =  QRect( QPoint( mp.x(), geometry().top() ), geometry().bottomRight() ) ;
	break;
    case Right:
	geom =  QRect( geometry().topLeft(), QPoint( p.x(), geometry().bottom() ) ) ;
	break;
    case Center:
	geom.moveTopLeft( pp );
	break;
    default:
//fprintf(stderr, "KWin::mouseMoveEvent with mode = %d\n", mode);
	break;
    }

    QRect desktopArea = workspace()->clientArea();
    int marge = 5;

    if ( isResize() && geom.size() != size() ) {
        if (geom.bottom() < desktopArea.top()+marge)
            geom.setBottom(desktopArea.top()+marge);
        if (geom.top() > desktopArea.bottom()-marge)
            geom.setTop(desktopArea.bottom()-marge);
        if (geom.right() < desktopArea.left()+marge)
            geom.setRight(desktopArea.left()+marge);
        if (geom.left() > desktopArea.right()-marge)
            geom.setLeft(desktopArea.right()-marge);

        geom.setSize( adjustedSize( geom.size() ) );
	if  (options->resizeMode == Options::Opaque ) {
	    setGeometry( geom );
	} else if ( options->resizeMode == Options::Transparent ) {
	    clearbound();
	    drawbound( geom );
	}
    }
    else if ( isMove() && geom.topLeft() != geometry().topLeft() ) {
	geom.moveTopLeft( workspace()->adjustClientPosition( this, geom.topLeft() ) );
        if (geom.bottom() < desktopArea.top()+marge)
            geom.moveBottomLeft( QPoint( geom.left(), desktopArea.top()+marge));
        if (geom.top() > desktopArea.bottom()-marge)
            geom.moveTopLeft( QPoint( geom.left(), desktopArea.bottom()-marge));
        if (geom.right() < desktopArea.left()+marge)
            geom.moveTopRight( QPoint( desktopArea.left()+marge, geom.top()));
        if (geom.left() > desktopArea.right()-marge)
            geom.moveTopLeft( QPoint( desktopArea.right()-marge, geom.top()));
        switch ( options->moveMode ) {
        case Options::Opaque:
            move( geom.topLeft() );
            break;
        case Options::Transparent:
            clearbound();
            drawbound( geom );
            break;
        }
    }

    QApplication::syncX(); // process our own configure events synchronously.
}

/*!
  Reimplemented to provide move/resize
 */
void Client::enterEvent( QEvent * )
{
}

/*!
  Reimplemented to provide move/resize
 */
void Client::leaveEvent( QEvent * )
{
    if ( !buttonDown )
	setCursor( arrowCursor );
}



/*!
  Reimplemented to inform the client about the new window position.
 */
void Client::setGeometry( int x, int y, int w, int h )
{
    QWidget::setGeometry(x, y, w, h);
    if ( !isResize() )
	sendSynteticConfigureNotify();
}

/*!
  Reimplemented to inform the client about the new window position.
 */
void Client::move( int x, int y )
{
    QWidget::move( x, y );
    sendSynteticConfigureNotify();
}


/*!\reimp
 */
void Client::showEvent( QShowEvent* )
{
    if ( isIconified() && !isTransient() )
	animateIconifyOrDeiconify( FALSE );
    setMappingState( NormalState );
 }

/*!
  Reimplemented to hide the window wrapper as well. Also informs the
  workspace.
 */
void Client::hideEvent( QHideEvent* )
{
    workspace()->clientHidden( this );
}



/*!
  Late initialialize the client after the window has been managed.

  Ensure to call the superclasses init() implementation when subclassing.
 */
void Client::init()
{
}

/*!\fn captionChange( const QString& name )

  Indicates that the caption (the window title) changed to \a
  name. Subclasses shall then repaint the title string in a clever,
  fast mannor. The default implementation calls repaint( FALSE );
 */
void Client::captionChange( const QString& )
{
    repaint( FALSE );
}


/*!\fn activeChange( bool act )

  Indicates that the activation state changed to \a act.  Subclasses
  may want to indicate the new state graphically in a clever, fast
  mannor.  The default implementation calls repaint( FALSE );
 */
void Client::activeChange( bool )
{
    repaint( FALSE );
}

/*!
  Indicates that the application's icon changed to \a act.  Subclasses
  may want to indicate the new state graphically in a clever, fast
  mannor.  The default implementation calls repaint( FALSE );
 */
void Client::iconChange()
{
    repaint( FALSE );
}


/*!\fn maximizeChange( bool max )

  Indicates that the window was maximized or demaximized. \a max is
  set respectively. Subclasses may want to indicate the new state
  graphically, for example with a different icon.
 */
void Client::maximizeChange( bool )
{
}

/*!\fn stickyChange( bool sticky )

  Indicates that the window was made sticky or unsticky. \a sticky is
  set respectively. Subclasses may want to indicate the new state
  graphically, for example with a different icon.
 */
void Client::stickyChange( bool )
{
}



/*!
  Paints a client window.

  The default implementation does nothing. To be implemented by subclasses.
 */
void Client::paintEvent( QPaintEvent * )
{
}



/*!
  Releases the window. The client has done its job and the window is still existing.
 */
void Client::releaseWindow()
{
    if ( win ) {
	gravitate( TRUE );
	windowWrapper()->releaseWindow();
	win = 0;
    }
}

/*!
  Invalidates the window to avoid the client accessing it again.

  This function is called by the workspace when the window has been
  destroyed.
 */
void Client::invalidateWindow()
{
    win = 0;
    windowWrapper()->invalidateWindow();
}



/*!
  Iconifies this client plus its transients
 */
void Client::iconify()
{
    if (!isMovable())
       return;

    if ( isShade() )
	setShade( FALSE );
    if ( workspace()->iconifyMeansWithdraw( this ) ) {
	Events::raise( isTransient() ? Events::TransDelete : Events::Delete );
	setMappingState( WithdrawnState );
	hide();
	return;
    }
    Events::raise( Events::Iconify );
    setMappingState( IconicState );

    if ( !isTransient() )
	animateIconifyOrDeiconify( TRUE );
    hide();

    workspace()->iconifyOrDeiconifyTransientsOf( this );
}


/*!
  Closes the window by either sending a delete_window message or
  using XKill.
 */
void Client::closeWindow()
{
    Events::raise( Events::Close );
    if ( Pdeletewindow ){
	sendClientMessage( win, atoms->wm_protocols, atoms->wm_delete_window);
    }
    else {
	// client will not react on wm_delete_window. We have not choice
	// but destroy his connection to the XServer.
	XKillClient(qt_xdisplay(), win );
	workspace()->destroyClient( this );
    }
}


/*!
  Kills the window via XKill
 */
void Client::killWindow()
{
    // not sure if we need an Events::Kill or not.. until then, use
    // Events::Close
    Events::raise( Events::Close );

    // always kill this client at the server
    XKillClient(qt_xdisplay(), win );
    workspace()->destroyClient( this );
}

void Client::maximize( MaximizeMode m)
{
    if (!isMovable() || !isResizable() )
       return;

    QRect clientArea = workspace()->clientArea();

    if (isShade())
	setShade( FALSE );

    if ( !geom_restore.isNull() )
	m = MaximizeRestore;

    if ( m != MaximizeRestore ) {
	Events::raise( Events::Maximize );
	geom_restore = geometry();
    }

    switch (m) {

    case MaximizeVertical:
	setGeometry(
		    QRect(QPoint(x(), clientArea.top()),
			  adjustedSize(QSize(width(), clientArea.height())))
		    );
	info->setState( NET::MaxVert, NET::MaxVert );
	break;

    case MaximizeHorizontal:

	setGeometry(
		    QRect(
			  QPoint(clientArea.left(), y()),
			  adjustedSize(QSize(clientArea.width(), height())))
		    );
	info->setState( NET::MaxHoriz, NET::MaxHoriz );
	break;
	
    case MaximizeRestore: {
	Events::raise( Events::UnMaximize );
	setGeometry(geom_restore);
	QRect invalid;
	geom_restore = invalid;
	info->setState( 0, NET::Max );
	} break;

    case MaximizeFull:

	setGeometry(
		    QRect(clientArea.topLeft(), adjustedSize(clientArea.size()))
		    );
	info->setState( NET::Max, NET::Max );
    }

    maximizeChange( m != MaximizeRestore );
}


void Client::toggleSticky()
{
    setSticky( !isSticky() );
}

void Client::maximize()
{
    maximize( MaximizeFull );
}




/*!
  Catch events of the WindowWrapper
 */
bool Client::eventFilter( QObject *o, QEvent * e)
{
    if ( o != wwrap )
	return FALSE;
    switch ( e->type() ) {
    case QEvent::Show:
	windowWrapperShowEvent( (QShowEvent*)e );
	break;
    case QEvent::Hide:
	windowWrapperHideEvent( (QHideEvent*)e );
	break;
    default:
	break;
    }

    return FALSE;
}

void Client::gravitate( bool invert )
{
    int gravity, dx, dy;
    dx = dy = 0;

    gravity = NorthWestGravity;
    if ( xSizeHint.flags & PWinGravity)
	gravity = xSizeHint.win_gravity;

    switch (gravity) {
    case NorthWestGravity:
	dx = 0;
	dy = 0;
	break;
    case NorthGravity:
	dx = -windowWrapper()->x();
	dy = 0;
	break;
    case NorthEastGravity:
	dx = -( width() - windowWrapper()->width() );
	dy = 0;
	break;
    case WestGravity:
	dx = 0;
	dy = -windowWrapper()->y();
	break;
    case CenterGravity:
    case StaticGravity:
	dx = -windowWrapper()->x();
	dy = -windowWrapper()->y();
	break;
    case EastGravity:
	dx = -( width() - windowWrapper()->width() );
	dy = -windowWrapper()->y();
	break;
    case SouthWestGravity:
	dx = 0;
	dy = -( height() - windowWrapper()->height() );
	break;
    case SouthGravity:
	dx = -windowWrapper()->x();
	dy = -( height() - windowWrapper()->height() );
	break;
    case SouthEastGravity:
	dx = -( width() - windowWrapper()->width() - 1 );
	dy = -( height() - windowWrapper()->height() - 1 );
	break;
    }
    if (invert)
	move( x() - dx, y() - dy );
    else
	move( x() + dx, y() + dy );
}

/*!
  Reimplement to handle crossing events (qt should provide xroot, yroot)

  Crossing events are necessary for the focus-follows-mouse focus
  policies, to do proper activation and deactivation.
*/
bool Client::x11Event( XEvent * e)
{
    if ( e->type == EnterNotify && e->xcrossing.mode == NotifyNormal ) {
	if ( options->focusPolicy == Options::ClickToFocus )
	    return TRUE;
	
	if ( options->autoRaise && !isDesktop() && !isDock() && !isMenu() && workspace()->focusChangeEnabled() ) {
	    delete autoRaiseTimer;
	    autoRaiseTimer = new QTimer( this );
	    connect( autoRaiseTimer, SIGNAL( timeout() ), this, SLOT( autoRaise() ) );
	    autoRaiseTimer->start( options->autoRaiseInterval, TRUE  );
	}
	
	if ( options->focusPolicy !=  Options::FocusStrictlyUnderMouse   && ( isDesktop() || isDock() || isMenu() ) )
	    return TRUE;
	
	workspace()->requestFocus( this );
	return TRUE;
    }
    if ( e->type == LeaveNotify && e->xcrossing.mode == NotifyNormal ) {
	if ( !buttonDown )
	    setCursor( arrowCursor );
	bool lostMouse = !rect().contains( QPoint( e->xcrossing.x, e->xcrossing.y ) );
	if ( lostMouse ) {
	    delete autoRaiseTimer;
	    autoRaiseTimer = 0;
	    }
	if ( options->focusPolicy == Options::FocusStrictlyUnderMouse )
	    if ( isActive() && lostMouse )
		workspace()->requestFocus( 0 ) ;
	return TRUE;
    }
    return FALSE;
}


/*!
  Returns a logical mouse position for the cursor position \a
  p. Possible positions are: Nowhere, TopLeft , BottomRight,
  BottomLeft, TopRight, Top, Bottom, Left, Right, Center
 */
Client::MousePosition Client::mousePosition( const QPoint& p ) const
{
    const int range = 16;
    const int border = 4;

    MousePosition m = Nowhere;


    if ( ( p.x() > border && p.x() < width() - border )
	 && ( p.y() > border && p.y() < height() - border ) )
	return Center;

    if ( p.y() <= range && p.x() <= range)
	m = TopLeft;
    else if ( p.y() >= height()-range && p.x() >= width()-range)
	m = BottomRight;
    else if ( p.y() >= height()-range && p.x() <= range)
	m = BottomLeft;
    else if ( p.y() <= range && p.x() >= width()-range)
	m = TopRight;
    else if ( p.y() <= border )
	m = Top;
    else if ( p.y() >= height()-border )
	m = Bottom;
    else if ( p.x() <= border )
	m = Left;
    else if ( p.x() >= width()-border )
	m = Right;
    else
	m = Center;
    return m;
}

/*!
  Sets an appropriate cursor shape for the logical mouse position \a m

  \sa QWidget::setCursor()
 */
void Client::setMouseCursor( MousePosition m )
{
    switch ( m ) {
    case TopLeft:
    case BottomRight:
	setCursor( sizeFDiagCursor );
	break;
    case BottomLeft:
    case TopRight:
	setCursor( sizeBDiagCursor );
	break;
    case Top:
    case Bottom:
	setCursor( sizeVerCursor );
	break;
    case Left:
    case Right:
	setCursor( sizeHorCursor );
	break;
    default:
	setCursor( arrowCursor );
	break;
    }
}


bool Client::isShade() const
{
    return shaded;
}

void Client::setShade( bool s )
{
    if (!isMovable())
       return;

    if ( shaded == s )
	return;

    shaded = s;

    int as = options->animateShade? options->animSteps : 1;

    if (shaded ) {
	int h = height();
	QSize s( sizeForWindowSize( QSize( windowWrapper()->width(), 0), TRUE ) );
	windowWrapper()->hide();
	repaint( FALSE ); // force direct repaint
	setWFlags( WNorthWestGravity );
	int step = QMAX( 4, QABS( h - s.height() ) / as )+1;
	do {
	    h -= step;
	    resize ( s.width(), h );
	    QApplication::syncX();
	} while ( h > s.height() + step );
	clearWFlags( WNorthWestGravity );
	resize (s );
	XEvent tmpE;
	do {
	    XWindowEvent (qt_xdisplay(), windowWrapper()->winId(),
			  SubstructureNotifyMask, &tmpE);
	} while ( tmpE.type != UnmapNotify  || tmpE.xunmap.window != win );
    } else {
	int h = height();
	QSize s( sizeForWindowSize( windowWrapper()->size(), TRUE ) );
	setWFlags( WNorthWestGravity );
	int step = QMAX( 4, QABS( h - s.height() ) / as )+1;
	do {
	    h += step;
	    resize ( s.width(), h );
	    // assume a border
	    // we do not have time to wait for X to send us paint events
  	    repaint( 0, h - step-5, width(), step+5, TRUE);
	    QApplication::syncX();
	} while ( h < s.height() - step );
	clearWFlags( WNorthWestGravity );
	resize ( s );
	windowWrapper()->show();
	activateLayout();
	repaint();
	if ( isActive() )
	    workspace()->requestFocus( this );
    }

    workspace()->iconifyOrDeiconifyTransientsOf( this );
}


/*!
  Sets the client's active state to \a act.

  This function does only change the visual appearance of the client,
  it does not change the focus setting. Use
  Workspace::activateClient() or Workspace::requestFocus() instead.

  If a client receives or looses the focus, it calls setActive() on
  its own.

 */
void Client::setActive( bool act)
{
    windowWrapper()->setActive( act );
    if ( act )
	workspace()->setActiveClient( this );

    if ( active == act )
	return;
    active = act;
    if ( !active && autoRaiseTimer ) {
	delete autoRaiseTimer;
	autoRaiseTimer = 0;
    }
    activeChange( active );
}


/*!
  Sets the window's sticky property to b
 */
void Client::setSticky( bool b )
{
    if ( is_sticky == b )
	return;
    is_sticky = b;
    if ( isVisible() ) {
	if ( is_sticky )
	    Events::raise( Events::Sticky );
	else
	    Events::raise( Events::UnSticky );
    }
    if ( !is_sticky )
	setDesktop( workspace()->currentDesktop() );
    else
	info->setDesktop( NETWinInfo::OnAllDesktops );
    workspace()->setStickyTransientsOf( this, b );
    stickyChange( is_sticky );
}


/*!
  Let the window stay on top or not, depending on \a b

  \sa Workspace::setClientOnTop()
 */
void Client::setStaysOnTop( bool b )
{
    if ( b == staysOnTop() )
	return;
    stays_on_top = b;
    info->setState( b?NET::StaysOnTop:0, NET::StaysOnTop );
}


void Client::setDesktop( int desktop)
{
    desk = desktop;
    info->setDesktop( desktop );
}

void Client::getWMHints()
{
    // get the icons, allow scaling
    icon_pix = KWin::icon( win, 32, 32, TRUE );
    miniicon_pix = KWin::icon( win, 16, 16, TRUE );
    if ( !isWithdrawn() )
	iconChange();

    input = TRUE;
    XWMHints *hints = XGetWMHints(qt_xdisplay(), win );
    if ( hints ) {
	if ( hints->flags & InputHint )
	    input = hints->input;
	XFree((char*)hints);
    }
}

void Client::getWindowProtocols(){
  Atom *p;
  int i,n;

  Pdeletewindow = 0;
  Ptakefocus = 0;
  Pcontexthelp = 0;

  if (XGetWMProtocols(qt_xdisplay(), win, &p, &n)){
      for (i = 0; i < n; i++)
	  if (p[i] == atoms->wm_delete_window)
	      Pdeletewindow = 1;
	  else if (p[i] == atoms->wm_take_focus)
	      Ptakefocus = 1;
	  else if (p[i] == atoms->net_wm_context_help)
	      Pcontexthelp = 1;
      if (n>0)
	  XFree(p);
  }
}

/*!
  Puts the focus on this window. Clients should never calls this
  themselves, instead they should use Workspace::requestFocus().
 */
void Client::takeFocus()
{
    if ( isMenu() )
	return; // menus don't take focus

    if ( input )
	XSetInputFocus( qt_xdisplay(), win, RevertToPointerRoot, kwin_time );
    if ( Ptakefocus )
	sendClientMessage(win, atoms->wm_protocols, atoms->wm_take_focus);
}


/*!\reimp
 */
void Client::setMask( const QRegion & reg)
{
    mask = reg;
    QWidget::setMask( reg );
}


/*!
  Returns the main client. For normal windows, this is the window
  itself. For transient windows, it is the parent.

 */
Client* Client::mainClient()
{
    if  ( !isTransient() )
	return this;
    ClientList saveset;
    Client* c = this;
    do {
	saveset.append( c );
	c = workspace()->findClient( c->transientFor() );
    } while ( c && c->isTransient() && !saveset.contains( c ) );

    return c?c:this;
}


/*!
  Returns whether the window provides context help or not. If it does,
  you should show a help menu item or a help button lie '?' and call
  contextHelp() if this is invoked.

  \sa contextHelp()
 */
bool Client::providesContextHelp() const
{
    return Pcontexthelp;
}


/*!
  Invokes context help on the window. Only works if the window
  actually provides context help.

  \sa providesContextHelp()
 */
void Client::contextHelp()
{
    if ( Pcontexthelp ) {
	sendClientMessage(win, atoms->wm_protocols, atoms->net_wm_context_help);
	QWhatsThis::enterWhatsThisMode();
    }
}


/*!
  Performs a mouse command on this client (see options.h)
 */
bool Client::performMouseCommand( Options::MouseCommand command, QPoint globalPos)
{
    bool replay = FALSE;
    switch (command) {
    case Options::MouseRaise:
	workspace()->raiseClient( this );
	break;
    case Options::MouseLower:
	workspace()->lowerClient( this );
	break;
    case Options::MouseOperationsMenu:
	break;
    case Options::MouseToggleRaiseAndLower:
	if ( workspace()->topClientOnDesktop() == this )
	    workspace()->lowerClient( this );
	else
	    workspace()->raiseClient( this );
	break;
    case Options::MouseActivateAndRaise:
	workspace()->requestFocus( this );
	workspace()->raiseClient( this );
	break;
    case Options::MouseActivateAndLower:
	workspace()->requestFocus( this );
	workspace()->raiseClient( this );
	break;
    case Options::MouseActivate:
	workspace()->requestFocus( this );
	break;
    case Options::MouseActivateRaiseAndPassClick:
	workspace()->requestFocus( this );
	workspace()->raiseClient( this );
	replay = TRUE;
	break;
    case Options::MouseActivateAndPassClick:
	workspace()->requestFocus( this );
	workspace()->raiseClient( this );
	replay = TRUE;
	break;
    case Options::MouseMove:
	if (!isMovable())
	    break;
	mode = Center;
	moveResizeMode = TRUE;
	workspace()->setFocusChangeEnabled(false);
	buttonDown = TRUE;
	moveOffset = mapFromGlobal( globalPos );
	invertedMoveOffset = rect().bottomRight() - moveOffset;
 	grabMouse( arrowCursor );
	grabKeyboard();
	if ( options->moveMode != Options::Opaque )
	    XGrabServer( qt_xdisplay() );
	break;
    case Options::MouseResize: {
	if (!isMovable())
	    break;
	moveResizeMode = TRUE;
	workspace()->setFocusChangeEnabled(false);
	buttonDown = TRUE;
	moveOffset = mapFromGlobal( globalPos );
	int x = moveOffset.x(), y = moveOffset.y();
	bool left = x < width() / 3;
	bool right = x >= 2 * width() / 3;
	bool top = y < height() / 3;
	bool bot = y >= 2 * height() / 3;
	if (top)
	    mode = left ? TopLeft : (right ? TopRight : Top);
	else if (bot)
	    mode = left ? BottomLeft : (right ? BottomRight : Bottom);
	else
	    mode = (x < width() / 2) ? Left : Right;
	invertedMoveOffset = rect().bottomRight() - moveOffset;
	setMouseCursor( mode );
	grabMouse( cursor()  );
	grabKeyboard();
	resizeHorizontalDirectionFixed = FALSE;
	resizeVerticalDirectionFixed = FALSE;
	if ( options->resizeMode != Options::Opaque )
	    XGrabServer( qt_xdisplay() );
	} break;
    case Options::MouseNothing:
	// fall through
    default:
	replay = TRUE;
	break;
    }
    return replay;
}


void Client::keyPressEvent( QKeyEvent * e )
{
    if ( !isMove() && !isResize() )
	return;
    bool is_control = e->state() & ControlButton;
    int delta = is_control?1:8;
    QPoint pos = QCursor::pos();
    switch ( e->key() ) {
    case Key_Left:
	pos.rx() -= delta;
	if ( pos.x() <= workspace()->geometry().left() ) {
	    if ( mode == TopLeft || mode == BottomLeft ) {
		moveOffset.rx() += delta;
		invertedMoveOffset.rx() += delta;
	    } else {
		moveOffset.rx() -= delta;
		invertedMoveOffset.rx() -= delta;
	    }
	}
	if ( isResize() && !resizeHorizontalDirectionFixed ) {
	    resizeHorizontalDirectionFixed = TRUE;
	    if ( mode == BottomRight )
		mode = BottomLeft;
	    else if ( mode == TopRight )
		mode = TopLeft;
	    setMouseCursor( mode );
	    grabMouse( cursor() );
	}
	break;
    case Key_Right:
	pos.rx() += delta;
	if ( pos.x() >= workspace()->geometry().right() ) {
	    if ( mode == TopRight || mode == BottomRight ) {
		moveOffset.rx() += delta;
		invertedMoveOffset.rx() += delta;
	    } else {
		moveOffset.rx() -= delta;
		invertedMoveOffset.rx() -= delta;
	    }
	}
	if ( isResize() && !resizeHorizontalDirectionFixed ) {
	    resizeHorizontalDirectionFixed = TRUE;
	    if ( mode == BottomLeft )
		mode = BottomRight;
	    else if ( mode == TopLeft )
		mode = TopRight;
	    setMouseCursor( mode );
	    grabMouse( cursor() );
	}
	break;
    case Key_Up:
	pos.ry() -= delta;
	if ( pos.y() <= workspace()->geometry().top() ) {
	    if ( mode == TopLeft || mode == TopRight ) {
		moveOffset.ry() += delta;
		invertedMoveOffset.ry() += delta;
	    } else {
		moveOffset.ry() -= delta;
		invertedMoveOffset.ry() -= delta;
	    }
	}
	if ( isResize() && !resizeVerticalDirectionFixed ) {
	    resizeVerticalDirectionFixed = TRUE;
	    if ( mode == BottomLeft )
		mode = TopLeft;
	    else if ( mode == BottomRight )
		mode = TopRight;
	    setMouseCursor( mode );
	    grabMouse( cursor() );
	}
	break;
    case Key_Down:
	pos.ry() += delta;
	if ( pos.y() >= workspace()->geometry().bottom() ) {
	    if ( mode == BottomLeft || mode == BottomRight ) {
		moveOffset.ry() += delta;
		invertedMoveOffset.ry() += delta;
	    } else {
		moveOffset.ry() -= delta;
		invertedMoveOffset.ry() -= delta;
	    }
	}
	if ( isResize() && !resizeVerticalDirectionFixed ) {
	    resizeVerticalDirectionFixed = TRUE;
	    if ( mode == TopLeft )
		mode = BottomLeft;
	    else if ( mode == TopRight )
		mode = BottomRight;
	    setMouseCursor( mode );
	    grabMouse( cursor() );
	}
	break;
    case Key_Space:
    case Key_Return:
    case Key_Enter:
	clearbound();
	if ( ( isMove() && options->moveMode != Options::Opaque )
	     || ( isResize() && options->resizeMode != Options::Opaque ) )
	    XUngrabServer( qt_xdisplay() );
	setGeometry( geom );
	moveResizeMode = FALSE;
	workspace()->setFocusChangeEnabled(true);
	releaseMouse();
	releaseKeyboard();
	buttonDown = FALSE;
	break;
    default:
	return;
    }
    QCursor::setPos( pos );
}


QCString Client::windowRole()
{
    extern Atom qt_window_role;
    Atom type;
    int format;
    unsigned long length, after;
    unsigned char *data;
    QCString result;
    if ( XGetWindowProperty( qt_xdisplay(), win, qt_window_role, 0, 1024,
			     FALSE, XA_STRING, &type, &format,
			     &length, &after, &data ) == Success ) {
	if ( data )
	    result = (const char*) data;
	XFree( data );
    }
    return result;
}

QCString Client::sessionId()
{
    extern Atom qt_sm_client_id;
    Atom type;
    int format;
    unsigned long length, after;
    unsigned char *data;
    QCString result;
    if ( XGetWindowProperty( qt_xdisplay(), win, qt_sm_client_id, 0, 1024,
			     FALSE, XA_STRING, &type, &format,
			     &length, &after, &data ) == Success ) {
	if ( data )
	    result = (const char*) data;
	XFree( data );
    }
    return result;
}

void Client::activateLayout()
{
    if ( layout() )
	layout()->activate();
}


NET::WindowType Client::windowType() const
{
    NET::WindowType wt = info->windowType();
    if ( wt == NET::Unknown )
	wt = NET::Normal;
    return wt;
}

bool Client::wantsTabFocus() const
{
    return windowType() == NET::Normal;
}

bool Client::isMovable() const
{
    return windowType() == NET::Normal || windowType() == NET::Toolbar;
}

bool Client::isDesktop() const
{
    return windowType() == NET::Desktop;
}

bool Client::isDock() const
{
    return windowType() == NET::Dock;
}

bool Client::isMenu() const
{
    return windowType() == NET::Menu;
}



/*!
  Returns \a area with the client's strut taken into account.

  Used from Workspace in updateClientArea.
 */
QRect Client::adjustedClientArea( const QRect& area ) const
{
    QRect r = area;
    NETStrut strut = info->strut();
    if ( strut.left > 0 )
	r.setLeft( r.left() + (int) strut.left );
    if ( strut.top > 0 )
	r.setTop( r.top() + (int) strut.top );
    if ( strut.right > 0  )
	r.setRight( r.right() - (int) strut.right );
    if ( strut.bottom > 0  )
	r.setBottom( r.bottom() - (int) strut.bottom );
    return r;
}


void Client::animateIconifyOrDeiconify( bool iconify)
{
    if ( blockAnimation )
	return;
    // the function is a bit tricky since it will ensure that an
    // animation action needs always the same time regardless of the
    // performance of the machine or the X-Server.

    float lf,rf,tf,bf,step;

    int options_dot_ResizeAnimation = 1;

    step = 40. * (11 - options_dot_ResizeAnimation);

    NETRect r = info->iconGeometry();
    QRect icongeom( r.pos.x, r.pos.y, r.size.width, r.size.height );
    if ( !icongeom.isValid() )
	return;

    QPixmap pm = animationPixmap( iconify ? width() : icongeom.width() );

    QRect before, after;
    if ( iconify ) {
	before = QRect( x(), y(), width(), pm.height() );
	after = QRect( icongeom.x(), icongeom.y(), icongeom.width(), pm.height() );
    } else {
	before = QRect( icongeom.x(), icongeom.y(), icongeom.width(), pm.height() );
	after = QRect( x(), y(), width(), pm.height() );
    }

    lf = (after.left() - before.left())/step;
    rf = (after.right() - before.right())/step;
    tf = (after.top() - before.top())/step;
    bf = (after.bottom() - before.bottom())/step;


    XGrabServer( qt_xdisplay() );

    QRect area = before;
    QRect area2;
    QPixmap pm2;

    QTime t;
    t.start();
    float diff;

    QPainter p ( workspace()->desktopWidget() );
    bool need_to_clear = FALSE;
    QPixmap pm3;
    do {
	if (area2 != area){
	    pm = animationPixmap( area.width() );
	    pm2 = QPixmap::grabWindow( qt_xrootwin(), area.x(), area.y(), area.width(), area.height() );
	    p.drawPixmap( area.x(), area.y(), pm );
	    if ( need_to_clear ) {
		p.drawPixmap( area2.x(), area2.y(), pm3 );
		need_to_clear = FALSE;
	    }
	    area2 = area;
	}
	XFlush(qt_xdisplay());
	XSync( qt_xdisplay(), FALSE );
	diff = t.elapsed();
 	if (diff > step)
 	    diff = step;
	area.setLeft(before.left() + int(diff*lf));
	area.setRight(before.right() + int(diff*rf));
	area.setTop(before.top() + int(diff*tf));
	area.setBottom(before.bottom() + int(diff*bf));
	if (area2 != area ) {
	    if ( area2.intersects( area ) )
		p.drawPixmap( area2.x(), area2.y(), pm2 );
	    else { // no overlap, we can clear later to avoid flicker
		pm3 = pm2;
		need_to_clear = TRUE;
	    }
	}
    } while ( t.elapsed() < step);
    if (area2 == area || need_to_clear )
	p.drawPixmap( area2.x(), area2.y(), pm2 );

    p.end();
    XUngrabServer( qt_xdisplay() );
}


/*!
  The pixmap shown during iconify/deiconify animation
 */
QPixmap Client::animationPixmap( int w )
{
    QFont font = options->font(isActive());
    QFontMetrics fm( font );
    QPixmap pm( w, fm.lineSpacing() );
    pm.fill( options->color(Options::TitleBar, isActive() || isIconified() ) );
    QPainter p( &pm );
    p.setPen(options->color(Options::Font, isActive() || isIconified() ));
    p.setFont(options->font(isActive()));
    p.drawText( pm.rect(), AlignLeft|AlignVCenter|SingleLine, caption() );
    return pm;
}



void Client::autoRaise()
{
    workspace()->raiseClient( this );
    delete autoRaiseTimer;
    autoRaiseTimer = 0;
}


NoBorderClient::NoBorderClient( Workspace *ws, WId w, QWidget *parent, const char *name )
    : Client( ws, w, parent, name )
{
    QHBoxLayout* h = new QHBoxLayout( this );
    h->addWidget( windowWrapper() );
}

NoBorderClient::~NoBorderClient()
{
}

#include "client.moc"

