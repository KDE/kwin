/*****************************************************************
kwin - the KDE window manager

Copyright (C) 1999, 2000 Matthias Ettrich <ettrich@kde.org>
******************************************************************/
//#define QT_CLEAN_NAMESPACE
#include <kconfig.h>
#include <kglobal.h>
#include <kglobalsettings.h>
#include <kglobalaccel.h>
#include <klocale.h>
#include <stdlib.h>
#include <qwhatsthis.h>
#include <qdatastream.h>
#include <kapp.h>
#include <dcopclient.h>
#include <kprocess.h>

#include <netwm.h>

#include "workspace.h"
#include "client.h"
#include "tabbox.h"
#include "atoms.h"
#include "plugins.h"
#include "events.h"
#include "killwindow.h"
#include <X11/X.h>
#include <X11/Xos.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <X11/keysym.h>
#include <X11/keysymdef.h>
#include <X11/extensions/shape.h>
#include <X11/cursorfont.h>

const int XIconicState = IconicState;
#undef IconicState

#include <kwin.h>
#include <kapp.h>


// NET WM Protocol handler class
class RootInfo : public NETRootInfo
{
public:
    RootInfo( Workspace* ws, Display *dpy, Window w, const char *name, unsigned long pr, int scr= -1)
	: NETRootInfo( dpy, w, name, pr, scr ) {
	    workspace = ws;
    }
    ~RootInfo() {}

    void changeNumberOfDesktops(int n) { workspace->setNumberOfDesktops( n ); }
    void changeCurrentDesktop(int d) { workspace->setCurrentDesktop( d ); }
    void changeActiveWindow(Window w) {
	::Client* c = workspace->findClient( (WId) w );
	if ( c )
	    workspace->activateClient( c );
    }
    void closeWindow(Window w) {
	::Client* c = workspace->findClient( (WId) w );
	if ( c ) {
	    c->closeWindow();
	}
    }
    void moveResize(Window, int, int, unsigned long) { }

private:
    Workspace* workspace;
};


QString Workspace::desktopName( int desk )
{
    return QString::fromUtf8( rootInfo->desktopName( desk ) );
}

extern Time kwin_time;
extern void kwin_updateTime();

// used to store the return values of
// XShapeQueryExtension.
// Necessary since shaped window are an extension to X
static int kwin_has_shape = 0;
static int kwin_shape_event = 0;
static bool block_focus = FALSE;
// does the window w  need a shape combine mask around it?
bool Shape::hasShape( WId w){
    int xws, yws, xbs, ybs;
    unsigned wws, hws, wbs, hbs;
    int boundingShaped, clipShaped;
    if (!kwin_has_shape)
	return FALSE;
    XShapeQueryExtents(qt_xdisplay(), w,
		       &boundingShaped, &xws, &yws, &wws, &hws,
		       &clipShaped, &xbs, &ybs, &wbs, &hbs);
    return boundingShaped != 0;
}

int Shape::shapeEvent()
{
    return kwin_shape_event;
}

bool Motif::noBorder( WId w )
{
    struct MwmHints {
	ulong flags;
	ulong functions;
	ulong decorations;
	long input_mode;
	ulong status;
    };
    Atom type;
    int format;
    unsigned long length, after;
    unsigned char* data;
    MwmHints* hints = 0;
    if ( XGetWindowProperty( qt_xdisplay(), w, atoms->motif_wm_hints, 0, 5,
			     FALSE, atoms->motif_wm_hints, &type, &format,
			     &length, &after, &data ) == Success ) {
	if ( data )
	    hints = (MwmHints*) data;
    }
    bool result = FALSE;
    if ( hints ) {
	if ( hints->flags & (1L << 1 ) ) { //  // MWM_HINTS_DECORATIONS;
	    if ( hints->decorations == 0 )
		result = TRUE;
	}
	XFree( data );
    }
    return result;
}




/*!
  Creates a new client for window \a w, depending on certain hints
  (like Motif hints and the NET_WM_TYPE.

  Shaped windows always get a NoBorderClient.
 */
Client* Workspace::clientFactory( WId w )
{
    NETWinInfo ni( qt_xdisplay(), w, root, NET::WMWindowType );

    if ( (ni.windowType() == NET::Normal || ni.windowType() == NET::Unknown)
	 && Motif::noBorder( w ) )
	return new NoBorderClient( this, w );

    switch ( ni.windowType() ) {
    case NET::Desktop:
	{
	    XLowerWindow( qt_xdisplay(), w );
	    Client * c = new NoBorderClient( this, w);
	    c->setSticky( TRUE );
	    setDesktopClient( c );
	    return c;
	}

    case NET::Tool:
	return ( mgr.allocateClient( this, w, true ) );

    case NET::Menu:
    case NET::Dock:
	{
	    Client * c = new NoBorderClient( this, w );
	    c->setSticky( TRUE );
	    return c;
	}

    case NET::Override:
	return new NoBorderClient( this, w);

    default:
	break;
    }

    if ( Shape::hasShape( w ) ){
	return new NoBorderClient( this, w );
    }
    return ( mgr.allocateClient( this, w, false ) );
}

// Rikkus: This class is too complex. It needs splitting further.
// It's a nightmare to understand, especially with so few comments :(

// Matthias: Feel free to ask me questions about it. Feel free to add
// comments. I dissagree that further splittings makes it easier. 2500
// lines are not too much. It's the task that is complex, not the
// code.
Workspace::Workspace( bool restore )
  : QObject           (0, "workspace"),
    DCOPObject        ("KWinInterface"),
    current_desktop   (0),
    number_of_desktops(0),
    desktop_widget    (0),
    desktop_client    (0),
    active_client     (0),
    should_get_focus  (0),
    most_recently_raised (0),
    control_grab      (false),
    tab_grab          (false),
    mouse_emulation   (false),
    focus_change      (true),
    tab_box           (0),
    popup             (0),
    desk_popup        (0),
    keys              (0),
    root              (0)
{
    root = qt_xrootwin();
    default_colormap = DefaultColormap(qt_xdisplay(), qt_xscreen() );
    installed_colormap = default_colormap;
    session.setAutoDelete( TRUE );

    area = QApplication::desktop()->geometry();

    if ( restore )
      loadSessionInfo();

    (void) QApplication::desktop(); // trigger creation of desktop widget

    desktop_widget =
      new QWidget(
        0,
        "desktop_widget",
        Qt::WType_Desktop | Qt::WPaintUnclipped
    );

    // select windowmanager privileges
    XSelectInput(qt_xdisplay(), root,
		 KeyPressMask |
		 PropertyChangeMask |
		 ColormapChangeMask |
		 SubstructureRedirectMask |
		 SubstructureNotifyMask
		 );

    int dummy;
    kwin_has_shape =
      XShapeQueryExtension(qt_xdisplay(), &kwin_shape_event, &dummy);

    // compatibility
    long data = 1;

    XChangeProperty(
      qt_xdisplay(),
      qt_xrootwin(),
      atoms->kwin_running,
      atoms->kwin_running,
      32,
      PropModeAppend,
      (unsigned char*) &data,
      1
    );

    grabKey(XK_Tab, Mod1Mask);
    grabKey(XK_Tab, Mod1Mask | ShiftMask);
    grabKey(XK_Tab, ControlMask);
    grabKey(XK_Tab, ControlMask | ShiftMask);
    createKeybindings();
    tab_box = new TabBox( this );

    init();

    if ( restore ) { // pseudo session management with wmCommand
	for (SessionInfo* info = session.first(); info; info = session.next() ) {
	    if ( info->sessionId.isEmpty() && !info->wmCommand.isEmpty() ) {
		KShellProcess proc;
		proc << QString::fromLatin1( info->wmCommand );
		proc.start(KShellProcess::DontCare);	
	    }
	}
    }
}

void Workspace::init()
{
    supportWindow = new QWidget;

    unsigned long protocols =
	NET::Supported |
	NET::SupportingWMCheck |
	NET::ClientList |
	NET::ClientListStacking |
	NET::NumberOfDesktops |
	NET::CurrentDesktop |
	NET::ActiveWindow |
	NET::WorkArea |
	NET::CloseWindow |
	NET::DesktopNames |
	
	NET::WMName |
	NET::WMDesktop |
	NET::WMWindowType |
	NET::WMState |
	NET::WMStrut |
	NET::WMIconGeometry |
	NET::WMIcon |
	NET::WMPid |
	NET::WMKDESystemTrayWinFor
	;
	
    rootInfo = new RootInfo( this, qt_xdisplay(), supportWindow->winId(), "KWin", protocols, qt_xscreen() );

    loadDesktopSettings();
    setCurrentDesktop( 1 );

    unsigned int i, nwins;
    Window root_return, parent_return, *wins;
    XWindowAttributes attr;

    connect(&mgr, SIGNAL(resetAllClients()), this,
            SLOT(slotResetAllClients()));
    connect(kapp, SIGNAL(appearanceChanged()), this,
            SLOT(slotResetAllClients()));

    XQueryTree(qt_xdisplay(), root, &root_return, &parent_return, &wins, &nwins);
    for (i = 0; i < nwins; i++) {
	XGetWindowAttributes(qt_xdisplay(), wins[i], &attr);
	if (attr.override_redirect )
	    continue;
	if (attr.map_state != IsUnmapped) {
	    if ( addSystemTrayWin( wins[i] ) )
		continue;
	    Client* c = clientFactory( wins[i] );
	    if ( c != desktop_client ) {
		clients.append( c );
		stacking_order.append( c );
	    }
	    if ( c->wantsTabFocus() )
		focus_chain.append( c );
	    c->manage( TRUE );
	    if ( c == desktop_client )
		setDesktopClient( c );
	    if ( root != qt_xrootwin() ) {
		// TODO may use QWidget:.create
		XReparentWindow( qt_xdisplay(), c->winId(), root, 0, 0 );
		c->move(0,0);
	    }
	}
    }
    if ( wins )
	XFree((void *) wins);
    propagateClients();

    //CT initialize the cascading info
    for( int i = 0; i < numberOfDesktops(); i++) {
      CascadingInfo inf;
      inf.pos = QPoint(0,0);
      inf.col = 0;
      inf.row = 0;
      cci.append(inf);
    }

    updateClientArea();

}

Workspace::~Workspace()
{
    if ( desktop_client ) {
	WId win = desktop_client->window();
	delete desktop_client;
	XMapWindow( qt_xdisplay(), win );
	XLowerWindow( qt_xdisplay(), win );
    }
    for ( ClientList::ConstIterator it = clients.begin(); it != clients.end(); ++it) {
	WId win = (*it)->window();
	delete (*it);
	XMapWindow( qt_xdisplay(), win );
    }
    delete desktop_widget;
    delete tab_box;
    delete popup;
    delete keys;
    if ( root == qt_xrootwin() )
	XDeleteProperty(qt_xdisplay(), qt_xrootwin(), atoms->kwin_running);
    KGlobal::config()->sync();

    delete rootInfo;
    delete supportWindow;
}


/*!
  Handles workspace specific XEvents
 */
bool Workspace::workspaceEvent( XEvent * e )
{
    if ( mouse_emulation && e->type == ButtonPress || e->type == ButtonRelease ) {
	mouse_emulation = FALSE;
	XUngrabKeyboard( qt_xdisplay(), kwin_time );
    }

    Client * c = findClient( e->xany.window );
    if ( c )
	return c->windowEvent( e );

    switch (e->type) {
    case ButtonPress:
    case ButtonRelease:
    case MotionNotify:
	break;
    case UnmapNotify:
	// this is special due to
	// SubstructureNotifyMask. e->xany.window is the window the
	// event is reported to. Take care not to confuse Qt.
	c = findClient( e->xunmap.window );

	if ( c )
	    return c->windowEvent( e );

	// check for system tray windows
 	if ( removeSystemTrayWin( e->xunmap.window ) )
 	    return TRUE;

	return ( e->xunmap.event != e->xunmap.window ); // hide wm typical event from Qt
	
    case MapNotify:

	return ( e->xmap.event != e->xmap.window ); // hide wm typical event from Qt
	
    case ReparentNotify:
	c = findClient( e->xreparent.window );
	if ( c )
	    (void) c->windowEvent( e );
	
	//do not confuse Qt with these events. After all, _we_ are the
	//window manager who does the reparenting.
	return TRUE;
    case DestroyNotify:
	if ( removeSystemTrayWin( e->xdestroywindow.window ) )
	    return TRUE;
	return destroyClient( findClient( e->xdestroywindow.window ) );
    case MapRequest:
	kwin_updateTime();
	c = findClient( e->xmaprequest.window );
	if ( !c ) {
	    if ( e->xmaprequest.parent ) { // == root ) { //###TODO store rpeviously destroyed client ids
		if ( addSystemTrayWin( e->xmaprequest.window ) )
		    return TRUE;
		c = clientFactory( e->xmaprequest.window );
		if ( root != qt_xrootwin() ) {
		    // TODO may use QWidget:.create
		    XReparentWindow( qt_xdisplay(), c->winId(), root, 0, 0 );
		}
		if ( c != desktop_client ) {
		    if ( c->wantsTabFocus() )
			focus_chain.prepend( c );
		    clients.append( c );
		    stacking_order.append( c );
		}
	    }
	}
	if ( c ) {
	    bool result = c->windowEvent( e );
	    propagateClients();
	    if ( c == desktop_client )
		setDesktopClient( c );
	    return result;
	}
	break;
    case EnterNotify:
	if ( !QWhatsThis::inWhatsThisMode() )
	    break;
	{
	    QWidget* w = QWidget::find( e->xcrossing.window );
	    if ( w && w->inherits("WindowWrapper" ) )
		QWhatsThis::leaveWhatsThisMode();
	}
	break;
    case LeaveNotify:
	if ( !QWhatsThis::inWhatsThisMode() )
	    break;
	c = findClientWidthId( e->xcrossing.window );
	if ( c && e->xcrossing.detail != NotifyInferior )
	    QWhatsThis::leaveWhatsThisMode();
	break;
    case ConfigureRequest:
	c = findClient( e->xconfigurerequest.window );
	if ( c )
	    return c->windowEvent( e );
	else if ( e->xconfigurerequest.parent == root ) {
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
    case KeyPress:
	if ( QWidget::keyboardGrabber() ) {
	    freeKeyboard( FALSE );
	    break;
	}
	if ( mouse_emulation )
	    return keyPressMouseEmulation( e->xkey );
	return keyPress(e->xkey);
    case KeyRelease:
	if ( QWidget::keyboardGrabber() ) {
	    freeKeyboard( FALSE );
	    break;
	}
	if ( mouse_emulation )
	    return FALSE;
	return keyRelease(e->xkey);
	break;
    case FocusIn:
	break;
    case FocusOut:
	break;
    case PropertyNotify:
    case ClientMessage:
	return netCheck( e );
	break;
    default:
	if  ( e->type == Shape::shapeEvent() ) {
	    c = findClient( ((XShapeEvent *)e)->window );
	    if ( c )
		c->updateShape();
	}
	break;
    }
    return FALSE;
}

bool Workspace::hasClient(Client* c)
{
    for ( ClientList::ConstIterator it = clients.begin(); it != clients.end(); ++it) {
	if ( (*it) == c )
	    return TRUE;
    }

    return FALSE;
}

/*!
  Finds the client that embedds the window \a w
 */
Client* Workspace::findClient( WId w ) const
{
    for ( ClientList::ConstIterator it = clients.begin(); it != clients.end(); ++it) {
	if ( (*it)->window() == w )
	    return *it;
    }
    if ( desktop_client && w == desktop_client->window() )
	return desktop_client;
    return 0;
}

/*!
  Finds the client with window id \a w
 */
Client* Workspace::findClientWidthId( WId w ) const
{
    for ( ClientList::ConstIterator it = clients.begin(); it != clients.end(); ++it) {
	if ( (*it)->winId() == w )
	    return *it;
    }
    return 0;
}


/*!
  Returns the workspace's geometry

  \sa clientArea()
 */
QRect Workspace::geometry() const
{
    if ( root == qt_xrootwin() )
	return QRect( QPoint(0, 0), QApplication::desktop()->size() );
    else {
	// todo caching, keep track of configure notify etc.
	QRect r;
	XWindowAttributes attr;
	if (XGetWindowAttributes(qt_xdisplay(), root, &attr)){
	    r.setRect(0, 0, attr.width, attr.height );
	}
	return r;
    }
}

/*
  Destroys the client \a c
 */
bool Workspace::destroyClient( Client* c)
{
    if ( !c )
	return FALSE;
    clients.remove( c );
    stacking_order.remove( c );
    focus_chain.remove( c );
    c->invalidateWindow();
    clientHidden( c );
    if ( c == desktop_client )
	desktop_client = 0;
    if ( c == most_recently_raised )
	most_recently_raised = 0;
    if ( c == should_get_focus )
	should_get_focus = 0;
    if ( c == active_client )
	active_client = 0;
    delete c;
    propagateClients();
    updateClientArea();
    return TRUE;
}



/*!
  Auxiliary function to release a passive keyboard grab
 */
void Workspace::freeKeyboard(bool pass){
    if (!pass)
      XAllowEvents(qt_xdisplay(), AsyncKeyboard, kwin_time);
    else
      XAllowEvents(qt_xdisplay(), ReplayKeyboard, kwin_time);
    QApplication::syncX();
}

/*!
  Handles alt-tab / control-tab
 */
bool Workspace::keyPress(XKeyEvent key)
{
    if ( root != qt_xrootwin() )
	return FALSE;
    int kc = XKeycodeToKeysym(qt_xdisplay(), key.keycode, 0);
    int km = key.state & (ControlMask | Mod1Mask | ShiftMask);

    if (!control_grab){

	if( (kc == XK_Tab)  &&
	    ( km == (Mod1Mask | ShiftMask)
	      || km == (Mod1Mask)
	      )){
	    if (!tab_grab){
		if ( options->altTabStyle == Options::CDE  || !options->focusPolicyIsReasonable() ){
		    // CDE style raise / lower
		    Client* c = topClientOnDesktop();
		    Client* nc = c;
		    if (km & ShiftMask){
			do {
			    nc = previousStaticClient(nc);
			} while (nc && nc != c &&
				 (!nc->isOnDesktop(currentDesktop()) ||
				  nc->isIconified() || !nc->wantsTabFocus() ) );

		    }
		    else
			do {
			    nc = nextStaticClient(nc);
			} while (nc && nc != c &&
				 (!nc->isOnDesktop(currentDesktop()) ||
				  nc->isIconified() || !nc->wantsTabFocus() ) );
		    if (c && c != nc)
			lowerClient( c );
		    if (nc) {
			if ( options->focusPolicyIsReasonable() )
			    activateClient( nc );
			else
			    raiseClient( nc );
		    }
		    freeKeyboard(FALSE);
		    return TRUE;
		}
	        XGrabPointer( qt_xdisplay(), root, TRUE,
			      (uint)(ButtonPressMask | ButtonReleaseMask |
				     ButtonMotionMask | EnterWindowMask |
				     LeaveWindowMask | PointerMotionMask),
			      GrabModeSync, GrabModeAsync,
			      None, None, kwin_time );
		XGrabKeyboard(qt_xdisplay(),
			      root, FALSE,
			      GrabModeAsync, GrabModeAsync,
			      kwin_time);
		tab_grab 	= TRUE;
		tab_box->setMode( TabBox::WindowsMode );
		tab_box->reset();
	    }
	    tab_box->nextPrev( (km & ShiftMask) == 0 );
	    keys->setEnabled( FALSE );
	    tab_box->delayedShow();
	}
    }

    if (!tab_grab){


	if( (kc == XK_Tab)  &&
	    ( km == (ControlMask | ShiftMask)
	      || km == (ControlMask)
	      )){
	    if (!control_grab){
	        XGrabPointer( qt_xdisplay(), root, TRUE,
			      (uint)(ButtonPressMask | ButtonReleaseMask |
				     ButtonMotionMask | EnterWindowMask |
				     LeaveWindowMask | PointerMotionMask),
			      GrabModeSync, GrabModeAsync,
			      None, None, kwin_time );
		XGrabKeyboard(qt_xdisplay(),
			      root, FALSE,
			      GrabModeAsync, GrabModeAsync,
			      kwin_time);
		control_grab = TRUE;
		tab_box->setMode( TabBox::DesktopMode );
		tab_box->reset();
	    }
	    tab_box->nextPrev( (km & ShiftMask) == 0 );
	    keys->setEnabled( FALSE );
	    tab_box->delayedShow();
	}
    }

    if (control_grab || tab_grab){
	if (kc == XK_Escape){
	    XUngrabKeyboard(qt_xdisplay(), kwin_time);
	    XUngrabPointer( qt_xdisplay(), kwin_time);
	    tab_box->hide();
	    keys->setEnabled( TRUE );
	    tab_grab = FALSE;
	    control_grab = FALSE;
	    return TRUE;
	}
	return FALSE;
    }

    freeKeyboard(FALSE);
    return FALSE;
}

/*!
  Handles alt-tab / control-tab
 */
bool Workspace::keyRelease(XKeyEvent key)
{
    if ( root != qt_xrootwin() )
	return FALSE;
    int i;
    if (tab_grab){
	XModifierKeymap* xmk = XGetModifierMapping(qt_xdisplay());
	for (i=0; i<xmk->max_keypermod; i++)
	    if (xmk->modifiermap[xmk->max_keypermod * Mod1MapIndex + i]
		== key.keycode){
		XUngrabKeyboard(qt_xdisplay(), kwin_time);
		XUngrabPointer( qt_xdisplay(), kwin_time);
		tab_box->hide();
		keys->setEnabled( TRUE );
		tab_grab = false;
 		if ( tab_box->currentClient() ){

		    activateClient( tab_box->currentClient() );
 		}
	    }
	XFreeModifiermap(xmk);
    }
    if (control_grab){
	XModifierKeymap* xmk = XGetModifierMapping(qt_xdisplay());
	for (i=0; i<xmk->max_keypermod; i++)
	    if (xmk->modifiermap[xmk->max_keypermod * ControlMapIndex + i]
		== key.keycode){
		XUngrabPointer( qt_xdisplay(), kwin_time);
		XUngrabKeyboard(qt_xdisplay(), kwin_time);
		tab_box->hide();
		keys->setEnabled( TRUE );
		control_grab = False;
		if ( tab_box->currentDesktop() != -1 )
		    setCurrentDesktop( tab_box->currentDesktop() );
	    }
	XFreeModifiermap(xmk);
    }
    return FALSE;
}

/*!
  auxiliary functions to travers all clients according the focus
  order. Useful for kwm´s Alt-tab feature.
*/
Client* Workspace::nextClient( Client* c ) const
{
    if ( focus_chain.isEmpty() )
	return 0;
    ClientList::ConstIterator it = focus_chain.find( c );
    if ( it == focus_chain.end() )
	return focus_chain.last();
    if ( it == focus_chain.begin() )
	return focus_chain.last();
    --it;
    return *it;
}

/*!
  auxiliary functions to travers all clients according the focus
  order. Useful for kwm´s Alt-tab feature.
*/
Client* Workspace::previousClient( Client* c ) const
{
    if ( focus_chain.isEmpty() )
	return 0;
    ClientList::ConstIterator it = focus_chain.find( c );
    if ( it == focus_chain.end() )
	return focus_chain.first();
    ++it;
    if ( it == focus_chain.end() )
	return focus_chain.first();
    return *it;
}

/*!
  auxiliary functions to travers all clients according the static
  order. Useful for the CDE-style Alt-tab feature.
*/
Client* Workspace::nextStaticClient( Client* c ) const
{
    if ( clients.isEmpty() )
	return 0;
    ClientList::ConstIterator it = clients.find( c );
    if ( it == clients.end() )
	return clients.first();
    ++it;
    if ( it == clients.end() )
	return clients.first();
    return *it;
}
/*!
  auxiliary functions to travers all clients according the static
  order. Useful for the CDE-style Alt-tab feature.
*/
Client* Workspace::previousStaticClient( Client* c ) const
{
    if ( clients.isEmpty() )
	return 0;
    ClientList::ConstIterator it = clients.find( c );
    if ( it == clients.end() )
	return clients.last();
    if ( it == clients.begin() )
	return clients.last();
    --it;
    return *it;
}


/*!
  Returns topmost visible client. Windows on the dock, the desktop
  or of any other special kind are excluded.
 */
Client* Workspace::topClientOnDesktop() const
{
    if ( most_recently_raised && stacking_order.contains( most_recently_raised ) &&
	 most_recently_raised->isVisible() )
	return most_recently_raised;

    for ( ClientList::ConstIterator it = stacking_order.fromLast(); it != stacking_order.end(); --it) {
	if ( !(*it)->isDesktop() && (*it)->isVisible() && (*it)->wantsTabFocus() )
	    return *it;
    }
    return 0;
}

/*
  Grabs the keysymbol \a keysym with the given modifiers \a mod
  plus all possibile combinations of Lock and NumLock
 */
void Workspace::grabKey(KeySym keysym, unsigned int mod){
  static int NumLockMask = 0;
  if (!keysym||!XKeysymToKeycode(qt_xdisplay(), keysym)) return;
  if (!NumLockMask){
    XModifierKeymap* xmk = XGetModifierMapping(qt_xdisplay());
    int i;
    for (i=0; i<8; i++){
      if (xmk->modifiermap[xmk->max_keypermod * i] ==
	  XKeysymToKeycode(qt_xdisplay(), XK_Num_Lock))
	NumLockMask = (1<<i);
    }
    XFreeModifiermap(xmk);
  }
  XGrabKey(qt_xdisplay(),
	   XKeysymToKeycode(qt_xdisplay(), keysym), mod,
	   qt_xrootwin(), FALSE,
	   GrabModeAsync, GrabModeSync);
  XGrabKey(qt_xdisplay(),
	   XKeysymToKeycode(qt_xdisplay(), keysym), mod | LockMask,
	   qt_xrootwin(), FALSE,
	   GrabModeAsync, GrabModeSync);
  XGrabKey(qt_xdisplay(),
	   XKeysymToKeycode(qt_xdisplay(), keysym), mod | NumLockMask,
	   qt_xrootwin(), FALSE,
	   GrabModeAsync, GrabModeSync);
  XGrabKey(qt_xdisplay(),
	   XKeysymToKeycode(qt_xdisplay(), keysym), mod | LockMask | NumLockMask,
	   qt_xrootwin(), FALSE,
	   GrabModeAsync, GrabModeSync);

}

/*!
  Informs the workspace about the active client, i.e. the client that
  has the focus (or None if no client has the focus). This functions
  is called by the client itself that gets focus. It has no other
  effect than fixing the focus chain and the return value of
  activeClient(). And of course, to propagate the active client to the
  world.
 */
void Workspace::setActiveClient( Client* c )
{
    if ( active_client == c )
	return;
    if ( active_client )
	active_client->setActive( FALSE );
    active_client = c;
    if ( active_client ) {
	focus_chain.remove( c );
	if ( c->wantsTabFocus() )
	    focus_chain.append( c );
    }

    // toplevel menubar handling
    Client* main = 0;
    if ( active_client )
	main = active_client->mainClient();

    // show the new menu bar first...
    Client* menubar = 0;
    for ( ClientList::ConstIterator it = clients.begin(); it != clients.end(); ++it) {
	if ( (*it)->isMenu() && (*it)->mainClient() == main ) {
	    menubar = *it;
	    break;
	}
    }
    if ( !menubar && desktop_client ) {
	for ( ClientList::ConstIterator it = clients.begin(); it != clients.end(); ++it) {
	    if ( (*it)->isMenu() && (*it)->mainClient() == desktop_client ) {
		menubar = *it;
		break;
	    }
	}
    }

    if ( menubar ) {
	menubar->show();
	raiseClient( menubar );
    }

    // ... then hide the other ones. Avoids flickers.
    for ( ClientList::ConstIterator it = clients.begin(); it != clients.end(); ++it) {
	if ( (*it)->isMenu() && (*it) != menubar )
	    (*it)->hide();
    }

    rootInfo->setActiveWindow( active_client? active_client->window() : 0 );
    updateColormap();
}


/*!
  Tries to activate the client \a c. This function performs what you
  expect when clicking the respective entry in a taskbar: showing and
  raising the client (this may imply switching to the another virtual
  desktop) and putting the focus onto it. Once X really gave focus to
  the client window as requested, the client itself will call
  setActiveClient() and the operation is complete. This may not happen
  with certain focus policies, though.

  \sa setActiveClient(), requestFocus()
 */
void Workspace::activateClient( Client* c, bool force )
{
    if (!c->isOnDesktop(currentDesktop()) ) {
	setCurrentDesktop( c->desktop() );
    }
    raiseClient( c );
    if ( c->isIconified() )
	Events::raise( Events::DeIconify );
    c->show();
    iconifyOrDeiconifyTransientsOf( c );
    if ( options->focusPolicyIsReasonable() ) {
	requestFocus( c, force );
    }
}

void Workspace::iconifyOrDeiconifyTransientsOf( Client* c )
{
    if ( c->isIconified() || c->isShade() ) {
	for ( ClientList::ConstIterator it = clients.begin(); it != clients.end(); ++it) {
	    if ( (*it)->transientFor() == c->window()
		 && !(*it)->isIconified()
		 && !(*it)->isShade() ) {
		(*it)->setMappingState( XIconicState );
		(*it)->hide();
		iconifyOrDeiconifyTransientsOf( (*it) );
	    }
	}
    } else {
	for ( ClientList::ConstIterator it = clients.begin(); it != clients.end(); ++it) {
	    if ( (*it)->transientFor() == c->window() && !(*it)->isVisible() ) {
		(*it)->show();
		iconifyOrDeiconifyTransientsOf( (*it) );
	    }
	}
    }
}



/*!
  Sets the client \a c's transient windows' sticky property to \a sticky.
 */
void Workspace::setStickyTransientsOf( Client* c, bool sticky )
{
    for ( ClientList::ConstIterator it = clients.begin(); it != clients.end(); ++it) {
	if ( (*it)->transientFor() == c->window() && (*it)->isSticky() != sticky )
	    (*it)->setSticky( sticky );
    }
}

/*!
  Returns whether a client with the specified \a caption exists, or not.
 */
bool Workspace::hasCaption( const QString& caption )
{
    for ( ClientList::ConstIterator it = clients.begin(); it != clients.end(); ++it) {
	if ( (*it)->caption() == caption )
	    return TRUE;
    }
    return FALSE;
}


/*!
  Tries to activate the client by asking X for the input focus. This
  function does not perform any show, raise or desktop switching. See
  Workspace::activateClient() instead.

  \sa Workspace::activateClient()
 */
void Workspace::requestFocus( Client* c, bool force )
{
    if (!focusChangeEnabled())
	return;

    //TODO will be different for non-root clients. (subclassing?)
    if ( !c ) {
	focusToNull();
	return;
    }

    if ( !popup || !popup->isVisible() )
	popup_client = c;

    if ( c->isVisible() && !c->isShade() ) {
	c->takeFocus( force );
	should_get_focus = c;
	focus_chain.remove( c );
	if ( c->wantsTabFocus() )
	    focus_chain.append( c );
    } else if ( c->isShade() ) {
	// client cannot accept focus, but at least the window should be active (window menu, et. al. )
	focusToNull();
	if ( c->wantsInput() )
	    c->setActive( TRUE );
    }
}


/*!
  Updates the current colormap according to the currently active client
 */
void Workspace::updateColormap()
{
    Colormap cmap = default_colormap;
    if ( activeClient() && activeClient()->colormap() != None )
	cmap = activeClient()->colormap();
    if ( cmap != installed_colormap ) {
	XInstallColormap(qt_xdisplay(), cmap );
	installed_colormap = cmap;
    }
}



/*!
  Informs the workspace that the client \a c has been hidden. If it
  was the active client, the workspace activates another one.

  \a c may already be destroyed
 */
void Workspace::clientHidden( Client* c )
{
    if ( c != active_client && ( active_client ||  c != should_get_focus ) )
	return;

    active_client = 0;
    should_get_focus = 0;
    if (!block_focus ) {
	if ( c->wantsTabFocus() && focus_chain.contains( c ) ) {
	    focus_chain.remove( c );
	    focus_chain.prepend( c );
	}
	if ( options->focusPolicyIsReasonable() && !focus_chain.isEmpty() ) {
	    for (ClientList::ConstIterator it = focus_chain.fromLast();
		 it != focus_chain.end();
		 --it) {
		if ((*it)->isVisible()) {
		    requestFocus(*it);
		    return;
		}
	    }
	}
    }
    if ( desktop_client )
	requestFocus( desktop_client );
    else
	focusToNull();
}


QPopupMenu* Workspace::clientPopup( Client* c )
{
    popup_client = c;
    if ( !popup ) {
	popup = new QPopupMenu;
	popup->setCheckable( TRUE );
	popup->setFont(KGlobalSettings::menuFont());
	connect( popup, SIGNAL( aboutToShow() ), this, SLOT( clientPopupAboutToShow() ) );
	connect( popup, SIGNAL( activated(int) ), this, SLOT( clientPopupActivated(int) ) );

	PluginMenu *deco = new PluginMenu(&mgr, popup);
	deco->setFont(KGlobalSettings::menuFont());

	desk_popup = new QPopupMenu( popup );
	desk_popup->setCheckable( TRUE );
	desk_popup->setFont(KGlobalSettings::menuFont());
	connect( desk_popup, SIGNAL( activated(int) ), this, SLOT( sendToDesktop(int) ) );
	connect( desk_popup, SIGNAL( aboutToShow() ), this, SLOT( desktopPopupAboutToShow() ) );

	popup->insertItem( i18n("&Move"), Options::MoveOp );
	popup->insertItem( i18n("&Size"), Options::ResizeOp );
	popup->insertItem( i18n("Mi&nimize"), Options::IconifyOp );
	popup->insertItem( i18n("Ma&ximize"), Options::MaximizeOp );
	popup->insertItem( i18n("Sh&ade"), Options::ShadeOp );
	popup->insertItem( i18n("Always &On Top"), Options::StaysOnTopOp );

	popup->insertSeparator();

    	popup->insertItem(i18n("&Decoration"), deco );
    	popup->insertItem(i18n("&To desktop"), desk_popup );

	popup->insertSeparator();

	QString k = KAccel::keyToString( keys->currentKey( "Window close" ), true );
	popup->insertItem(i18n("&Close")+'\t'+k, Options::CloseOp );
    }
    return popup;
}

void Workspace::performWindowOperation( Client* c, Options::WindowOperation op ) {
    if ( !c )
	return;

    switch ( op ) {
    case Options::MoveOp:
	c->performMouseCommand( Options::MouseMove, QCursor::pos() );
	break;
    case Options::ResizeOp:
	c->performMouseCommand( Options::MouseResize, QCursor::pos() );
	break;
    case Options::CloseOp:
	c->closeWindow();
	break;
    case Options::MaximizeOp:
	c->maximize();
	break;
    case Options::IconifyOp:
	c->iconify();
	break;
    case Options::ShadeOp:
	c->setShade( !c->isShade() );
	break;
    case Options::StaysOnTopOp:
	c->setStaysOnTop( !c->staysOnTop() );
	raiseClient( c );
	break;
    default:
	break;
    }
}

void Workspace::clientPopupActivated( int id )
{
    performWindowOperation( popup_client, (Options::WindowOperation) id );
}

/*!
  Places the client \a c according to the workspace's layout policy
 */
void Workspace::doPlacement( Client* c )
{
  if (options->placement == Options::Random)
    randomPlacement( c );
  else if (options->placement == Options::Smart)
    smartPlacement( c );
  else if (options->placement == Options::Cascade)
    cascadePlacement( c );
}

/*!
  Place the client \a c according to a simply "random" placement algorithm.
 */
void Workspace::randomPlacement(Client* c){
    const int step  = 24;
    static int px = step;
    static int py = 2 * step;
    int tx,ty;

    QRect maxRect = clientArea();

    if (px < maxRect.x())
	px = maxRect.x();
    if (py < maxRect.y())
	py = maxRect.y();

    px += step;
    py += 2*step;

    if (px > maxRect.width()/2)
	px =  maxRect.x() + step;
    if (py > maxRect.height()/2)
	py =  maxRect.y() + step;
    tx = px;
    ty = py;
    if (tx + c->width() > maxRect.right()){
	tx = maxRect.right() - c->width();
	if (tx < 0)
	    tx = 0;
	px =  maxRect.x();
    }
    if (ty + c->height() > maxRect.bottom()){
	ty = maxRect.bottom() - c->height();
	if (ty < 0)
	    ty = 0;
	py =  maxRect.y();
    }
    c->move( tx, ty );
}

/*!
  Place the client \a c according to a really smart placement algorithm :-)
*/
void Workspace::smartPlacement(Client* c){
    /*
     * SmartPlacement by Cristian Tibirna (tibirna@kde.org)
     * adapted for kwm (16-19jan98) and for kwin (16Nov1999) using (with
     * permission) ideas from fvwm, authored by
     * Anthony Martin (amartin@engr.csulb.edu).
     */

    const int none = 0, h_wrong = -1, w_wrong = -2; // overlap types
    long int overlap, min_overlap = 0;
    int x_optimal, y_optimal;
    int possible;

    int cxl, cxr, cyt, cyb;     //temp coords
    int  xl,  xr,  yt,  yb;     //temp coords

    // get the maximum allowed windows space
    QRect maxRect = clientArea();
    int x = maxRect.left(), y = maxRect.top();
    x_optimal = x; y_optimal = y;

    //client gabarit
    int ch = c->height(), cw = c->width();

    bool first_pass = true; //CT lame flag. Don't like it. What else would do?

    //loop over possible positions
    do {
	//test if enough room in x and y directions
	if ( y + ch > maxRect.bottom() && ch <= maxRect.height())
	    overlap = h_wrong; // this throws the algorithm to an exit
	else if( x + cw > maxRect.right() )
	    overlap = w_wrong;
	else {
	    overlap = none; //initialize

	    cxl = x; cxr = x + cw;
	    cyt = y; cyb = y + ch;
	    QValueList<Client*>::ConstIterator l;
	    for(l = clients.begin(); l != clients.end() ; ++l ) {
		if((*l)->isOnDesktop(currentDesktop()) && (*l) != desktop_client &&
		   !(*l)->isIconified() && (*l) != c ) {

		    xl = (*l)->x();          yt = (*l)->y();
		    xr = xl + (*l)->width(); yb = yt + (*l)->height();

		    //if windows overlap, calc the overall overlapping
		    if((cxl < xr) && (cxr > xl) &&
		       (cyt < yb) && (cyb > yt)) {
			xl = QMAX(cxl, xl); xr = QMIN(cxr, xr);
			yt = QMAX(cyt, yt); yb = QMIN(cyb, yb);
			overlap += (xr - xl) * (yb - yt);
		    }
		}
	    }
	}

	//CT first time we get no overlap we stop.
	if (overlap == none) {
	    x_optimal = x;
	    y_optimal = y;
	    break;
	}

	if (first_pass) {
	    first_pass = false;
	    min_overlap = overlap;
	}
	//CT save the best position and the minimum overlap up to now
	else if ( overlap >= none && overlap < min_overlap) {
	    min_overlap = overlap;
	    x_optimal = x;
	    y_optimal = y;
	}

	// really need to loop? test if there's any overlap
	if ( overlap > none ) {

	    possible = maxRect.right();
	    if ( possible - cw > x) possible -= cw;

	    // compare to the position of each client on the current desk
	    QValueList<Client*>::ConstIterator l;
	    for(l = clients.begin(); l != clients.end() ; ++l) {

		if ( (*l)->isOnDesktop(currentDesktop()) && (*l) != desktop_client &&
		     !(*l)->isIconified() &&  (*l) != c ) {

		    xl = (*l)->x();          yt = (*l)->y();
		    xr = xl + (*l)->width(); yb = yt + (*l)->height();

		    // if not enough room above or under the current tested client
		    // determine the first non-overlapped x position
		    if( ( y < yb ) && ( yt < ch + y ) ) {

			if( xr > x )
			    possible = possible < xr ? possible : xr;

			if( xl - cw > x )
			    possible = possible < xl - cw ? possible : xl - cw;
		    }
		}
	    }
	    x = possible;
	}

	// ... else ==> not enough x dimension (overlap was wrong on horizontal)
	else if ( overlap == w_wrong ) {
	    x = maxRect.left();
	    possible = maxRect.bottom();

	    if ( possible - ch > y ) possible -= ch;

	    //test the position of each window on current desk
	    QValueList<Client*>::ConstIterator l;
	    for( l = clients.begin(); l != clients.end() ; ++l ) {
		if( (*l)->isOnDesktop( currentDesktop() ) && (*l) != desktop_client &&
		    (*l) != c   &&  !c->isIconified() ) {

		    xl = (*l)->x();          yt = (*l)->y();
		    xr = xl + (*l)->width(); yb = yt + (*l)->height();

		    if( yb > y)
			possible = possible < yb ? possible : yb;

		    if( yt - ch > y )
			possible = possible < yt - ch ? possible : yt - ch;
		}
	    }
	    y = possible;
	}
    }
    while( overlap != none && overlap != h_wrong && y< maxRect.bottom() );

    if(ch>= maxRect.height() )
        y_optimal=maxRect.top();

    // place the window
    c->move( x_optimal, y_optimal );

}

/*!
  Place windows in a cascading order, remembering positions for each desktop
*/
void Workspace::cascadePlacement (Client* c, bool re_init) {
    /* cascadePlacement by Cristian Tibirna (tibirna@kde.org) (30Jan98)
 */
  // work coords
    int xp, yp;

    //CT how do I get from the 'Client' class the size that NW squarish "handle"
    int delta_x = 24;
    int delta_y = 24;

    int d = currentDesktop() - 1;

    // get the maximum allowed windows space and desk's origin
    //    (CT 20Nov1999 - is this common to all desktops?)
    QRect maxRect = clientArea();

  // initialize often used vars: width and height of c; we gain speed
    int ch = c->height();
    int cw = c->width();
    int H = maxRect.bottom();
    int W = maxRect.right();
    int X = maxRect.left();
    int Y = maxRect.top();

  //initialize if needed
    if (re_init) {
	cci[d].pos = QPoint(X, Y);
	cci[d].col = cci[d].row = 0;
    }


    xp = cci[d].pos.x();
    yp = cci[d].pos.y();

    //here to touch in case people vote for resize on placement
    if ((yp + ch ) > H) yp = Y;

    if ((xp + cw ) > W)
	if (!yp) {
	    smartPlacement(c);
	    return;
	}
	else xp = X;

  //if this isn't the first window
    if ( cci[d].pos.x() != X && cci[d].pos.y() != Y ) {
		/* The following statements cause an internal compiler error with
		 * egcs-2.91.66 on SuSE Linux 6.3. The equivalent forms compile fine.
		 * 22-Dec-1999 CS
		 *
		 * if ( xp != X && yp == Y ) xp = delta_x * (++(cci[d].col));
		 * if ( yp != Y && xp == X ) yp = delta_y * (++(cci[d].row));
		 */
	if ( xp != X && yp == Y )
	{
		++(cci[d].col);
		xp = delta_x * cci[d].col;
	}
	if ( yp != Y && xp == X )
	{
		++(cci[d].row);
		yp = delta_y * cci[d].row;
	}

	// last resort: if still doesn't fit, smart place it
	if ( ((xp + cw) > W - X) || ((yp + ch) > H - Y) ) {
	    smartPlacement(c);
	    return;
	}
    }

    // place the window
    c->move( QPoint( xp, yp ) );

    // new position
    cci[d].pos = QPoint( xp + delta_x,  yp + delta_y );
}


/*!
  Cascades all clients on the current desktop
 */
void Workspace::cascadeDesktop()
{
  ClientList::Iterator it(clients.fromLast());
  for (; it != clients.end(); --it) {
    if((!(*it)->isOnDesktop(currentDesktop())) ||
       ((*it)->isIconified())                  ||
       ((*it)->isSticky())                     ||
       (!(*it)->isMovable()) )
      continue;
    cascadePlacement(*it);
  }
}

/*!
  Unclutters the current desktop by smart-placing all clients
  again.
 */
void Workspace::unclutterDesktop()
{
  ClientList::Iterator it(clients.fromLast());
  for (; it != clients.end(); --it) {
    if((!(*it)->isOnDesktop(currentDesktop())) ||
       ((*it)->isIconified())                  ||
       ((*it)->isSticky())                     ||
       (!(*it)->isMovable()) )
      continue;
    smartPlacement(*it);
  }
}


/*!
  Reread settings
 */
void Workspace::reconfigure()
{
    KGlobal::config()->reparseConfiguration();
    options->reload();
    keys->readSettings();
}


/*!
  avoids managing a window with title \a title
 */
void Workspace::doNotManage( QString title )
{
    doNotManageList.append( title );
}

/*!
  Hack for java applets
 */
bool Workspace::isNotManaged( const QString& title )
{
    for ( QStringList::Iterator it = doNotManageList.begin(); it != doNotManageList.end(); ++it ) {
	if ( (*it) == title ) {
	    doNotManageList.remove( it );
	    return TRUE;
	}
    }
    return FALSE;
}

/*!
  Lowers the client \a c taking stays-on-top flags, layers,
  transient windows and window groups into account.
 */
void Workspace::lowerClient( Client* c )
{
    if ( !c )
	return;

    if ( c == desktop_client )
	return; // deny

    ClientList saveset;

    if ( c->transientFor() ) {
	/* search for a non-transient managed window, which this client
	   is transient to (possibly over many ways) */
	saveset.append( c );
	Client* t = findClient( c->transientFor() );
	Client* tmp;
	while ( t && !saveset.contains( t ) && t->transientFor() ) {
	    tmp = findClient( t->transientFor() );
	    if ( !tmp )
		break;
	    saveset.append( t );
	    t = tmp;
	}
	if ( t && !saveset.contains( t ) && t != desktop_client ) {
	    lowerClient( t );
	    return;
	}
    }

    saveset.clear();
    saveset.append( c );
    lowerTransientsOf(saveset, c );
    stacking_order.remove(c);
    stacking_order.prepend(c);

    stacking_order = constrainedStackingOrder( stacking_order );
    Window* new_stack = new Window[ stacking_order.count() + 1 ];
    int i = 0;
    for ( ClientList::ConstIterator it = stacking_order.fromLast(); it != stacking_order.end(); --it) {
	new_stack[i++] = (*it)->winId();
    }
    XRaiseWindow(qt_xdisplay(), new_stack[0]);
    XRestackWindows(qt_xdisplay(), new_stack, i);
    delete [] new_stack;

    propagateClients( TRUE );

    if ( c == most_recently_raised )
	most_recently_raised = 0;

}


/*!
  Raises the client \a c taking layers, stays-on-top flags,
  transient windows and window groups into account.
 */
void Workspace::raiseClient( Client* c )
{
    if ( !c )
	return;

    ClientList saveset;

    if ( c == desktop_client ) {
	saveset.clear();
	saveset.append( c );
	raiseTransientsOf(saveset, c );
	return; // deny
    }

    most_recently_raised = c;

    stacking_order.remove( c );
    stacking_order.append( c );

    if ( c->transientFor() ) {
	saveset.append( c );
	Client* t = findClient( c->transientFor() );
	Client* tmp;
	while ( t && !saveset.contains( t ) && t->transientFor() ) {
	    tmp = findClient( t->transientFor() );
	    if ( !tmp )
		break;
	    saveset.append( t );
	    t = tmp;
	}
	if ( t && !saveset.contains( t ) && t != desktop_client ) {
	    raiseClient( t );
	    most_recently_raised = c;
	    return;
	}
    }

    saveset.clear();
    saveset.append( c );
    raiseTransientsOf(saveset, c );

    stacking_order = constrainedStackingOrder( stacking_order );

    /* workaround to help broken full-screen applications to keep (modal) dialogs visible
     */
    if ( c->isTransient() && c->mainClient() == c ) {
	bool has_full_screen = false;
	for ( ClientList::ConstIterator it = stacking_order.fromLast(); it != stacking_order.end(); --it) {
	    if ( (*it) ==  c )
		break;
	    if ( (*it)->isVisible() && (*it)->isFullScreen() &&
		 !(*it)->isDesktop() && (*it)->staysOnTop() ) {
		has_full_screen = true;
		break;
	    }
	}
	if ( has_full_screen ) {
	    stacking_order.remove( c );
	    stacking_order.append( c );
	    saveset.clear();
	    saveset.append( c );
	    raiseTransientsOf( saveset, c);
	}
    }
    /* end workaround */

    Window* new_stack = new Window[ stacking_order.count() + 1 ];
    int i = 0;
    for ( ClientList::ConstIterator it = stacking_order.fromLast(); it != stacking_order.end(); --it) {
	new_stack[i++] = (*it)->winId();
    }
    XRaiseWindow(qt_xdisplay(), new_stack[0]);
    XRestackWindows(qt_xdisplay(), new_stack, i);
    delete [] new_stack;


    propagateClients( TRUE );

    if ( tab_box->isVisible() )
	tab_box->raise();

}


/*!
  Private auxiliary function used in raiseClient()
 */
void Workspace::raiseTransientsOf( ClientList& safeset, Client* c )
{
    ClientList local = stacking_order;
    for ( ClientList::ConstIterator it = local.begin(); it != local.end(); ++it) {
	if ( (*it)->transientFor() == c->window() && !safeset.contains( *it ) ) {
	    safeset.append( *it );
	    stacking_order.remove( *it );
	    stacking_order.append( *it );
	    raiseTransientsOf( safeset, *it );
	}
    }
}

/*!
  Private auxiliary function used in lowerClient()
 */
void Workspace::lowerTransientsOf( ClientList& safeset, Client* c )
{
    ClientList local = stacking_order;
    for ( ClientList::ConstIterator it = local.fromLast(); it!=local.end(); --it) {
	if ((*it)->transientFor() == c->window() && !safeset.contains(*it)) {
	    safeset.append( *it );
	    lowerTransientsOf( safeset, *it );
	    stacking_order.remove( *it );
	    stacking_order.prepend( *it );
	}
    }
}



/*!
  Returns a stacking order based upon \a list that fulfills certain contained.

  Currently it obeyes the staysOnTop flag only.

  \sa Client::staysOnTop()
 */
ClientList Workspace::constrainedStackingOrder( const ClientList& list )
{
    ClientList result;
    ClientList::ConstIterator it;
    for (  it = list.begin(); it!=list.end(); ++it) {
	if ( !(*it)->staysOnTop() && !(*it)->mainClient()->staysOnTop())
	    result.append( *it );
    }
    for ( it = list.begin(); it!=list.end(); ++it) {
	if ( (*it)->staysOnTop() || (*it)->mainClient()->staysOnTop() )
	    result.append( *it );
    }
    return result;
}

/*!
  Puts the focus on a dummy window
 */
void Workspace::focusToNull(){
  static Window w = 0;
  int mask;
  XSetWindowAttributes attr;
  if (w == 0) {
    mask = CWOverrideRedirect;
    attr.override_redirect = 1;
    w = XCreateWindow(qt_xdisplay(), qt_xrootwin(), 0, 0, 1, 1, 0, CopyFromParent,
		      InputOnly, CopyFromParent, mask, &attr);
    XMapWindow(qt_xdisplay(), w);
  }
  XSetInputFocus(qt_xdisplay(), w, RevertToPointerRoot, kwin_time );
  updateColormap();
}


/*!
  Declares client \a c to be the desktop.
 */
void Workspace::setDesktopClient( Client* c)
{
    desktop_client = c;
    if ( desktop_client ) {
	desktop_client->lower();
	desktop_client->setGeometry( geometry() );
    }
}


/*!
  Sets the current desktop to \a new_desktop

  Shows/Hides windows according to the stacking order and finally
  propages the new desktop to the world
 */
void Workspace::setCurrentDesktop( int new_desktop ){
    if (new_desktop < 1 || new_desktop > number_of_desktops )
	return;

    Client* old_active_client = active_client;
    active_client = 0;
    block_focus = TRUE;
    
    ClientList mapList;
    ClientList unmapList;

    if (new_desktop != current_desktop) {
	/*
	  optimized Desktop switching: unmapping done from back to front
	  mapping done from front to back => less exposure events
	*/

	for ( ClientList::ConstIterator it = stacking_order.begin(); it != stacking_order.end(); ++it) {
	    if ( (*it)->isVisible() && !(*it)->isOnDesktop( new_desktop ) ) {
		(*it)->hide();
		unmapList += (*it);
	    }
	}
	current_desktop = new_desktop;
	rootInfo->setCurrentDesktop( current_desktop ); // propagate befor the shows below
	
	for ( ClientList::ConstIterator it = stacking_order.fromLast(); it != stacking_order.end(); --it) {
	    if ( (*it)->isOnDesktop( new_desktop ) && !(*it)->isIconified() ) {
		(*it)->show();
		mapList += (*it);
	    }
	}
    }
    current_desktop = new_desktop;

    rootInfo->setCurrentDesktop( current_desktop );

    // restore the focus on this desktop
    block_focus = FALSE;
    Client* c = 0;

    if ( options->focusPolicyIsReasonable()) {
	// Search in focus chain
	
	if ( focus_chain.contains( old_active_client ) && old_active_client->isVisible() ) {
	    c = old_active_client;
	    active_client = c; // the requestFocus below will fail, as the client is already active
	}
	
	if ( !c ) {
	    for( ClientList::ConstIterator it = focus_chain.fromLast(); it != focus_chain.end(); --it) {
		if ( (*it)->isVisible() && !(*it)->isSticky() ) {
		    c = *it;
		    break;	
		}
	    }
	}
	
	if ( !c ) {
	    for( ClientList::ConstIterator it = focus_chain.fromLast(); it != focus_chain.end(); --it) {
		if ( (*it)->isVisible() ) {
		    c = *it;
		    break;	
		}
	    }
	}

	if (!c) {
	    // Search top-most visible window
	    for ( ClientList::ConstIterator it = stacking_order.fromLast(); it != stacking_order.end(); --it) {
		if ( (*it)->isVisible() ) {
		    c = *it;
		    break;
		}
	    }
	}
    }

    if ( c ) {
	requestFocus( c );
	// don't let the panel cover fullscreen windows on desktop switches
	if ( c->isFullScreen() && !c->isDesktop() && c->staysOnTop() )
	    raiseClient( c );
    } else {
	focusToNull();
    }

    QApplication::syncX();
    XEvent tmpE;
    for ( ClientList::ConstIterator it = mapList.begin(); it != mapList.end(); ++it) {
	while ( XCheckTypedWindowEvent( qt_xdisplay(), (*it)->windowWrapper()->winId(), UnmapNotify, &tmpE ) )
	    ; // eat event
    }
    for ( ClientList::ConstIterator it = unmapList.begin(); it != unmapList.end(); ++it) {
	while ( XCheckTypedWindowEvent( qt_xdisplay(), (*it)->windowWrapper()->winId(), MapNotify, &tmpE ) )
	    ; // eat event
    }
}



/*!
  Returns the workspace's desktop widget. The desktop widget is
  sometimes required by clients to draw on it, for example outlines on
  moving or resizing.
 */
QWidget* Workspace::desktopWidget()
{
    return desktop_widget;
}

/*!
  Sets the number of virtual desktops to \a n
 */
void Workspace::setNumberOfDesktops( int n )
{
    if ( n == number_of_desktops )
	return;
    number_of_desktops = n;
    rootInfo->setNumberOfDesktops( number_of_desktops );
    saveDesktopSettings();
}

/*!
  Handles client messages sent to the workspace
 */
bool Workspace::netCheck( XEvent* e )
{
    unsigned int dirty = rootInfo->event( e );

    if ( dirty & NET::DesktopNames )
	saveDesktopSettings();

    return FALSE;
}

/*!
  Propagates the managed clients to the world
 */
void Workspace::propagateClients( bool onlyStacking )
{
    WId* cl;
    int i;
    if ( !onlyStacking ) {
	cl = new WId[ clients.count()];
	i = 0;
	for ( ClientList::ConstIterator it = clients.begin(); it != clients.end(); ++it ) {
	    cl[i++] =  (*it)->window();
	}
	rootInfo->setClientList( (Window*) cl, i );
	delete [] cl;
    }

    cl = new WId[ stacking_order.count()];
    i = 0;
    for ( ClientList::ConstIterator it = stacking_order.begin(); it != stacking_order.end(); ++it) {
	cl[i++] =  (*it)->window();
    }
    rootInfo->setClientListStacking(  (Window*) cl, i );
    delete [] cl;
}



/*!
  Check whether \a w is a system tray window. If so, add it to the respective
  datastructures and propagate it to the world.
 */
bool Workspace::addSystemTrayWin( WId w )
{
    if ( systemTrayWins.contains( w ) )
	return TRUE;

    NETWinInfo ni( qt_xdisplay(), w, root, NET::WMKDESystemTrayWinFor );
    WId trayWinFor = ni.kdeSystemTrayWinFor();
    if ( !trayWinFor )
 	return FALSE;
    systemTrayWins.append( SystemTrayWindow( w, trayWinFor ) );
    XSelectInput( qt_xdisplay(), w,
		  StructureNotifyMask
		  );
    XAddToSaveSet( qt_xdisplay(), w );
    propagateSystemTrayWins();
    return TRUE;
}

/*!
  Check whether \a w is a system tray window. If so, remove it from
  the respective datastructures and propagate this to the world.
 */
bool Workspace::removeSystemTrayWin( WId w )
{
    if ( !systemTrayWins.contains( w ) )
	return FALSE;
    systemTrayWins.remove( w );
    propagateSystemTrayWins();
    return TRUE;
}


/*!
  Returns whether iconify means actually withdraw for client \a. This
  is TRUE for clients that have a docking window. In that case the
  docking window will serve as icon replacement.
 */
bool Workspace::iconifyMeansWithdraw( Client* c)
{
    for ( SystemTrayWindowList::ConstIterator it = systemTrayWins.begin(); it != systemTrayWins.end(); ++it ) {
	if ( (*it).winFor == c->window() )
	    return TRUE;
    }
    return FALSE;
}

/*!
  Propagates the systemTrayWins to the world
 */
void Workspace::propagateSystemTrayWins()
{
    WId* cl = new WId[ systemTrayWins.count()];
    int i = 0;
    for ( SystemTrayWindowList::ConstIterator it = systemTrayWins.begin(); it != systemTrayWins.end(); ++it ) {
	cl[i++] =  (*it).win;
    }

    rootInfo->setKDESystemTrayWindows( (Window*) cl, i );
    delete [] cl;
}


/*!
  Create the global accel object \c keys.
 */
void Workspace::createKeybindings(){
    keys = new KGlobalAccel();

#include "kwinbindings.cpp"

    keys->connectItem( "Switch to desktop 1", this, SLOT( slotSwitchDesktop1() ));
    keys->connectItem( "Switch to desktop 2", this, SLOT( slotSwitchDesktop2() ));
    keys->connectItem( "Switch to desktop 3", this, SLOT( slotSwitchDesktop3() ));
    keys->connectItem( "Switch to desktop 4", this, SLOT( slotSwitchDesktop4() ));
    keys->connectItem( "Switch to desktop 5", this, SLOT( slotSwitchDesktop5() ));
    keys->connectItem( "Switch to desktop 6", this, SLOT( slotSwitchDesktop6() ));
    keys->connectItem( "Switch to desktop 7", this, SLOT( slotSwitchDesktop7() ));
    keys->connectItem( "Switch to desktop 8", this, SLOT( slotSwitchDesktop8() ));
    keys->connectItem( "Switch to desktop 9", this, SLOT( slotSwitchDesktop9() ));
    keys->connectItem( "Switch to desktop 10", this, SLOT( slotSwitchDesktop10() ));
    keys->connectItem( "Switch to desktop 11", this, SLOT( slotSwitchDesktop11() ));
    keys->connectItem( "Switch to desktop 12", this, SLOT( slotSwitchDesktop12() ));
    keys->connectItem( "Switch to desktop 13", this, SLOT( slotSwitchDesktop13() ));
    keys->connectItem( "Switch to desktop 14", this, SLOT( slotSwitchDesktop14() ));
    keys->connectItem( "Switch to desktop 15", this, SLOT( slotSwitchDesktop15() ));
    keys->connectItem( "Switch to desktop 16", this, SLOT( slotSwitchDesktop16() ));
    keys->connectItem( "Switch desktop left", this, SLOT( slotSwitchDesktopLeft() ));
    keys->connectItem( "Switch desktop right", this, SLOT( slotSwitchDesktopRight() ));

    keys->connectItem( "Pop-up window operations menu", this, SLOT( slotWindowOperations() ) );
    keys->connectItem( "Window close", this, SLOT( slotWindowClose() ) );
    keys->connectItem( "Window maximize", this, SLOT( slotWindowMaximize() ) );
    keys->connectItem( "Window maximize horizontal", this, SLOT( slotWindowMaximizeHorizontal() ) );
    keys->connectItem( "Window maximize vertical", this, SLOT( slotWindowMaximizeVertical() ) );
    keys->connectItem( "Window iconify", this, SLOT( slotWindowIconify() ) );
    keys->connectItem( "Window shade", this, SLOT( slotWindowShade() ) );
    keys->connectItem( "Window move", this, SLOT( slotWindowMove() ) );
    keys->connectItem( "Window resize", this, SLOT( slotWindowResize() ) );
    keys->connectItem( "Window raise", this, SLOT( slotWindowRaise() ) );
    keys->connectItem( "Window lower", this, SLOT( slotWindowLower() ) );

    keys->connectItem( "Mouse emulation", this, SLOT( slotMouseEmulation() ) );

    keys->connectItem( "Logout", this, SLOT( slotLogout() ) );

    keys->connectItem( "Kill Window", this, SLOT( slotKillWindow() ) );
    keys->readSettings();
}

void Workspace::slotSwitchDesktop1(){
    setCurrentDesktop(1);
}
void Workspace::slotSwitchDesktop2(){
    setCurrentDesktop(2);
}
void Workspace::slotSwitchDesktop3(){
    setCurrentDesktop(3);
}
void Workspace::slotSwitchDesktop4(){
    setCurrentDesktop(4);
}
void Workspace::slotSwitchDesktop5(){
    setCurrentDesktop(5);
}
void Workspace::slotSwitchDesktop6(){
    setCurrentDesktop(6);
}
void Workspace::slotSwitchDesktop7(){
    setCurrentDesktop(7);
}
void Workspace::slotSwitchDesktop8(){
    setCurrentDesktop(8);
}
void Workspace::slotSwitchDesktop9(){
    setCurrentDesktop(9);
}
void Workspace::slotSwitchDesktop10(){
    setCurrentDesktop(10);
}
void Workspace::slotSwitchDesktop11(){
    setCurrentDesktop(11);
}
void Workspace::slotSwitchDesktop12(){
    setCurrentDesktop(12);
}
void Workspace::slotSwitchDesktop13(){
    setCurrentDesktop(13);
}
void Workspace::slotSwitchDesktop14(){
    setCurrentDesktop(14);
}
void Workspace::slotSwitchDesktop15(){
    setCurrentDesktop(15);
}
void Workspace::slotSwitchDesktop16(){
    setCurrentDesktop(16);
}
void Workspace::slotSwitchDesktopRight(){
  int d = currentDesktop() + 1;
  if ( d > number_of_desktops )
    d = 1;
  setCurrentDesktop(d);
}
void Workspace::slotSwitchDesktopLeft(){
  int d = currentDesktop() - 1;
  if ( d <= 0 )
    d = number_of_desktops;
  setCurrentDesktop(d);
}

/*!
  Maximizes the popup client
 */
void Workspace::slotWindowMaximize()
{
    if ( popup_client )
	popup_client->maximize( Client::MaximizeFull );
}

/*!
  Maximizes the popup client vertically
 */
void Workspace::slotWindowMaximizeVertical()
{
    if ( popup_client )
	popup_client->maximize( Client::MaximizeVertical );
}

/*!
  Maximizes the popup client horiozontally
 */
void Workspace::slotWindowMaximizeHorizontal()
{
    if ( popup_client )
	popup_client->maximize( Client::MaximizeHorizontal );
}


/*!
  Iconifies the popup client
 */
void Workspace::slotWindowIconify()
{
    performWindowOperation( popup_client, Options::IconifyOp );
}

/*!
  Shades/unshades the popup client respectively
 */
void Workspace::slotWindowShade()
{
    performWindowOperation( popup_client, Options::ShadeOp );
}

/*!
  Raises the popup client
 */
void Workspace::slotWindowRaise()
{
    if ( popup_client )
	raiseClient( popup_client );
}

/*!
  Lowers the popup client
 */
void Workspace::slotWindowLower()
{
    if ( popup_client )
	lowerClient( popup_client );
}


/*!
  Invokes keyboard mouse emulation
 */
void Workspace::slotMouseEmulation()
{

    if ( mouse_emulation ) {
	XUngrabKeyboard(qt_xdisplay(), kwin_time);
	mouse_emulation = FALSE;
	return;
    }

    if ( XGrabKeyboard(qt_xdisplay(),
		       root, FALSE,
		       GrabModeAsync, GrabModeAsync,
		       kwin_time) == GrabSuccess ) {
	mouse_emulation = TRUE;
	mouse_emulation_state = 0;
	mouse_emulation_window = 0;
    }
}

void Workspace::slotLogout()
{
  kapp->requestShutDown();
}


/*!
  Kill Window feature, similar to xkill
 */
void Workspace::slotKillWindow()
{
    KillWindow kill( this );
    kill.start();
}


/*!
  Kills the window at position \a x,  \a y
 */
void Workspace::killWindowAtPosition(int x, int y)
{
    ClientList::ConstIterator it(stacking_order.fromLast());
    for ( ; it != stacking_order.end(); --it)
    {
        Client *client = (*it);
        if ( client->frameGeometry().contains(QPoint(x, y)) &&
             client->isOnDesktop( currentDesktop() ) &&
             !client->isIconified() )
        {
            client->killWindow();
            return;
        }
    }
}

/*!
  Adjusts the desktop popup to the current values and the location of
  the popup client.
 */
void Workspace::desktopPopupAboutToShow()
{
    if ( !desk_popup )
	return;
    desk_popup->clear();
    desk_popup->insertItem( i18n("&All desktops"), 0 );
    if ( popup_client->isSticky() )
	desk_popup->setItemChecked( 0, TRUE );
    desk_popup->insertSeparator( -1 );
    int id;
    for ( int i = 1; i <= numberOfDesktops(); i++ ) {
	id = desk_popup->insertItem( QString("&%1  %2").arg(i).arg( desktopName(i) ), i );
	if ( popup_client && !popup_client->isSticky() && popup_client->desktop()  == i )
	    desk_popup->setItemChecked( id, TRUE );
    }
}


/*!
  The client popup menu will become visible soon.

  Adjust the items according to the respective popup client.
 */
void Workspace::clientPopupAboutToShow()
{
    if ( !popup_client || !popup )
	return;
    popup->setItemEnabled( Options::ResizeOp, popup_client->isResizable() );
    popup->setItemEnabled( Options::MoveOp, popup_client->isMovable() );
    popup->setItemEnabled( Options::MaximizeOp, popup_client->isMaximizable() );
    popup->setItemChecked( Options::MaximizeOp, popup_client->isMaximized() );
    popup->setItemChecked( Options::ShadeOp, popup_client->isShade() );
    popup->setItemChecked( Options::StaysOnTopOp, popup_client->staysOnTop() );
    popup->setItemEnabled( Options::IconifyOp, popup_client->isMinimizable() );
}


/*!
  Sends client \a c to desktop \a desk.

  Takes care of transients as well.
 */
void Workspace::sendClientToDesktop( Client* c, int desk )
{
    if ( c->isSticky() )
	c->setSticky( FALSE );

    if ( c->isOnDesktop( desk ) )
	return;

    c->setDesktop( desk );

    if ( c->isOnDesktop( currentDesktop() ) )
	c->show();
    else
	c->hide();

    for ( ClientList::ConstIterator it = clients.begin(); it != clients.end(); ++it) {
	if ( (*it)->transientFor() == c->window() ) {
	    sendClientToDesktop( *it, desk );
	}
    }
}

/*!
  Sends the popup client to desktop \a desk

  Internal slot for the window operation menu
 */
void Workspace::sendToDesktop( int desk )
{
    if ( !popup_client )
	return;
    if ( desk == 0 ) {
	popup_client->setSticky( !popup_client->isSticky() );
	return;
    }

    sendClientToDesktop( popup_client, desk );

}


/*!
  Shows the window operations popup menu for the activeClient()
 */
void Workspace::slotWindowOperations()
{
    if ( !active_client )
	return;
    if ( !active_client->isMovable())
        return;

    QPopupMenu* p = clientPopup( active_client );
    p->popup( active_client->mapToGlobal( active_client->windowWrapper()->geometry().topLeft() ) );
}


/*!
  Closes the popup client
 */
void Workspace::slotWindowClose()
{
    if ( tab_box->isVisible() )
	return;
    performWindowOperation( popup_client, Options::CloseOp );
}


/*!
  Starts keyboard move mode for the popup client
 */
void Workspace::slotWindowMove()
{
    performWindowOperation( popup_client, Options::MoveOp );
}

/*!
  Starts keyboard resize mode for the popup client
 */
void Workspace::slotWindowResize()
{
    performWindowOperation( popup_client, Options::ResizeOp );
}


/*!
  Client \a c is moved around to position \a pos. This gives the
  workspace the opportunity to interveniate and to implement
  snap-to-windows functionality.
 */
QPoint Workspace::adjustClientPosition( Client* c, QPoint pos )
{
  //CT 16mar98, 27May98 - magics: BorderSnapZone, WindowSnapZone
  //CT adapted for kwin on 25Nov1999
  if (options->windowSnapZone || options->borderSnapZone ) {

    int snap;        //snap trigger

    QRect maxRect = clientArea();
    int xmin = maxRect.left();
    int xmax = maxRect.right()+1;               //desk size
    int ymin = maxRect.top();
    int ymax = maxRect.bottom()+1;
    int cx, cy, rx, ry, cw, ch;                 //these don't change

    int nx, ny;                         //buffers
    int deltaX = xmax, deltaY = ymax;   //minimum distance to other clients

    int lx, ly, lrx, lry; //coords and size for the comparison client, l

    nx = cx = pos.x();
    ny = cy = pos.y();
    rx = cx + (cw = c->width());
    ry = cy + (ch = c->height());

    // border snap
    snap = options->borderSnapZone;
    if (snap) {
      if ( QABS(cx-xmin) < snap ){
	deltaX = QABS(cx - xmin);
	nx = xmin;
      }
      if ((QABS(xmax-rx) < snap) && (QABS(xmax-rx) < deltaX)) {
	deltaX = QABS(xmax-rx);
	nx = xmax - cw;
      }

      if ( QABS(cy-ymin) < snap ){
	deltaY = QABS(cy-ymin);
	ny = ymin;
      }
      if ((QABS(ymax-ry) < snap)  && (QABS(ymax-ry) < deltaY)) {
	deltaY = QABS(ymax-ry);
	ny = ymax - ch;
      }
    }

    // windows snap
    snap = options->windowSnapZone;
    if (snap) {
      QValueList<Client *>::ConstIterator l;
      for (l = clients.begin();l != clients.end();++l ) {
	if((*l)->isOnDesktop(currentDesktop()) && (*l) != desktop_client &&
	   !(*l)->isIconified() && (*l)->transientFor() == None &&
	   (*l) != c ) {
	  lx = (*l)->x();
	  ly = (*l)->y();
	  lrx = lx + (*l)->width();
	  lry = ly + (*l)->height();

	  if( ( ( cy <= lry ) && ( cy  >= ly  ) )  ||
	      ( ( ry >= ly  ) && ( ry  <= lry ) )  ||
	      ( ( ly >= cy  ) && ( lry <= ry  ) ) ) {
	    if ( ( QABS( lrx - cx ) < snap )   &&
		 ( QABS( lrx -cx ) < deltaX ) ) {
	      deltaX = QABS( lrx - cx );
	      nx = lrx;
	    }
	    if ( ( QABS( rx - lx ) < snap )    &&
		 ( QABS( rx - lx ) < deltaX ) ) {
	      deltaX = QABS(rx - lx);
	      nx = lx - cw;
	    }
	  }

	  if( ( ( cx <= lrx ) && ( cx  >= lx  ) )  ||
	      ( ( rx >= lx  ) && ( rx  <= lrx ) )  ||
	      ( ( lx >= cx  ) && ( lrx <= rx  ) ) ) {
	    if ( ( QABS( lry - cy ) < snap )   &&
		 ( QABS( lry -cy ) < deltaY ) ) {
	      deltaY = QABS( lry - cy );
	      ny = lry;
	    }
	    if ( ( QABS( ry-ly ) < snap )      &&
		 ( QABS( ry - ly ) < deltaY ) ) {
	      deltaY = QABS( ry - ly );
	      ny = ly - ch;
	    }
	  }
	}
      }
    }
    pos = QPoint(nx, ny);
  }
  return pos;
}



/*!
  Returns the child window under the mouse and activates the
  respective client if necessary.

  Auxiliary function for the mouse emulation system.
 */
WId Workspace::getMouseEmulationWindow()
{
    Window root;
    Window child = qt_xrootwin();
    int root_x, root_y, lx, ly;
    uint state;
    Window w;
    Client * c = 0;
    do {
	w = child;
	if (!c)
	    c = findClientWidthId( w );
	XQueryPointer( qt_xdisplay(), w, &root, &child,
		       &root_x, &root_y, &lx, &ly, &state );
    } while  ( child != None && child != w );

    if ( c && !c->isActive() )
	activateClient( c );
    return (WId) w;
}

/*!
  Sends a faked mouse event to the specified window. Returns the new button state.
 */
unsigned int Workspace::sendFakedMouseEvent( QPoint pos, WId w, MouseEmulation type, int button, unsigned int state )
{
    if ( !w )
	return state;
    QWidget* widget = QWidget::find( w );
    if ( (!widget ||  widget->inherits("QToolButton") ) && !findClient( w ) ) {
	int x, y;
	Window xw;
	XTranslateCoordinates( qt_xdisplay(), qt_xrootwin(), w, pos.x(), pos.y(), &x, &y, &xw );
	if ( type == EmuMove ) { // motion notify events
	    XMotionEvent e;
	    e.type = MotionNotify;
	    e.window = w;
	    e.root = qt_xrootwin();
	    e.subwindow = w;
	    e.time = kwin_time;
	    e.x = x;
	    e.y = y;
	    e.x_root = pos.x();
	    e.y_root = pos.y();
	    e.state = state;
	    e.is_hint = NotifyNormal;
	    XSendEvent( qt_xdisplay(), w, TRUE, ButtonMotionMask, (XEvent*)&e );
	} else {
	    XButtonEvent e;
	    e.type = type == EmuRelease ? ButtonRelease : ButtonPress;
	    e.window = w;
	    e.root = qt_xrootwin();
	    e.subwindow = w;
	    e.time = kwin_time;
	    e.x = x;
	    e.y = y;
	    e.x_root = pos.x();
	    e.y_root = pos.y();
	    e.state = state;
	    e.button = button;
	    XSendEvent( qt_xdisplay(), w, TRUE, ButtonPressMask, (XEvent*)&e );
	
	    if ( type == EmuPress ) {
		switch ( button ) {
		case 2:
		    state |= Button2Mask;
		    break;
		case 3:
		    state |= Button3Mask;
		    break;
		default: // 1
		    state |= Button1Mask;
		    break;
		}
	    } else {
		switch ( button ) {
		case 2:
		    state &= ~Button2Mask;
		    break;
		case 3:
		    state &= ~Button3Mask;
		    break;
		default: // 1
		    state &= ~Button1Mask;
		    break;
		}
	    }
	}
    }
    return state;
}

/*!
  Handles keypress event during mouse emulation
 */
bool Workspace::keyPressMouseEmulation( XKeyEvent key )
{
    if ( root != qt_xrootwin() )
	return FALSE;
    int kc = XKeycodeToKeysym(qt_xdisplay(), key.keycode, 0);
    int km = key.state & (ControlMask | Mod1Mask | ShiftMask);

    bool is_control = km & ControlMask;
    bool is_alt = km & Mod1Mask;
    bool is_shift = km & ShiftMask;
    int delta = is_control?1:is_alt?32:8;
    QPoint pos = QCursor::pos();

    switch ( kc ) {
    case XK_Left:
    case XK_KP_Left:
	pos.rx() -= delta;
	break;
    case XK_Right:
    case XK_KP_Right:
	pos.rx() += delta;
	break;
    case XK_Up:
    case XK_KP_Up:
	pos.ry() -= delta;
	break;
    case XK_Down:
    case XK_KP_Down:
	pos.ry() += delta;
	break;
    case XK_F1:
	if ( !mouse_emulation_state )
	    mouse_emulation_window = getMouseEmulationWindow();
	if ( (mouse_emulation_state & Button1Mask) == 0 )
	    mouse_emulation_state = sendFakedMouseEvent( pos, mouse_emulation_window, EmuPress, Button1, mouse_emulation_state );		
	if ( !is_shift )
	    mouse_emulation_state = sendFakedMouseEvent( pos, mouse_emulation_window, EmuRelease, Button1, mouse_emulation_state );		
	break;
    case XK_F2:
	if ( !mouse_emulation_state )
	    mouse_emulation_window = getMouseEmulationWindow();
	if ( (mouse_emulation_state & Button2Mask) == 0 )
	    mouse_emulation_state = sendFakedMouseEvent( pos, mouse_emulation_window, EmuPress, Button2, mouse_emulation_state );		
	if ( !is_shift )
	    mouse_emulation_state = sendFakedMouseEvent( pos, mouse_emulation_window, EmuRelease, Button2, mouse_emulation_state );		
	break;
    case XK_F3:
	if ( !mouse_emulation_state )
	    mouse_emulation_window = getMouseEmulationWindow();
	if ( (mouse_emulation_state & Button3Mask) == 0 )
	    mouse_emulation_state = sendFakedMouseEvent( pos, mouse_emulation_window, EmuPress, Button3, mouse_emulation_state );		
	if ( !is_shift )
	    mouse_emulation_state = sendFakedMouseEvent( pos, mouse_emulation_window, EmuRelease, Button3, mouse_emulation_state );		
	break;
    case XK_Return:
    case XK_space:
    case XK_KP_Enter:
    case XK_KP_Space:
	{
	    if ( !mouse_emulation_state ) {
		// nothing was pressed, fake a LMB click
		mouse_emulation_window = getMouseEmulationWindow();
		mouse_emulation_state = sendFakedMouseEvent( pos, mouse_emulation_window, EmuPress, Button1, mouse_emulation_state );		
		mouse_emulation_state = sendFakedMouseEvent( pos, mouse_emulation_window, EmuRelease, Button1, mouse_emulation_state );		
	    } else { // release all
		if ( mouse_emulation_state & Button1Mask )
		    mouse_emulation_state = sendFakedMouseEvent( pos, mouse_emulation_window, EmuRelease, Button1, mouse_emulation_state );
		if ( mouse_emulation_state & Button2Mask )
		    mouse_emulation_state = sendFakedMouseEvent( pos, mouse_emulation_window, EmuRelease, Button2, mouse_emulation_state );
		if ( mouse_emulation_state & Button3Mask )
		    mouse_emulation_state = sendFakedMouseEvent( pos, mouse_emulation_window, EmuRelease, Button3, mouse_emulation_state );
	    }
	}
	// fall through
    case XK_Escape:
	XUngrabKeyboard(qt_xdisplay(), kwin_time);
	mouse_emulation = FALSE;
	return TRUE;
    default:
	return FALSE;
    }

    QCursor::setPos( pos );
    if ( mouse_emulation_state )
	mouse_emulation_state = sendFakedMouseEvent( pos, mouse_emulation_window, EmuMove, 0,  mouse_emulation_state );		
    return TRUE;

}


/*!
  Puts a new decoration frame around every client. Used to react on
  style changes.
 */
void Workspace::slotResetAllClients()
{

    ClientList stack = stacking_order;
    Client* active = activeClient();
    block_focus = TRUE;
    Client* prev = 0;
    for (ClientList::Iterator it = stack.fromLast(); it != stack.end(); --it) {
	Client *oldClient = (*it);
	oldClient->hide();
	WId w = oldClient->window();
	XUnmapWindow( qt_xdisplay(), w );
	oldClient->releaseWindow();
	// Replace oldClient with newClient in all lists
	Client *newClient = clientFactory (w);
	if ( oldClient == active )
	    active = newClient;
	ClientList::Iterator jt = clients.find (oldClient);
	(*jt) = newClient;
	jt = stacking_order.find (oldClient);
	(*jt) = newClient;
	jt = focus_chain.find (oldClient);
	(*jt) = newClient;
	newClient->cloneMode(oldClient);
	delete oldClient;
	bool showIt = newClient->manage( TRUE, TRUE, FALSE );
	if ( prev ) {
	    Window stack[2];
	    stack[0] = prev->winId();;
	    stack[1] = newClient->winId();
	    XRestackWindows(  qt_xdisplay(), stack, 2 );
	}
	if ( showIt )
	    newClient->show();
	prev = newClient;
    }
    block_focus = FALSE;
    if ( active )
	requestFocus( active );
}

/*!
  Stores the current session in the config file

  \sa loadSessionInfo()
 */
void Workspace::storeSession( KConfig* config )
{
    config->setGroup("Session" );
    int count =  0;
    for (ClientList::Iterator it = clients.begin(); it != clients.end(); ++it) {
	Client* c = (*it);
	QCString sessionId = c->sessionId();
	QCString windowRole = c->windowRole();
	QCString wmCommand = c->wmCommand();
	if ( !sessionId.isEmpty() || !wmCommand.isEmpty() ) {
	    count++;
	    QString n = QString::number(count);
	    config->writeEntry( QString("sessionId")+n, sessionId.data() );
	    config->writeEntry( QString("windowRole")+n, windowRole.data() );
	    config->writeEntry( QString("wmCommand")+n, wmCommand.data() );
	    config->writeEntry( QString("geometry")+n,  QRect( c->pos(), c->windowWrapper()->size() ) );
	    config->writeEntry( QString("restore")+n, c->geometryRestore() );
	    config->writeEntry( QString("maximize")+n, (int) c->maximizeMode() );
	    config->writeEntry( QString("desktop")+n, c->desktop() );
	    config->writeEntry( QString("iconified")+n, c->isIconified()?"true":"false" );
	    config->writeEntry( QString("sticky")+n, c->isSticky()?"true":"false" );
	    config->writeEntry( QString("shaded")+n, c->isShade()?"true":"false" );
	    config->writeEntry( QString("staysOnTop")+n, c->staysOnTop()?"true":"false" );
	}
    }
    config->writeEntry( "count", count );
}


/*!
  Loads the session information from the config file.

  \sa storeSession()
 */
void Workspace::loadSessionInfo()
{
    session.clear();
    KConfig* config = kapp->sessionConfig();
    config->setGroup("Session" );
    int count =  config->readNumEntry( "count" );
    for ( int i = 1; i <= count; i++ ) {
	QString n = QString::number(i);
	SessionInfo* info = new SessionInfo;
	session.append( info );
	info->sessionId = config->readEntry( QString("sessionId")+n ).latin1();
	info->windowRole = config->readEntry( QString("windowRole")+n ).latin1();
	info->wmCommand = config->readEntry( QString("wmCommand")+n ).latin1();
	info->geometry = config->readRectEntry( QString("geometry")+n );
	info->restore = config->readRectEntry( QString("restore")+n );
	info->maximize = config->readNumEntry( QString("maximize")+n, 0 );
	info->desktop = config->readNumEntry( QString("desktop")+n, 0 );
	info->iconified = config->readBoolEntry( QString("iconified")+n, FALSE );
	info->sticky = config->readBoolEntry( QString("sticky")+n, FALSE );
	info->shaded = config->readBoolEntry( QString("shaded")+n, FALSE );
	info->staysOnTop = config->readBoolEntry( QString("staysOnTop")+n, FALSE  );
    }
}


/*!
  Returns the SessionInfo for client \a c. The returned session
  info is removed from the storage. It's up to the caller to delete it.

  May return 0 if there's no session info for the client.
 */
SessionInfo* Workspace::takeSessionInfo( Client* c )
{
    if ( session.isEmpty() )
	return 0;

    QCString sessionId = c->sessionId();
    QCString windowRole = c->windowRole();
    QCString wmCommand = c->wmCommand();

     for (SessionInfo* info = session.first(); info; info = session.next() ) {
	
	 // a real session managed client
	if ( info->sessionId == sessionId &&
	     ( ( info->windowRole.isEmpty() && windowRole.isEmpty() )
	       || (info->windowRole == windowRole ) ) )
	    return session.take();
	
	// pseudo session management
	if ( info->sessionId.isEmpty() && !info->wmCommand.isEmpty() &&
	     info->wmCommand == wmCommand &&
	     ( ( info->windowRole.isEmpty() && windowRole.isEmpty() )
	       || (info->windowRole == windowRole ) ) )
	    return session.take();
    }

    return 0;
}


/*!
  Updates the current client area according to the current clients.

  If the area changes, the new area is propagate to the world.

  The client area is the area that is available for clients (that
  which is not taken by windows like panels, the top-of-screen menu
  etc).

  \sa clientArea()
 */
void Workspace::updateClientArea()
{
    QRect all = QApplication::desktop()->geometry();
    QRect a = all;
    for ( ClientList::ConstIterator it = clients.begin(); it != clients.end(); ++it) {
	a = a.intersect( (*it)->adjustedClientArea( all ) );
    }

    if ( area != a ) {
	area = a;
	NETRect r;
	r.pos.x = area.x();
	r.pos.y = area.y();
	r.size.width = area.width();
	r.size.height = area.height();
	for( int i = 1; i <= numberOfDesktops(); i++)
	    rootInfo->setWorkArea( i, r );
	
	for ( ClientList::ConstIterator it = clients.begin(); it != clients.end(); ++it) {
	    if ( (*it)->isMaximized() )
		(*it)->maximize( Client::MaximizeAdjust );
	}
    }
}


/*!
  returns the area available for clients. This is the desktop
  geometry minus windows on the dock.  Placement algorithms should
  refer to this rather than geometry().

  \sa geometry()
 */
QRect Workspace::clientArea()
{
  return area;
}


void Workspace::loadDesktopSettings()
{
    KConfig c("kdeglobals");
    c.setGroup("Desktops");
    int n = c.readNumEntry("Number", 4);
    number_of_desktops = n;
    rootInfo->setNumberOfDesktops( number_of_desktops );
    for(int i = 1; i <= n; i++) {
	QString s = c.readEntry(QString("Name_%1").arg(i),
				      i18n("Desktop %1").arg(i));
	rootInfo->setDesktopName( i, s.utf8().data() );
    }
}

void Workspace::saveDesktopSettings()
{
    KConfig c("kdeglobals");
    c.setGroup("Desktops");
    c.writeEntry("Number", number_of_desktops );
    for(int i = 1; i <= number_of_desktops; i++) {
	QString s = desktopName( i );
	QString defaultvalue = i18n("Desktop %1").arg(i);
	if ( s.isEmpty() ) {
	    s = defaultvalue;
	    rootInfo->setDesktopName( i, s.utf8().data() );
	}
	
	if (s != defaultvalue) {
   	    c.writeEntry( QString("Name_%1").arg(i), s );
	} else {
 	    QString currentvalue = c.readEntry(QString("Name_%1").arg(i));
  	    if (currentvalue != defaultvalue)
	        c.writeEntry( QString("Name_%1").arg(i), "" );
	}
    }
}



#include "workspace.moc"
