#include <kconfig.h>
#include <kglobal.h>
#include <kglobalaccel.h>
#include <klocale.h>
#include <kwin.h>

#include "workspace.h"
#include "client.h"
#include "stdclient.h"
#include "beclient.h"
#include "systemclient.h"
#include "tabbox.h"
#include "atoms.h"
#include <X11/X.h>
#include <X11/Xos.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <X11/keysym.h>
#include <X11/extensions/shape.h>

// used to store the return values of
// XShapeQueryExtension.
// Necessary since shaped window are an extension to X
static int kwin_has_shape = 0;
static int kwin_shape_event = 0;

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


static Client* clientFactory( Workspace *ws, WId w )
{
    // hack TODO hints
    char* name = 0;
    QString s;
    if ( XFetchName( qt_xdisplay(), (Window) w, &name ) && name ) {
	s = QString::fromLatin1( name );
	XFree( name );
    }
    if ( s == "THE DESKTOP" ) {
	XLowerWindow( qt_xdisplay(), w );
	Client * c = new NoBorderClient( ws, w);
	c->setSticky( TRUE );
	ws->setDesktopClient( c );
	return c;
    }
    if ( s == "Kicker" ) {
	Client * c = new NoBorderClient( ws, w);
	c->setSticky( TRUE );
	return c;
    }

    if ( Shape::hasShape( w ) ){
	return new NoBorderClient( ws, w );
    }

    KConfig *config = KGlobal::config();
    config->setGroup("style");
    // well, it will be soon ;-)
    QString tmpStr = config->readEntry("Plugin", "standard");
    if(tmpStr == "system")
	return new SystemClient( ws, w );
    else if(tmpStr == "be")
	return new BeClient( ws, w );
    else
	return new StdClient( ws, w );
}

Workspace::Workspace()
{
    root = qt_xrootwin(); // no MDI for now

    (void) QApplication::desktop(); // trigger creation of desktop widget
    desktop_widget = new QWidget(0, "desktop_widget", Qt::WType_Desktop | Qt::WPaintUnclipped );

    // select windowmanager privileges
    XSelectInput(qt_xdisplay(), root,
		 KeyPressMask |
		 PropertyChangeMask |
		 ColormapChangeMask |
		 SubstructureRedirectMask |
		 SubstructureNotifyMask
		 );

    int dummy;
    kwin_has_shape = XShapeQueryExtension(qt_xdisplay(), &kwin_shape_event, &dummy);

    // compatibility
    long data = 1;
    XChangeProperty(qt_xdisplay(), qt_xrootwin(), atoms->kwm_running, atoms->kwm_running, 32,
		    PropModeAppend, (unsigned char*) &data, 1);

    init();
    control_grab = FALSE;
    tab_grab = FALSE;
    tab_box = new TabBox( this );
    keys = 0;
    grabKey(XK_Tab, Mod1Mask);
    grabKey(XK_Tab, Mod1Mask | ShiftMask);
    grabKey(XK_Tab, ControlMask);
    grabKey(XK_Tab, ControlMask | ShiftMask);
    createKeybindings();

}

Workspace::Workspace( WId rootwin )
{
    qDebug("create MDI workspace for %d", rootwin );
    root = rootwin;

    // select windowmanager privileges
    XSelectInput(qt_xdisplay(), root,
		 KeyPressMask |
		 PropertyChangeMask |
		 ColormapChangeMask |
		 SubstructureRedirectMask |
		 SubstructureNotifyMask
		 );

    control_grab = FALSE;
    tab_grab = FALSE;
    tab_box = 0;
    keys = 0;
    init();
}

void Workspace::init()
{
    tab_box = 0;
    active_client = 0;
    should_get_focus = 0;
    desktop_client = 0;
    current_desktop = 0;
    number_of_desktops = 0;
    setNumberOfDesktops( 4 ); // TODO options
    setCurrentDesktop( 1 );

    unsigned int i, nwins;
    Window dw1, dw2, *wins;
    XWindowAttributes attr;

    XGrabServer( qt_xdisplay() );
    XQueryTree(qt_xdisplay(), root, &dw1, &dw2, &wins, &nwins);
    for (i = 0; i < nwins; i++) {
	XGetWindowAttributes(qt_xdisplay(), wins[i], &attr);
	if (attr.override_redirect )
	    continue;
	if (attr.map_state != IsUnmapped) {
	    if ( addDockwin( wins[i] ) )
		continue;
	    Client* c = clientFactory( this, wins[i] );
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
		qDebug(" create a mdi client");
		XReparentWindow( qt_xdisplay(), c->winId(), root, 0, 0 );
		c->move(0,0);
	    }
	}
    }
    XFree((void *) wins);
    XUngrabServer( qt_xdisplay() );
    popup = 0;
    propagateClients();

    //CT initialize the cascading info
    for( int i = 0; i < numberOfDesktops(); i++) {
      CascadingInfo inf;
      inf.pos = QPoint(0,0);
      inf.col = 0;
      inf.row = 0;
      cci.append(inf);
    }
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
    delete tab_box;
    delete popup;
    delete keys;
    if ( root == qt_xrootwin() )
	XDeleteProperty(qt_xdisplay(), qt_xrootwin(), atoms->kwm_running);
}


/*!
  Handles workspace specific XEvents
 */
bool Workspace::workspaceEvent( XEvent * e )
{
    Client * c = findClient( e->xany.window );
    if ( c )
	return c->windowEvent( e );

    switch (e->type) {
    case ButtonPress:
    case ButtonRelease:
	break;
    case UnmapNotify:
	// this is special due to
	// SubstructureNotifyMask. e->xany.window is the window the
	// event is reported to. Take care not to confuse Qt.
	c = findClient( e->xunmap.window );
	
	if ( c )
	    return c->windowEvent( e );

	// check for dock windows
	if ( removeDockwin( e->xunmap.window ) )
	    return TRUE;
	
	if ( e->xunmap.event == root ) { 
	    // keep track of map/unmap for own own windows to avoid
	    // race conditions
	    c = findClientWidthId( e->xunmap.window );
	    if ( c )
		return c->windowEvent( e );
	}
	
	if ( e->xunmap.event != e->xunmap.window ) // hide wm typical event from Qt
	    return TRUE;
    case MapNotify:
	if ( e->xunmap.event == root ) { 
	    // keep track of map/unmap for own own windows to avoid
	    // race conditions
	    c = findClientWidthId( e->xmap.window );
	    if ( c )
		return c->windowEvent( e );
	}
    case ReparentNotify:
	c = findClient( e->xreparent.window );
	if ( c )
	    (void) c->windowEvent( e );
	//do not confuse Qt with these events. After all, _we_ are the
	//window manager who does the reparenting.
	return TRUE;
    case DestroyNotify:
	if ( removeDockwin( e->xdestroywindow.window ) )
	    return TRUE;
	return destroyClient( findClient( e->xdestroywindow.window ) );
    case MapRequest:
	c = findClient( e->xmaprequest.window );
	if ( !c ) {
	    if ( e->xmaprequest.parent == root ) {
		if ( addDockwin( e->xmaprequest.window ) )
		    return TRUE;
		c = clientFactory( this, e->xmaprequest.window );
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
		propagateClients();
	    }
	}
	if ( c ) {
	    bool result = c->windowEvent( e );
	    if ( c == desktop_client )
		setDesktopClient( c );
	    return result;
	}
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
	return keyPress(e->xkey);
	break;
    case KeyRelease:	
	return keyRelease(e->xkey);
	break;
    case FocusIn:
	break;
    case FocusOut:
	break;
    case ClientMessage:
	return clientMessage(e->xclient);
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

/*!
  Returns the workspace's client area.

  This is the area within the geometry() where clients can be placed,
  i.e. the full geometry minus space for desktop panels, taskbars,
  etc.

  Placement algorithms should refer to clientArea.

  \sa geometry()
 */
QRect Workspace::clientArea() const
{
    return geometry(); // for now
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
    delete c;
    clientHidden( c );
    if ( c == desktop_client )
	desktop_client = 0;
    propagateClients();
    return TRUE;
}



/*!
  Auxiliary function to release a passive keyboard grab
 */
void Workspace::freeKeyboard(bool pass){
    if (!pass)
      XAllowEvents(qt_xdisplay(), AsyncKeyboard, CurrentTime);
    else
      XAllowEvents(qt_xdisplay(), ReplayKeyboard, CurrentTime);
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

    const bool options_alt_tab_mode_is_CDE_style = FALSE; // TODO

    if (!control_grab){

	if( (kc == XK_Tab)  &&
	    ( km == (Mod1Mask | ShiftMask)
	      || km == (Mod1Mask)
	      )){
	    if (!tab_grab){
		if (options_alt_tab_mode_is_CDE_style ){
		    // CDE style raise / lower
		    Client* c = topClientOnDesktop();
		    Client* nc = c;
		    if (km & ShiftMask){
			do {
			    nc = previousStaticClient(nc);
			} while (nc && nc != c &&
				 (!nc->isOnDesktop(currentDesktop()) ||
				  nc->isIconified()));

		    }
		    else
			do {
			    nc = nextStaticClient(nc);
			} while (nc && nc != c &&
				 (!nc->isOnDesktop(currentDesktop()) ||
				  nc->isIconified()));
		    if (c && c != nc)
			;//TODO lowerClient(c);
		    if (nc)
			activateClient( nc );
		    freeKeyboard(FALSE);
		    return TRUE;
		}
		XGrabKeyboard(qt_xdisplay(),
			      root, FALSE,
			      GrabModeAsync, GrabModeAsync,
			      CurrentTime);
		tab_grab 	= TRUE;
		tab_box->setMode( TabBox::WindowsMode );
		tab_box->reset();
	    }
	    tab_box->nextPrev( (km & ShiftMask) == 0 );
	    tab_box->show();
	}
    }

    if (!tab_grab){


	if( (kc == XK_Tab)  &&
	    ( km == (ControlMask | ShiftMask)
	      || km == (ControlMask)
	      )){
//TODO 	    if (!options.ControlTab){
// 		freeKeyboard(TRUE);
// 		return TRUE;
// 	    }
	    if (!control_grab){
		XGrabKeyboard(qt_xdisplay(),
			      root, FALSE,
			      GrabModeAsync, GrabModeAsync,
			      CurrentTime);
		control_grab = TRUE;
		tab_box->setMode( TabBox::DesktopMode );
		tab_box->reset();
	    }
	    tab_box->nextPrev( (km & ShiftMask) == 0 );
	    tab_box->show();
	}
    }

    if (control_grab || tab_grab){
	if (kc == XK_Escape){
	    XUngrabKeyboard(qt_xdisplay(), CurrentTime);
	    tab_box->hide();
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
		XUngrabKeyboard(qt_xdisplay(), CurrentTime);
		tab_box->hide();
		tab_grab = false;
 		if ( tab_box->currentClient() ){
		
		    activateClient( tab_box->currentClient() );
 		}
	    }
    }
    if (control_grab){
	XModifierKeymap* xmk = XGetModifierMapping(qt_xdisplay());
	for (i=0; i<xmk->max_keypermod; i++)
	    if (xmk->modifiermap[xmk->max_keypermod * ControlMapIndex + i]
		== key.keycode){
		XUngrabKeyboard(qt_xdisplay(), CurrentTime);
		tab_box->hide();
		control_grab = False;
		if ( tab_box->currentDesktop() != -1 )
		    setCurrentDesktop( tab_box->currentDesktop() );
	    }
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
  Returns topmost visible client within the specified layer range on
  the current desktop, or 0 if no clients are visible. \a fromLayer has to
  be smaller than \a toLayer.
 */
Client* Workspace::topClientOnDesktop( int fromLayer, int toLayer) const
{
    fromLayer = toLayer = 0;
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
  }
  XGrabKey(qt_xdisplay(),
	   XKeysymToKeycode(qt_xdisplay(), keysym), mod,
	   qt_xrootwin(), FALSE,
	   GrabModeSync, GrabModeSync);
  XGrabKey(qt_xdisplay(),
	   XKeysymToKeycode(qt_xdisplay(), keysym), mod | LockMask,
	   qt_xrootwin(), FALSE,
	   GrabModeSync, GrabModeSync);
  XGrabKey(qt_xdisplay(),
	   XKeysymToKeycode(qt_xdisplay(), keysym), mod | NumLockMask,
	   qt_xrootwin(), FALSE,
	   GrabModeSync, GrabModeSync);
  XGrabKey(qt_xdisplay(),
	   XKeysymToKeycode(qt_xdisplay(), keysym), mod | LockMask | NumLockMask,
	   qt_xrootwin(), FALSE,
	   GrabModeSync, GrabModeSync);

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
    WId w = active_client? active_client->window() : 0;
    XChangeProperty(qt_xdisplay(), qt_xrootwin(),
		    atoms->net_active_window, XA_WINDOW, 32,
		    PropModeReplace, (unsigned char *)&w, 1);

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
void Workspace::activateClient( Client* c)
{
    if (!c->isOnDesktop(currentDesktop()) ) {
	setCurrentDesktop( c->desktop() );
    }
    raiseClient( c );
    c->show();
    if ( options->focusPolicyIsReasonable() )
	requestFocus( c );
}


/*!
  Tries to activate the client by asking X for the input focus. This
  function does not perform any show, raise or desktop switching. See
  Workspace::activateClient() instead.

  \sa Workspace::activateClient()
 */
void Workspace::requestFocus( Client* c)
{

    //TODO will be different for non-root clients. (subclassing?)
    if ( !c ) {
	focusToNull();
	return;
    }

    if ( c->isVisible() && !c->isShade() ) {
	c->takeFocus();
	should_get_focus = c;
    } else if ( c->isShade() ) {
	// client cannot accept focus, but at least the window should be active (window menu, et. al. )
	focusToNull();
	c->setActive( TRUE );
    }
}


/*!
  Informs the workspace that the client \a c has been hidden. If it
  was the active client, the workspace activates another one.

  \a c may already be destroyed
 */
void Workspace::clientHidden( Client* c )
{
    if ( c == active_client || ( !active_client && c == should_get_focus ) ) {
	active_client = 0;
	should_get_focus = 0;
	if ( clients.contains( c ) ) {
	    focus_chain.remove( c );
	    focus_chain.prepend( c );
	}
	if ( options->focusPolicyIsReasonable() ) {
	    for ( ClientList::ConstIterator it = focus_chain.fromLast(); it != focus_chain.begin(); --it) {
		if ( (*it)->isVisible() ) {
		    requestFocus( *it );
		    break;
		}
	    }
	}
    }
}


void Workspace::showPopup( const QPoint& pos, Client* c)
{
    // experimental!!!

    if ( !popup ) {
	popup = new QPopupMenu;
	
	// I wish I could use qt-2.1 features here..... grmblll
	QPopupMenu* deco = new QPopupMenu( popup );
	deco->insertItem("KDE Classic", 100 );
	deco->insertItem("Be-like style", 101 );
	deco->insertItem("System style", 102 );
			
	popup->insertItem("Decoration", deco );
    }
    popup_client = c;
    // TODO customize popup for the client
    int ret = popup->exec( pos );
    KConfig *config = KGlobal::config();
    config->setGroup("style");
    switch( ret ) {
    case 100:
        config->writeEntry("Plugin", "standard");
	setDecoration( 0 );
	break;
    case 101:
        config->writeEntry("Plugin", "be");
	setDecoration( 1 );
	break;
    case 102:
        config->writeEntry("Plugin", "system");
	setDecoration( 2 );
	break;
    default:
	break;
    }
    config->sync();

    popup_client = 0;
    ret = 0;
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
  long int overlap, min_overlap;
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
    if ( y + ch > maxRect.bottom() )
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
	  
	  xl = (*l)->x();           yt = (*l)->y();
	  xr = xl + (*l)->height(); yb = yt + (*l)->width();
	  
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

	  xl = (*l)->x();           yt = (*l)->y();
	  xr = xl + (*l)->height(); yb = yt + (*l)->width();
	  
	  // if not enough room above or under the current tested client
	  // determine the first non-overlapped x position
	  if( y < yb  && yt < ch + y ) {
	    
	    if( yb > x ) 
	      possible = possible < yb ? possible : yb;
	    
	    if( xl - cw > x ) 
	      possible = possible < xl - cw ? possible : xl - cw;
	  }
	}
	x = possible;
      }
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
	  
	  xl = (*l)->x();           yt = (*l)->y();
	  xr = xl + (*l)->height(); yb = yt + (*l)->width();
	  
	  if( yb > y) 
	    possible = possible < yb ? possible : yb;
	  
	  if( yt - ch > y ) 
	    possible = possible < yt - ch ? possible : yt - ch;
	}
	y = possible;
      }
    }
  }
  while( overlap != none && overlap != h_wrong );
  
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
    if ( xp != X && yp == Y ) xp = delta_x * (++(cci[d].col));
    if ( yp != Y && xp == X ) yp = delta_y * (++(cci[d].row));
    
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
  Raises the client \a c taking layers, transient windows and window
  groups into account.
 */
void Workspace::raiseClient( Client* c )
{
    if ( !c )
	return;
    if ( c == desktop_client )
	return; // deny

    stacking_order.remove( c );
    stacking_order.append( c );

    ClientList saveset;
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
	    return;
	}
    }

    saveset.clear();
    saveset.append( c );
    raiseTransientsOf(saveset, c );

    Window* new_stack = new Window[ stacking_order.count()+1];
    int i = 0;
    for ( ClientList::ConstIterator it = stacking_order.fromLast(); it != stacking_order.end(); --it) {
	new_stack[i++] = (*it)->winId();
    }
    XRaiseWindow(qt_xdisplay(), new_stack[0]);
    XRestackWindows(qt_xdisplay(), new_stack, i);
    delete [] new_stack;


    propagateClients( TRUE );
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
  Puts the focus on a dummy winodw
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
  XSetInputFocus(qt_xdisplay(), w, RevertToPointerRoot, CurrentTime );
  //colormapFocus(0); TODO
}

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
    if (new_desktop == current_desktop || new_desktop < 1 || new_desktop > number_of_desktops )
	return;

    /*
       optimized Desktop switching: unmapping done from back to front
       mapping done from front to back => less exposure events
    */

    for ( ClientList::ConstIterator it = stacking_order.fromLast(); it != stacking_order.end(); --it) {
	if ( (*it)->isVisible() && !(*it)->isOnDesktop( new_desktop ) ) {
	    (*it)->hide();
	}
    }

    for ( ClientList::ConstIterator it = stacking_order.begin(); it != stacking_order.end(); ++it) {
	if ( (*it)->isOnDesktop( new_desktop ) ) {
	    (*it)->show();
	    //XMapWindow( qt_xdisplay(), (*it)->winId() );
	}
    }

    current_desktop = new_desktop;

    XChangeProperty(qt_xdisplay(), qt_xrootwin(),
		    atoms->net_current_desktop, XA_CARDINAL, 32,
		    PropModeReplace, (unsigned char *)&current_desktop, 1);

    QApplication::syncX();
    KWM::switchToDesktop( current_desktop ); // ### compatibility
}


void Workspace::makeFullScreen( Client*  )
{
    // not yet implemented
}


// experimental
void Workspace::setDecoration( int deco )
{
    if ( !popup_client )
	return;
    Client* c = popup_client;
    WId w = c->window();
    clients.remove( c );
    stacking_order.remove( c );
    focus_chain.remove( c );
    bool mapped = c->isVisible();
    c->hide();
    c->releaseWindow();
    switch ( deco ) {
    case 1:
	c = new BeClient( this, w);
        break;
    case 2:
        c = new SystemClient(this, w);
        break;
    default:
	c = new StdClient( this, w );
    }
    clients.append( c );
    stacking_order.append( c );
    c->manage( mapped );
    activateClient( c );
}


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
    XChangeProperty(qt_xdisplay(), qt_xrootwin(),
		    atoms->net_number_of_desktops, XA_CARDINAL, 32,
		    PropModeReplace, (unsigned char *)&number_of_desktops, 1);
}

/*!
  Handles client messages sent to the workspace
 */
bool Workspace::clientMessage( XClientMessageEvent msg )
{
    if ( msg.message_type == atoms->net_active_window ) {
	Client * c = findClient( msg.data.l[0] );
	if ( c ) {
	    activateClient( c );
	    return TRUE;
	}
    } else if ( msg.message_type == atoms->net_current_desktop ) {
	setCurrentDesktop( msg.data.l[0] );
	return TRUE;
    }

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
	XChangeProperty(qt_xdisplay(), qt_xrootwin(),
			atoms->net_client_list, XA_WINDOW, 32,
			PropModeReplace, (unsigned char *)cl, clients.count());
	delete [] cl;
    }

    cl = new WId[ stacking_order.count()];
    i = 0;
    for ( ClientList::ConstIterator it = stacking_order.begin(); it != stacking_order.end(); ++it) {
	cl[i++] =  (*it)->window();
    }
    XChangeProperty(qt_xdisplay(), qt_xrootwin(),
		    atoms->net_client_list_stacking, XA_WINDOW, 32,
		    PropModeReplace, (unsigned char *)cl, stacking_order.count());
    delete [] cl;
}


bool Workspace::addDockwin( WId w )
{
    WId dockFor = 0;
    if ( !KWin::isDockWindow( w, &dockFor ) )
	return FALSE;
    dockwins.append( DockWindow( w, dockFor ) );
    XSelectInput( qt_xdisplay(), w,
		  StructureNotifyMask
		  );
    propagateDockwins();
    return TRUE;
}

bool Workspace::removeDockwin( WId w )
{
    if ( !dockwins.contains( w ) )
	return FALSE;
    dockwins.remove( w );
    propagateDockwins();
    return TRUE;
}

bool Workspace::iconifyMeansWithdraw( Client* c)
{
    for ( DockWindowList::ConstIterator it = dockwins.begin(); it != dockwins.end(); ++it ) {
	if ( (*it).dockFor == c->window() )
	    return TRUE;
    }
    return FALSE;
}

/*!
  Propagates the dockwins to the world
 */
void Workspace::propagateDockwins()
{
    WId* cl = new WId[ dockwins.count()];
    int i = 0;
    for ( DockWindowList::ConstIterator it = dockwins.begin(); it != dockwins.end(); ++it ) {
	cl[i++] =  (*it).dockWin;
    }
    XChangeProperty(qt_xdisplay(), qt_xrootwin(),
		    atoms->net_kde_docking_windows, XA_WINDOW, 32,
		    PropModeReplace, (unsigned char *)cl, dockwins.count());
    delete [] cl;
}


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
