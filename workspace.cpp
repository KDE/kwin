/*****************************************************************
kwin - the KDE window manager

Copyright (C) 1999, 2000 Matthias Ettrich <ettrich@kde.org>
******************************************************************/

//#define QT_CLEAN_NAMESPACE
#define select kwin_hide_select

#include <kconfig.h>
#include <kglobal.h>
#include <kglobalsettings.h>
#include <kglobalaccel.h>
#include <klocale.h>
#include <stdlib.h>
#include <qwhatsthis.h>
#include <qdatastream.h>
#include <qregexp.h>
#include <qclipboard.h>
#include <kapp.h>
#include <kipc.h>
#include <dcopclient.h>
#include <kprocess.h>
#include <kiconloader.h>
#include <kstartupinfo.h>
#include <qdesktopwidget.h>

#include "workspace.h"
#include "client.h"
#include "tabbox.h"
#include "atoms.h"
#include "plugins.h"
#include "events.h"
#include "killwindow.h"
#include <netwm.h>
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
#include <kdebug.h>

// Possible protoypes for select() were hidden as `kwin_hide_select.
// Undo the hiding definition and defines an acceptable prototype.
// This is how QT does this. It should work where QT works.
#ifdef HAVE_SYS_SELECT_H
#include <sys/select.h>
#endif
#undef select
extern "C" int select(int,void*,void*,void*,struct timeval*);

namespace KWinInternal {

// NET WM Protocol handler class
class RootInfo : public NETRootInfo
{
public:
    RootInfo( KWinInternal::Workspace* ws, Display *dpy, Window w, const char *name, unsigned long pr, int scr= -1)
        : NETRootInfo( dpy, w, name, pr, scr ) {
            workspace = ws;
    }
    ~RootInfo() {}

    void changeNumberOfDesktops(int n) { workspace->setNumberOfDesktops( n ); }
    void changeCurrentDesktop(int d) { workspace->setCurrentDesktop( d ); }
    void changeActiveWindow(Window w) {
        KWinInternal::Client* c = workspace->findClient( (WId) w );
        if ( c )
            workspace->activateClient( c );
    }
    void closeWindow(Window w) {
        KWinInternal::Client* c = workspace->findClient( (WId) w );
        if ( c ) {
            c->closeWindow();
        }
    }
    void moveResize(Window, int, int, unsigned long) { }

private:
    KWinInternal::Workspace* workspace;
};

class WorkspacePrivate
{
public:
    WorkspacePrivate() 
     : startup(0), electric_have_borders(false), 
       electric_current_border(None),
       electric_top_border(None),
       electric_bottom_border(None),
       electric_left_border(None),
       electric_right_border(None),
       electric_time_first(0),
       electric_time_last(0),
       movingClient(0)
    { };
    ~WorkspacePrivate() {};
    KStartupInfo* startup;
    bool electric_have_borders;
    WId electric_current_border;
    WId electric_top_border;
    WId electric_bottom_border;
    WId electric_left_border;
    WId electric_right_border;
    Time electric_time_first;
    Time electric_time_last;
    Client *movingClient;
};

};

using namespace KWinInternal;

extern int kwin_screen_number;

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
static Window null_focus_window = 0;
// does the window w  need a shape combine mask around it?
bool Shape::hasShape( WId w){
    int xws, yws, xbs, ybs;
    unsigned int wws, hws, wbs, hbs;
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
        return ( mgr->allocateClient( this, w, true ) );

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
    return ( mgr->allocateClient( this, w, false ) );
}

Workspace *Workspace::_self = 0;

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
    last_active_client     (0),
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
    _self = this;
    d = new WorkspacePrivate;
    mgr = new PluginMgr;
    connect(options, SIGNAL(resetPlugin()), mgr, SLOT(resetPlugin()));
    root = qt_xrootwin();
    default_colormap = DefaultColormap(qt_xdisplay(), qt_xscreen() );
    installed_colormap = default_colormap;
    session.setAutoDelete( TRUE );

#ifdef HAVE_XINERAMA
    if (XineramaIsActive(qt_xdisplay())) {
	xineramaInfo = XineramaQueryScreens(qt_xdisplay(), &numHeads);
    } else {
    	xineramaInfo = &dummy_xineramaInfo;
	QRect rect = QApplication::desktop()->geometry();

	dummy_xineramaInfo.screen_number = 0;
	dummy_xineramaInfo.x_org = rect.x();
	dummy_xineramaInfo.y_org = rect.y();
	dummy_xineramaInfo.width = rect.width();
	dummy_xineramaInfo.height = rect.height();

	numHeads = 1;
    }
#endif

    if ( restore )
      loadSessionInfo();

    loadFakeSessionInfo();

    (void) QApplication::desktop(); // trigger creation of desktop widget

    desktop_widget =
      new QWidget(
        0,
        "desktop_widget",
        Qt::WType_Desktop | Qt::WPaintUnclipped
    );

    // call this before XSelectInput() on the root window
    d->startup = new KStartupInfo( false, this );

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

    createKeybindings();
    tab_box = new TabBox( this );

    init();

    if ( restore )
        restoreLegacySession(kapp->sessionConfig());
}


void Workspace::init()
{
    if (options->electricBorders())
       createBorderWindows();

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
        NET::KDESystemTrayWindows |
        NET::CloseWindow |
        NET::WMName |
        NET::WMVisibleName |
        NET::WMDesktop |
        NET::WMWindowType |
        NET::WMState |
        NET::WMStrut |
        NET::WMIconGeometry |
        NET::WMIcon |
        NET::WMPid |
        NET::WMKDESystemTrayWinFor |
        NET::WMKDEFrameStrut
        ;

    rootInfo = new RootInfo( this, qt_xdisplay(), supportWindow->winId(), "KWin", protocols, qt_xscreen() );

    loadDesktopSettings();
    setCurrentDesktop( 1 );

    unsigned int i, nwins;
    Window root_return, parent_return, *wins;
    XWindowAttributes attr;

    connect(&resetTimer, SIGNAL(timeout()), this,
            SLOT(slotResetAllClients()));
    connect(&reconfigureTimer, SIGNAL(timeout()), this,
            SLOT(slotReconfigure()));

    connect(mgr, SIGNAL(resetAllClients()), this,
            SLOT(slotResetAllClients()));
    connect(kapp, SIGNAL(appearanceChanged()), this,
            SLOT(slotResetAllClientsDelayed()));

    connect(&focusEnsuranceTimer, SIGNAL(timeout()), this,
            SLOT(focusEnsurance()));

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
    raiseElectricBorders();
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
        storeFakeSessionInfo( *it );
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

    writeFakeSessionInfo();
    KGlobal::config()->sync();

    delete rootInfo;
    delete supportWindow;
    delete mgr;
    delete d;
    _self = 0;

#ifdef HAVE_XINERAMA
    if (xineramaInfo != &dummy_xineramaInfo)
    	XFree(xineramaInfo);
#endif
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

    if ( e->type == PropertyNotify || e->type == ClientMessage ) {
        if ( netCheck( e ) )
            return TRUE;
    }


    if ( e->type == FocusIn )
        focusEnsuranceTimer.stop();
    else if ( e->type == FocusOut )
        focusEnsuranceTimer.start(50);

    Client * c = findClient( e->xany.window );
    if ( c )
        return c->windowEvent( e );

    switch (e->type) {
    case ButtonPress:
        if ( tab_grab || control_grab ) {
            XUngrabKeyboard(qt_xdisplay(), kwin_time);
            XUngrabPointer( qt_xdisplay(), kwin_time);
            tab_box->hide();
            keys->setKeyEventsEnabled( TRUE );
            tab_grab = control_grab = false;
            return TRUE;
        }
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
        checkStartOnDesktop( e->xmaprequest.window );
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
                        focus_chain.append( c );
                    clients.append( c );
                    stacking_order.append( c );
                }
            }
        }
        if ( c ) {
            bool result = c->windowEvent( e );
            if ( c == desktop_client )
                setDesktopClient( c );
            return result;
        }
        break;
    case EnterNotify:
        if ( QWhatsThis::inWhatsThisMode() )
        {
            QWidget* w = QWidget::find( e->xcrossing.window );
            if ( w && w->inherits("WindowWrapper" ) )
                QWhatsThis::leaveWhatsThisMode();
        }

        if (d->electric_have_borders &&
           (e->xcrossing.window == d->electric_top_border ||
            e->xcrossing.window == d->electric_left_border ||
            e->xcrossing.window == d->electric_bottom_border ||
            e->xcrossing.window == d->electric_right_border))
        {
            // the user entered an electric border
            electricBorder(e);
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
        if ( mouse_emulation )
            return keyPressMouseEmulation( e->xkey );
        return keyPress(e->xkey);
    case KeyRelease:
        if ( mouse_emulation )
            return FALSE;
        return keyRelease(e->xkey);
        break;
    case FocusIn:
        break;
    case FocusOut:
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


void Workspace::removeClient( Client* c) {
    clients.remove( c );
    stacking_order.remove( c );
    focus_chain.remove( c );
    propagateClients();
}

/*
  Destroys the client \a c
 */
bool Workspace::destroyClient( Client* c)
{
    if ( !c )
        return FALSE;

    storeFakeSessionInfo( c );
    if (clients.contains(c))
	removeClient(c);

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
    if ( c == last_active_client )
        last_active_client = 0;
    delete c;
    updateClientArea();
    return TRUE;
}



/*!
  Handles alt-tab / control-tab
 */

#include <stdarg.h>

bool areKeySymXsDepressed( bool bAll, int nKeySyms, ... )
{
	va_list args;
	char keymap[32];

	kdDebug(125) << "areKeySymXsDepressed: " << (bAll ? "all of " : "any of ") << nKeySyms << endl;

	va_start( args, nKeySyms );
	XQueryKeymap( qt_xdisplay(), keymap );

	for( int iKeySym = 0; iKeySym < nKeySyms; iKeySym++ ) {
		uint keySymX = va_arg( args, uint );
		uchar keyCodeX = XKeysymToKeycode( qt_xdisplay(), keySymX );
		int i = keyCodeX / 8;
		char mask = 1 << (keyCodeX - (i * 8));

		kdDebug(125) << iKeySym << ": keySymX=0x" << QString::number( keySymX, 16 )
			<< " i=" << i << " mask=0x" << QString::number( mask, 16 )
			<< " keymap[i]=0x" << QString::number( keymap[i], 16 ) << endl;

		// Abort if bad index value,
		if( i < 0 || i >= 32 )
			return false;

		// If ALL keys passed need to be depressed,
		if( bAll ) {
			if( (keymap[i] & mask) == 0 )
				return false;
		} else {
			// If we are looking for ANY key press, and this key is depressed,
			if( keymap[i] & mask )
				return true;
		}
	}

	// If we were looking for ANY key press, then none was found, return false,
	// If we were looking for ALL key presses, then all were found, return true.
	return bAll;
}

bool areModKeysDepressed( uint keyCombQt )
{
	uint rgKeySyms[8];
	int nKeySyms = 0;

	if( keyCombQt & Qt::SHIFT ) {
		rgKeySyms[nKeySyms++] = XK_Shift_L;
		rgKeySyms[nKeySyms++] = XK_Shift_R;
	}
	if( keyCombQt & Qt::CTRL ) {
		rgKeySyms[nKeySyms++] = XK_Control_L;
		rgKeySyms[nKeySyms++] = XK_Control_R;
	}
	if( keyCombQt & Qt::ALT ) {
		rgKeySyms[nKeySyms++] = XK_Alt_L;
		rgKeySyms[nKeySyms++] = XK_Alt_R;
	}
	if( keyCombQt & (Qt::ALT<<1) ) {
		rgKeySyms[nKeySyms++] = XK_Meta_L;
		rgKeySyms[nKeySyms++] = XK_Meta_R;
	}

	// Is there a better way to push all 8 integer onto the stack?
	return areKeySymXsDepressed( false, nKeySyms,
		rgKeySyms[0], rgKeySyms[1], rgKeySyms[2], rgKeySyms[3],
		rgKeySyms[4], rgKeySyms[5], rgKeySyms[6], rgKeySyms[7] );
}

void Workspace::slotWalkThroughWindows()
{
    if ( root != qt_xrootwin() )
        return;
    if( tab_grab || control_grab )
        return;
    if ( options->altTabStyle == Options::CDE  || !options->focusPolicyIsReasonable() ) {
        XUngrabKeyboard(qt_xdisplay(), kwin_time); // need that because of accelerator raw mode
        // CDE style raise / lower
        CDEWalkThroughWindows( true );
    } else {
        if( areModKeysDepressed( walkThroughWindowsKeycode ) ) {
            if ( startKDEWalkThroughWindows() )
		KDEWalkThroughWindows( true );
        }
        else
            // if the shortcut has no modifiers, don't show the tabbox, but
            // simply go to the next window; if the shortcut has no modifiers,
            // the only sane thing to do is to release the key immediately
            // anyway, so the tabbox wouldn't appear anyway
            // it's done this way without grabbing because with grabbing
            // the keyboard wasn't ungrabbed and I really have no idea why
            // <l.lunak@kde.org>
            KDEOneStepThroughWindows( true );
    }
}

void Workspace::slotWalkBackThroughWindows()
{
    if ( root != qt_xrootwin() )
	return;
    if( tab_grab || control_grab )
	return;
    if ( options->altTabStyle == Options::CDE  || !options->focusPolicyIsReasonable() ) {
	// CDE style raise / lower
	CDEWalkThroughWindows( true );
    } else {
	if ( areModKeysDepressed( walkBackThroughWindowsKeycode ) ) {
	    if ( startKDEWalkThroughWindows() )
		KDEWalkThroughWindows( false );
	} else {
	    KDEOneStepThroughWindows( false );
	}
    }
}

void Workspace::slotWalkThroughDesktops()
{
    if ( root != qt_xrootwin() )
	return;
    if( tab_grab || control_grab )
	return;
    if ( areModKeysDepressed( walkThroughDesktopsKeycode ) ) {
	if ( startWalkThroughDesktops() )
	    walkThroughDesktops( true );
    } else {
	oneStepThroughDesktops( true );
    }
}

void Workspace::slotWalkBackThroughDesktops()
{
    if ( root != qt_xrootwin() )
        return;
    if( tab_grab || control_grab )
	return;
    if ( areModKeysDepressed( walkBackThroughDesktopsKeycode ) ) {
	if ( startWalkThroughDesktops() )
	    walkThroughDesktops( false );
    } else {
	oneStepThroughDesktops( false );
    }
}

void Workspace::slotWalkThroughDesktopList()
{
    if ( root != qt_xrootwin() )
	return;
    if( tab_grab || control_grab )
	return;
    if ( areModKeysDepressed( walkThroughDesktopListKeycode ) ) {
	if ( startWalkThroughDesktopList() )
	    walkThroughDesktops( true );
    } else {
	oneStepThroughDesktopList( true );
    }
}

void Workspace::slotWalkBackThroughDesktopList()
{
    if ( root != qt_xrootwin() )
        return;
    if( tab_grab || control_grab )
	return;
    if ( areModKeysDepressed( walkBackThroughDesktopListKeycode ) ) {
	if ( startWalkThroughDesktopList() )
	    walkThroughDesktops( false );
    } else {
	oneStepThroughDesktopList( false );
    }
}

bool Workspace::startKDEWalkThroughWindows()
{
    if ( XGrabPointer( qt_xdisplay(), root, TRUE,
		       (uint)(ButtonPressMask | ButtonReleaseMask |
			      ButtonMotionMask | EnterWindowMask |
			      LeaveWindowMask | PointerMotionMask),
		       GrabModeAsync, GrabModeAsync,
		       None, None, kwin_time ) != GrabSuccess ) {
	return FALSE;
    }
    if ( XGrabKeyboard(qt_xdisplay(),
		       root, FALSE,
		       GrabModeAsync, GrabModeAsync,
		       kwin_time) != GrabSuccess ) {
	XUngrabPointer( qt_xdisplay(), kwin_time);
	return FALSE;
    }
    tab_grab        = TRUE;
    keys->setKeyEventsEnabled( FALSE );
    tab_box->setMode( TabBox::WindowsMode );
    tab_box->reset();
    return TRUE;
}

bool Workspace::startWalkThroughDesktops( int mode )
{
    if ( XGrabPointer( qt_xdisplay(), root, TRUE,
		       (uint)(ButtonPressMask | ButtonReleaseMask |
			      ButtonMotionMask | EnterWindowMask |
			      LeaveWindowMask | PointerMotionMask),
		       GrabModeAsync, GrabModeAsync,
		       None, None, kwin_time ) != GrabSuccess ) {
	return FALSE;
    }
    if ( XGrabKeyboard(qt_xdisplay(),
		       root, FALSE,
		       GrabModeAsync, GrabModeAsync,
		       kwin_time) != GrabSuccess ) {
	XUngrabPointer( qt_xdisplay(), kwin_time);
	return FALSE;
    }
    control_grab = TRUE;
    keys->setKeyEventsEnabled( FALSE );
    tab_box->setMode( (TabBox::Mode) mode );
    tab_box->reset();
    return TRUE;
}

bool Workspace::startWalkThroughDesktops()
{
    return startWalkThroughDesktops( TabBox::DesktopMode );
}

bool Workspace::startWalkThroughDesktopList()
{
    return startWalkThroughDesktops( TabBox::DesktopListMode );
}

void Workspace::KDEWalkThroughWindows( bool forward )
{
    tab_box->nextPrev( forward );
    tab_box->delayedShow();
}

void Workspace::walkThroughDesktops( bool forward )
{
    tab_box->nextPrev( forward );
    tab_box->delayedShow();
}

void Workspace::CDEWalkThroughWindows( bool forward )
    {
    Client* c = topClientOnDesktop();
    Client* nc = c;
    if ( !forward ){
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
    }

void Workspace::KDEOneStepThroughWindows( bool forward )
{
    tab_box->setMode( TabBox::WindowsMode );
    tab_box->reset();
    tab_box->nextPrev( forward );
    if ( tab_box->currentClient() ){
        activateClient( tab_box->currentClient() );
    }
}

void Workspace::oneStepThroughDesktops( bool forward, int mode )
{
    tab_box->setMode( (TabBox::Mode) mode );
    tab_box->reset();
    tab_box->nextPrev( forward );
    if ( tab_box->currentDesktop() != -1 )
        setCurrentDesktop( tab_box->currentDesktop() );
}

void Workspace::oneStepThroughDesktops( bool forward )
{
    oneStepThroughDesktops( forward, TabBox::DesktopMode );
}

void Workspace::oneStepThroughDesktopList( bool forward )
{
    oneStepThroughDesktops( forward, TabBox::DesktopListMode );
}

/*!
  Handles holding alt-tab / control-tab
 */
bool Workspace::keyPress(XKeyEvent key)
{
    if ( root != qt_xrootwin() )
        return FALSE;

    uint keyCombQt = KAccel::keyEventXToKeyQt( (XEvent*)&key );

    if (!control_grab){
        if( keyCombQt == walkThroughWindowsKeycode
           || keyCombQt == walkBackThroughWindowsKeycode ) {
            if (!tab_grab) 
                return FALSE;
            KDEWalkThroughWindows( keyCombQt == walkThroughWindowsKeycode );
        }
    }

    if (!tab_grab){

        if( keyCombQt == walkThroughDesktopsKeycode
           || keyCombQt == walkBackThroughDesktopsKeycode ) {
            if (!control_grab)
                return FALSE;
            walkThroughDesktops( keyCombQt == walkThroughDesktopsKeycode );
        }
        else if( keyCombQt == walkThroughDesktopListKeycode
           || keyCombQt == walkBackThroughDesktopListKeycode ) {
            if (!control_grab) 
                return FALSE;
            walkThroughDesktops( keyCombQt == walkThroughDesktopListKeycode );
        }
    }

    if (control_grab || tab_grab){
        if ((keyCombQt & 0xffff) == Qt::Key_Escape){
            XUngrabKeyboard(qt_xdisplay(), kwin_time);
            XUngrabPointer( qt_xdisplay(), kwin_time);
            tab_box->hide();
            keys->setKeyEventsEnabled( TRUE );
            tab_grab = FALSE;
            control_grab = FALSE;
        }
        return TRUE;
    }

    return FALSE;
}

/*!
  Handles alt-tab / control-tab releasing
 */
bool Workspace::keyRelease(XKeyEvent key)
{
    if ( root != qt_xrootwin() )
        return FALSE;
    if( !tab_grab && !control_grab )
        return FALSE;
    unsigned int mk = key.state & KAccel::accelModMaskX();
    // key.state is state before the key release, so just checking mk being 0 isn't enough
    // using XQueryPointer() also doesn't seem to work well, so the check that all
    // modifiers are released is : only one modifier is active and the currently released
    // key is this modifier - if yes, release the grab
    int mod_index = -1;
    for( int i = ShiftMapIndex;
         i <= Mod5MapIndex;
         ++i )
        if(( mk & ( 1 << i )) != 0 ) {
            if( mod_index >= 0 )
                return FALSE;
            mod_index = i;
        }
    bool release = false;
    if( mod_index == -1 )
        release = true;
    else {
        XModifierKeymap* xmk = XGetModifierMapping(qt_xdisplay());
        for (int i=0; i<xmk->max_keypermod; i++)
            if (xmk->modifiermap[xmk->max_keypermod * mod_index + i]
                == key.keycode)
                release = true;
        XFreeModifiermap(xmk);
    }
    if( !release )
         return FALSE;
    if (tab_grab){
                XUngrabPointer( qt_xdisplay(), kwin_time);
                XUngrabKeyboard(qt_xdisplay(), kwin_time);
                tab_box->hide();
                keys->setKeyEventsEnabled( TRUE );
                tab_grab = false;
                if ( tab_box->currentClient() ){
                    activateClient( tab_box->currentClient() );
                }
    }
    if (control_grab){
                XUngrabPointer( qt_xdisplay(), kwin_time);
                XUngrabKeyboard(qt_xdisplay(), kwin_time);
                tab_box->hide();
                keys->setKeyEventsEnabled( TRUE );
                control_grab = False;
                if ( tab_box->currentDesktop() != -1 )
                    setCurrentDesktop( tab_box->currentDesktop() );
    }
    return FALSE;
}

//#undef XMODMASK

int Workspace::nextDesktop( int iDesktop ) const
{
	int i = desktop_focus_chain.find( iDesktop );
	if( i >= 0 && i+1 < (int)desktop_focus_chain.size() )
		return desktop_focus_chain[i+1];
	else if( desktop_focus_chain.size() > 0 )
		return desktop_focus_chain[ 0 ];
	else
		return 1;
}

int Workspace::previousDesktop( int iDesktop ) const
{
	int i = desktop_focus_chain.find( iDesktop );
	if( i-1 >= 0 )
		return desktop_focus_chain[i-1];
	else if( desktop_focus_chain.size() > 0 )
		return desktop_focus_chain[desktop_focus_chain.size()-1];
	else
		return numberOfDesktops();
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
    last_active_client = active_client;
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
        menubar->raise(); // better for FocusFollowsMouse than raiseClient(menubar)
        raiseElectricBorders();
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

  \sa stActiveClient(), requestFocus()
 */
void Workspace::activateClient( Client* c, bool force )
{
    raiseClient( c );
    if ( c->isIconified() )
        Events::raise( Events::DeIconify );
    c->show();
    iconifyOrDeiconifyTransientsOf( c );
    if ( options->focusPolicyIsReasonable() ) {
        requestFocus( c, force );
    }

    if (!c->isOnDesktop(currentDesktop()) ) {
        setCurrentDesktop( c->desktop() );
    }
}

void Workspace::iconifyOrDeiconifyTransientsOf( Client* c )
{
    if ( c->isIconified() || c->isShade() ) {
        bool exclude_menu = !c->isIconified();
        for ( ClientList::ConstIterator it = clients.begin(); it != clients.end(); ++it) {
            if ( (*it)->transientFor() == c->window()
                 && !(*it)->isIconified()
                 && !(*it)->isShade()
                 && ( !exclude_menu || !(*it)->isMenu() ) ) {
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
    if (!focusChangeEnabled() && ( c != active_client) )
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
    c->setActive( FALSE ); // clear the state in the client
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
    if ( desktop_client )
        requestFocus( desktop_client );
    else
        focusToNull();
    } // if blocking focus, move focus to desktop_client later if needed
      // in order to avoid flickering
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

        // PluginMenu *deco = new PluginMenu(mgr, popup);
        // deco->setFont(KGlobalSettings::menuFont());

        desk_popup = new QPopupMenu( popup );
        desk_popup->setCheckable( TRUE );
        desk_popup->setFont(KGlobalSettings::menuFont());
        connect( desk_popup, SIGNAL( activated(int) ), this, SLOT( sendToDesktop(int) ) );
        connect( desk_popup, SIGNAL( aboutToShow() ), this, SLOT( desktopPopupAboutToShow() ) );

        popup->insertItem( SmallIconSet( "move" ), i18n("&Move"), Options::MoveOp );
        popup->insertItem( i18n("&Size"), Options::ResizeOp );
        popup->insertItem( i18n("Mi&nimize"), Options::IconifyOp );
        popup->insertItem( i18n("Ma&ximize"), Options::MaximizeOp );
        popup->insertItem( i18n("Sh&ade"), Options::ShadeOp );
        popup->insertItem( SmallIconSet( "attach" ), i18n("Always &On Top"), Options::StaysOnTopOp );
        popup->insertItem( SmallIconSet( "filesave" ), i18n("Sto&re Settings"), Options::ToggleStoreSettingsOp );

        popup->insertSeparator();

        popup->insertItem(SmallIconSet( "configure" ), i18n("&Configure..."), this, SLOT( configureWM() ));
        popup->insertItem(i18n("&To desktop"), desk_popup );

        popup->insertSeparator();

        QString k = KAccel::keyToString( keys->currentKey( "Window close" ), true );
        popup->insertItem( SmallIconSet( "remove" ), i18n("&Close")+'\t'+k, Options::CloseOp );
    }
    return popup;
}

void Workspace::showWindowMenuAt( unsigned long id, int x, int y )
{
    Client *target = findClient( id );

    if (!target)
        return;

    Client* c = active_client;
    QPopupMenu* p = clientPopup( target );
    p->exec( QPoint( x, y ) );
    if ( hasClient( c ) )
        requestFocus( c );
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
    case Options::StickyOp:
        c->setSticky( !c->isSticky() );
        break;
    case Options::StaysOnTopOp:
        c->setStaysOnTop( !c->staysOnTop() );
        raiseClient( c );
        break;
    case Options::ToggleStoreSettingsOp:
        c->setStoreSettings( !c->storeSettings() );
        break;
    case Options::HMaximizeOp:
        c->maximize(Client::MaximizeHorizontal);
        break;
    case Options::VMaximizeOp:
        c->maximize(Client::MaximizeVertical);
        break;
    case Options::LowerOp:
        lowerClient(c);
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

    QRect maxRect = clientArea(PlacementArea);

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
     * Xinerama supported added by Balaji Ramani (balaji@yablibli.com)
     * with ideas from xfce.
     */

    const int none = 0, h_wrong = -1, w_wrong = -2; // overlap types
    long int overlap, min_overlap = 0;
    int x_optimal, y_optimal;
    int possible;

    int cxl, cxr, cyt, cyb;     //temp coords
    int  xl,  xr,  yt,  yb;     //temp coords
    int basket;                 //temp holder

    // get the maximum allowed windows space
    QRect maxRect = clientArea(PlacementArea);
    int x = maxRect.left(), y = maxRect.top();
    x_optimal = x; y_optimal = y;

    //client gabarit
    int ch = c->height() - 1;
    int cw = c->width()  - 1;

    bool first_pass = true; //CT lame flag. Don't like it. What else would do?

    //loop over possible positions
    do {
        //test if enough room in x and y directions
        if ( y + ch > maxRect.bottom() && ch < maxRect.height())
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
                        if((*l)->staysOnTop())
                            overlap += 16 * (xr - xl) * (yb - yt);
                        else
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

                        if( ( xr > x ) && ( possible > xr ) ) possible = xr;

                        basket = xl - cw;
                        if( ( basket > x) && ( possible > basket ) ) possible = basket;
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

                    // if not enough room to the left or right of the current tested client
                    // determine the first non-overlapped y position
                    if( ( yb > y ) && ( possible > yb ) ) possible = yb;

                    basket = yt - ch;
                    if( (basket > y ) && ( possible > basket ) ) possible = basket;
                }
            }
            y = possible;
        }
    }
    while( ( overlap != none ) && ( overlap != h_wrong ) && ( y < maxRect.bottom() ) );

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
    QRect maxRect = clientArea(PlacementArea);

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
  Marks the client as being moved around by the user.
 */
void Workspace::setClientIsMoving( Client *c )
{
//    assert(!c || !d->movingClient); // Catch attempts to move a second
    // window while still moving the first one.
    d->movingClient = c;
    if (d->movingClient)
       focus_change = false;
    else
       focus_change = true;
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


void Workspace::reconfigure()
{
    reconfigureTimer.start(200, true);
}


/*!
  Reread settings
 */
void Workspace::slotReconfigure()
{
    reconfigureTimer.stop();
    KGlobal::config()->reparseConfiguration();
    options->reload();
    keys->readSettings();
    tab_box->reconfigure();
    walkThroughDesktopsKeycode = keys->currentKey( "Walk through desktops" );
    walkBackThroughDesktopsKeycode = keys->currentKey( "Walk back through desktops" );
    walkThroughDesktopListKeycode = keys->currentKey( "Walk through desktop list" );
    walkBackThroughDesktopListKeycode = keys->currentKey( "Walk back through desktop list" );
    walkThroughWindowsKeycode = keys->currentKey( "Walk through windows" );
    walkBackThroughWindowsKeycode = keys->currentKey( "Walk back through windows" );
    mgr->updatePlugin();
    // NO need whatsoever to call slotResetAllClientsDelayed here,
    // updatePlugin resets all clients if necessary anyway.

    if (options->electricBorders())
       createBorderWindows();
    else
       destroyBorderWindows();
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
        QRegExp r( (*it) );
        if (r.match(title) != -1) {
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
//     XRaiseWindow(qt_xdisplay(), new_stack[0]);
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
//     XRaiseWindow(qt_xdisplay(), new_stack[0]);
    XRestackWindows(qt_xdisplay(), new_stack, i);
    delete [] new_stack;


    propagateClients( TRUE );

    if ( tab_box->isVisible() )
        tab_box->raise();

    raiseElectricBorders();
}

void Workspace::raiseOrLowerClient( Client *c)
{
    if (!c) return;

    if (c == most_recently_raised)
    {
        lowerClient(c);
    }
    else
    {
        raiseClient(c);
    }
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
  Just using XSetInputFocus() with None would block keyboard input
 */
void Workspace::focusToNull(){
  int mask;
  XSetWindowAttributes attr;
  if (null_focus_window == 0) {
    mask = CWOverrideRedirect;
    attr.override_redirect = 1;
    null_focus_window = XCreateWindow(qt_xdisplay(), qt_xrootwin(), -1,-1, 1, 1, 0, CopyFromParent,
                      InputOnly, CopyFromParent, mask, &attr);
    XMapWindow(qt_xdisplay(), null_focus_window);
  }
  XSetInputFocus(qt_xdisplay(), null_focus_window, RevertToPointerRoot, kwin_time );
  if( !block_focus )
      setActiveClient( NULL );
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
  Refreshes all the client windows
 */
void Workspace::refresh() {
   /* This idea/code is borrowed from FVWM 2.x */
   XSetWindowAttributes attributes;
   unsigned long valuemask = CWOverrideRedirect|
                             CWBackingStore|
                             CWSaveUnder|
                             CWBackPixmap;
   attributes.background_pixmap = None;
   attributes.save_under = False;
   attributes.override_redirect = True;
   attributes.backing_store = NotUseful;
   WId rw = XCreateWindow(qt_xdisplay(), root, 0, 0,
                          desktop_client->width(),
                          desktop_client->height(),
                          0,
                          CopyFromParent, CopyFromParent,
                          CopyFromParent, valuemask,
                          &attributes);
   XMapWindow(qt_xdisplay(), rw);
   XDestroyWindow(qt_xdisplay(), rw);
   XFlush(qt_xdisplay());
}

/*!
  During virt. desktop switching, desktop areas covered by windows that are
  going to be hidden are first obscured by new windows with no background
  ( i.e. transparent ) placed right below the windows. These invisible windows
  are removed after the switch is complete.
  Reduces desktop ( wallpaper ) repaints during desktop switching
*/
class ObscuringWindows
{
public:
    ~ObscuringWindows();
    void create( Client* c );
private:
    QValueList<Window> obscuring_windows;
    static QValueList<Window>* cached;
    static unsigned int max_cache_size;
};

QValueList<Window>* ObscuringWindows::cached = NULL;
unsigned int ObscuringWindows::max_cache_size = 0;

void ObscuringWindows::create( Client* c )
{
    if( cached == NULL )
        cached = new QValueList<Window>;
    Window obs_win;
    XWindowChanges chngs;
    int mask = CWSibling | CWStackMode;
    if( cached->count() > 0 ) {
        cached->remove( obs_win = cached->first());
        chngs.x = c->x();
        chngs.y = c->y();
        chngs.width = c->width();
        chngs.height = c->height();
        mask |= CWX | CWY | CWWidth | CWHeight;
    }
    else {
        XSetWindowAttributes a;
        a.background_pixmap = None;
        a.override_redirect = True;
        obs_win = XCreateWindow( qt_xdisplay(), qt_xrootwin(), c->x(), c->y(),
            c->width(), c->height(), 0, CopyFromParent, InputOutput,
            CopyFromParent, CWBackPixmap | CWOverrideRedirect, &a );
    }
    chngs.sibling = c->winId();
    chngs.stack_mode = Below;
    XConfigureWindow( qt_xdisplay(), obs_win, mask, &chngs );
    XMapWindow( qt_xdisplay(), obs_win );
    obscuring_windows.append( obs_win );
}

ObscuringWindows::~ObscuringWindows()
{
    max_cache_size = QMAX( max_cache_size, obscuring_windows.count() + 4 ) - 1;
    for( QValueList<Window>::ConstIterator it = obscuring_windows.begin();
         it != obscuring_windows.end();
         ++it ) {
        XUnmapWindow( qt_xdisplay(), *it );
        if( cached->count() < max_cache_size )
            cached->prepend( *it );
        else
            XDestroyWindow( qt_xdisplay(), *it );
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
        Events::raise((Events::Event) (Events::DesktopChange+new_desktop));

        ObscuringWindows obs_wins;

        if (d->movingClient && !d->movingClient->isSticky())
        {
            d->movingClient->setDesktop(-1); // All desktops
        }

        for ( ClientList::ConstIterator it = stacking_order.begin(); it != stacking_order.end(); ++it) {
            if ( (*it)->isVisible() && !(*it)->isOnDesktop( new_desktop ) ) {
                obs_wins.create( *it );
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

        if (d->movingClient && !d->movingClient->isSticky())
        {
            d->movingClient->setDesktop(new_desktop);
        }
    }
    current_desktop = new_desktop;
    KIPC::sendMessageAll(KIPC::BackgroundChanged, current_desktop);

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

/*        if (!c) { // this is useless - these windows don't accept focus
            // Search top-most visible window
            for ( ClientList::ConstIterator it = stacking_order.fromLast(); it != stacking_order.end(); --it) {
                if ( (*it)->isVisible() ) {
                    c = *it;
                    break;
                }
            }
        }*/
    }

    if ( c ) {
        requestFocus( c );
        // don't let the panel cover fullscreen windows on desktop switches
        if ( c->isFullScreen() && !c->isDesktop() && c->staysOnTop() )
            raiseClient( c );
    } else {
        focusToNull();
    }
    if( desktop_client ) {
        Window w_tmp;
        int i_tmp;
        XGetInputFocus( qt_xdisplay(), &w_tmp, &i_tmp );
        if( w_tmp == null_focus_window )
            requestFocus( desktop_client );
    }

    // Update focus chain:
    //  If input: chain = { 1, 2, 3, 4 } and current_desktop = 3,
    //   Output: chain = { 3, 1, 2, 4 }.
//    kdDebug(1212) << QString("Switching to desktop #%1, at focus_chain index %2\n")
//    	.arg(current_desktop).arg(desktop_focus_chain.find( current_desktop ));
    for( int i = desktop_focus_chain.find( current_desktop ); i > 0; i-- )
        desktop_focus_chain[i] = desktop_focus_chain[i-1];
    desktop_focus_chain[0] = current_desktop;

    QString s = "desktop_focus_chain[] = { ";
    for( uint i = 0; i < desktop_focus_chain.size(); i++ )
        s += QString::number(desktop_focus_chain[i]) + ", ";
//    kdDebug(1212) << s << "}\n";
}

void Workspace::nextDesktop()
{
    int desktop = currentDesktop() + 1;
    setCurrentDesktop(desktop > numberOfDesktops() ? 1 : desktop);
}

void Workspace::previousDesktop()
{
    int desktop = currentDesktop() - 1;
    setCurrentDesktop(desktop ? desktop : numberOfDesktops());
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

    // Resize and reset the desktop focus chain.
    desktop_focus_chain.resize( n );
    for( int i = 0; i < (int)desktop_focus_chain.size(); i++ )
    	desktop_focus_chain[i] = i+1;
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

/*!
  Called when a newly mapped client is ready ( has properties set correctly )
 */
void Workspace::clientReady( Client* )
{
    propagateClients();
}

/*!
  Propagates the managed clients to the world
 */
void Workspace::propagateClients( bool onlyStacking )
{
    Window *cl; // MW we should not assume WId and Window to be compatible
                                // when passig pointers around.

    int i;
    if ( !onlyStacking ) {
        cl = new Window[ clients.count()];
        i = 0;
        for ( ClientList::ConstIterator it = clients.begin(); it != clients.end(); ++it )
            cl[i++] =  (*it)->window();
        rootInfo->setClientList( cl, i );
        delete [] cl;
    }

    cl = new Window[ stacking_order.count()];
    i = 0;
    for ( ClientList::ConstIterator it = stacking_order.begin(); it != stacking_order.end(); ++it)
        cl[i++] =  (*it)->window();
    rootInfo->setClientListStacking( cl, i );
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
  Propagates the systemTrayWins to the world
 */
void Workspace::propagateSystemTrayWins()
{
    Window *cl = new Window[ systemTrayWins.count()];

    int i = 0;
    for ( SystemTrayWindowList::ConstIterator it = systemTrayWins.begin(); it != systemTrayWins.end(); ++it ) {
        cl[i++] =  (*it).win;
    }

    rootInfo->setKDESystemTrayWindows( cl, i );
    delete [] cl;
}


/*!
  Create the global accel object \c keys.
 */
void Workspace::createKeybindings(){
    keys = new KGlobalAccel();

#include "kwinbindings.cpp"

    keys->connectItem( "Switch to desktop 1", this, SLOT( slotSwitchToDesktop( int ) ));
    keys->connectItem( "Switch to desktop 2", this, SLOT( slotSwitchToDesktop( int ) ));
    keys->connectItem( "Switch to desktop 3", this, SLOT( slotSwitchToDesktop( int ) ));
    keys->connectItem( "Switch to desktop 4", this, SLOT( slotSwitchToDesktop( int ) ));
    keys->connectItem( "Switch to desktop 5", this, SLOT( slotSwitchToDesktop( int ) ));
    keys->connectItem( "Switch to desktop 6", this, SLOT( slotSwitchToDesktop( int ) ));
    keys->connectItem( "Switch to desktop 7", this, SLOT( slotSwitchToDesktop( int ) ));
    keys->connectItem( "Switch to desktop 8", this, SLOT( slotSwitchToDesktop( int ) ));
    keys->connectItem( "Switch to desktop 9", this, SLOT( slotSwitchToDesktop( int ) ));
    keys->connectItem( "Switch to desktop 10", this, SLOT( slotSwitchToDesktop( int ) ));
    keys->connectItem( "Switch to desktop 11", this, SLOT( slotSwitchToDesktop( int ) ));
    keys->connectItem( "Switch to desktop 12", this, SLOT( slotSwitchToDesktop( int ) ));
    keys->connectItem( "Switch to desktop 13", this, SLOT( slotSwitchToDesktop( int ) ));
    keys->connectItem( "Switch to desktop 14", this, SLOT( slotSwitchToDesktop( int ) ));
    keys->connectItem( "Switch to desktop 15", this, SLOT( slotSwitchToDesktop( int ) ));
    keys->connectItem( "Switch to desktop 16", this, SLOT( slotSwitchToDesktop( int ) ));
    keys->connectItem( "Switch desktop previous", this, SLOT( slotSwitchDesktopPrevious() ));
    keys->connectItem( "Switch desktop next", this, SLOT( slotSwitchDesktopNext() ));
    keys->connectItem( "Switch desktop left", this, SLOT( slotSwitchDesktopLeft() ));
    keys->connectItem( "Switch desktop right", this, SLOT( slotSwitchDesktopRight() ));
    keys->connectItem( "Switch desktop up", this, SLOT( slotSwitchDesktopUp() ));
    keys->connectItem( "Switch desktop down", this, SLOT( slotSwitchDesktopDown() ));

    /*keys->connectItem( "Switch to Window 1", this, SLOT( slotSwitchToWindow( int ) ));
    keys->connectItem( "Switch to Window 2", this, SLOT( slotSwitchToWindow( int ) ));
    keys->connectItem( "Switch to Window 3", this, SLOT( slotSwitchToWindow( int ) ));
    keys->connectItem( "Switch to Window 4", this, SLOT( slotSwitchToWindow( int ) ));
    keys->connectItem( "Switch to Window 5", this, SLOT( slotSwitchToWindow( int ) ));
    keys->connectItem( "Switch to Window 6", this, SLOT( slotSwitchToWindow( int ) ));
    keys->connectItem( "Switch to Window 7", this, SLOT( slotSwitchToWindow( int ) ));
    keys->connectItem( "Switch to Window 8", this, SLOT( slotSwitchToWindow( int ) ));
    keys->connectItem( "Switch to Window 9", this, SLOT( slotSwitchToWindow( int ) ));*/

    keys->connectItem( "Window to Desktop 1", this, SLOT( slotWindowToDesktop( int ) ));
    keys->connectItem( "Window to Desktop 2", this, SLOT( slotWindowToDesktop( int ) ));
    keys->connectItem( "Window to Desktop 3", this, SLOT( slotWindowToDesktop( int ) ));
    keys->connectItem( "Window to Desktop 4", this, SLOT( slotWindowToDesktop( int ) ));
    keys->connectItem( "Window to Desktop 5", this, SLOT( slotWindowToDesktop( int ) ));
    keys->connectItem( "Window to Desktop 6", this, SLOT( slotWindowToDesktop( int ) ));
    keys->connectItem( "Window to Desktop 7", this, SLOT( slotWindowToDesktop( int ) ));
    keys->connectItem( "Window to Desktop 8", this, SLOT( slotWindowToDesktop( int ) ));
    keys->connectItem( "Window to Desktop 9", this, SLOT( slotWindowToDesktop( int ) ));
    keys->connectItem( "Window to Desktop 10", this, SLOT( slotWindowToDesktop( int ) ));
    keys->connectItem( "Window to Desktop 11", this, SLOT( slotWindowToDesktop( int ) ));
    keys->connectItem( "Window to Desktop 12", this, SLOT( slotWindowToDesktop( int ) ));
    keys->connectItem( "Window to Desktop 13", this, SLOT( slotWindowToDesktop( int ) ));
    keys->connectItem( "Window to Desktop 14", this, SLOT( slotWindowToDesktop( int ) ));
    keys->connectItem( "Window to Desktop 15", this, SLOT( slotWindowToDesktop( int ) ));
    keys->connectItem( "Window to Desktop 16", this, SLOT( slotWindowToDesktop( int ) ));

    keys->connectItem( "Pop-up window operations menu", this, SLOT( slotWindowOperations() ) );
    keys->connectItem( "Window close", this, SLOT( slotWindowClose() ) );
    keys->connectItem( "Window close all", this, SLOT( slotWindowCloseAll() ) );
    keys->connectItem( "Window maximize", this, SLOT( slotWindowMaximize() ) );
    keys->connectItem( "Window maximize horizontal", this, SLOT( slotWindowMaximizeHorizontal() ) );
    keys->connectItem( "Window maximize vertical", this, SLOT( slotWindowMaximizeVertical() ) );
    keys->connectItem( "Window iconify", this, SLOT( slotWindowIconify() ) );
    //keys->connectItem( "Window iconify all", this, SLOT( slotWindowIconifyAll() ) );
    keys->connectItem( "Window shade", this, SLOT( slotWindowShade() ) );
    keys->connectItem( "Window move", this, SLOT( slotWindowMove() ) );
    keys->connectItem( "Window resize", this, SLOT( slotWindowResize() ) );
    keys->connectItem( "Window raise", this, SLOT( slotWindowRaise() ) );
    keys->connectItem( "Window lower", this, SLOT( slotWindowLower() ) );
    keys->connectItem( "Toggle raise and lower", this, SLOT( slotWindowRaiseOrLower() ) );
    keys->connectItem( "Window to next desktop", this, SLOT( slotWindowNextDesktop() ) );
    keys->connectItem( "Window to previous desktop", this, SLOT( slotWindowPreviousDesktop() ) );

    keys->connectItem( "Walk through desktops", this, SLOT( slotWalkThroughDesktops()));
    keys->connectItem( "Walk back through desktops", this, SLOT( slotWalkBackThroughDesktops()));
    keys->connectItem( "Walk through desktop list", this, SLOT( slotWalkThroughDesktopList()));
    keys->connectItem( "Walk back through desktop list", this, SLOT( slotWalkBackThroughDesktopList()));
    keys->connectItem( "Walk through windows",this, SLOT( slotWalkThroughWindows()));
    keys->connectItem( "Walk back through windows",this, SLOT( slotWalkBackThroughWindows()));

    keys->connectItem( "Mouse emulation", this, SLOT( slotMouseEmulation() ) );

    keys->connectItem( "Kill Window", this, SLOT( slotKillWindow() ) );

    keys->connectItem( "Screenshot of active window", this, SLOT( slotGrabWindow() ) );
    keys->connectItem( "Screenshot of desktop", this, SLOT( slotGrabDesktop() ) );

    keys->readSettings();
    walkThroughDesktopsKeycode = keys->currentKey( "Walk through desktops" );
    walkBackThroughDesktopsKeycode = keys->currentKey( "Walk back through desktops" );
    walkThroughDesktopListKeycode = keys->currentKey( "Walk through desktop list" );
    walkBackThroughDesktopListKeycode = keys->currentKey( "Walk back through desktop list" );
    walkThroughWindowsKeycode = keys->currentKey( "Walk through windows" );
    walkBackThroughWindowsKeycode = keys->currentKey( "Walk back through windows" );
    keys->setItemRawModeEnabled( "Walk through desktops", TRUE  );
    keys->setItemRawModeEnabled( "Walk back through desktops", TRUE );
    keys->setItemRawModeEnabled( "Walk through desktop list", TRUE  );
    keys->setItemRawModeEnabled( "Walk back through desktop list", TRUE  );
    keys->setItemRawModeEnabled( "Walk through windows", TRUE  );
    keys->setItemRawModeEnabled( "Walk back through windows", TRUE  );
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

void Workspace::slotSwitchDesktopNext(){
    int d = currentDesktop() + 1;
    if ( d > numberOfDesktops() )
        d = 1;
    setCurrentDesktop(d);
}
void Workspace::slotSwitchDesktopPrevious(){
    int d = currentDesktop() - 1;
    if ( d <= 0 )
        d = numberOfDesktops();
    setCurrentDesktop(d);
}
void Workspace::slotSwitchDesktopRight(){

    int d = currentDesktop() + options->desktopRows;
    if ( d > numberOfDesktops() )
        d -= numberOfDesktops();
    setCurrentDesktop(d);
}
void Workspace::slotSwitchDesktopLeft(){
    int d = currentDesktop() - options->desktopRows;
    if ( d < 1 )
        d += numberOfDesktops();
    setCurrentDesktop(d);
}
void Workspace::slotSwitchDesktopUp(){
    int d = currentDesktop();
    if ( (d-1) % options->desktopRows == 0 )
        d += options->desktopRows - 1;
    else
        d--;
    setCurrentDesktop(d);
}
void Workspace::slotSwitchDesktopDown(){
    int d = currentDesktop();
    if ( d % options->desktopRows == 0 )
        d -= options->desktopRows - 1;
    else
        d++;
    setCurrentDesktop(d);
}

void Workspace::slotSwitchToDesktop( int i )
{
	setCurrentDesktop( i );
}

/*void Workspace::slotSwitchToWindow( int i )
{
	int n = 0;
	for ( ClientList::ConstIterator it = clients.begin(); it != clients.end(); ++it) {
		if( (*it)->isOnDesktop( currentDesktop() ) ) {
			if( n == i ) {
				activateClient( (*it) );
				break;
			}
			n++;
		}
	}
}*/

void Workspace::slotWindowToDesktop( int i )
{
	if( i >= 1 && i <= numberOfDesktops() && popup_client )
		sendClientToDesktop( popup_client, i );
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

// This should probably be removed now that there is a "Show Desktop" binding.
void Workspace::slotWindowIconifyAll()
{
    int iDesktop = currentDesktop();

    for ( ClientList::ConstIterator it = clients.begin(); it != clients.end(); ++it) {
        if( (*it)->isOnDesktop( iDesktop ) && !(*it)->isIconified() )
            performWindowOperation( *it, Options::IconifyOp );
    }
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
  Does a toggle-raise-and-lower on the popup client;
  */
void Workspace::slotWindowRaiseOrLower()
{
    if  ( popup_client )
        raiseOrLowerClient( popup_client );
}

/*!
  Move window to next desktop
 */
void Workspace::slotWindowNextDesktop(){
    int d = currentDesktop() + 1;
    if ( d > numberOfDesktops() )
        d = 1;
    if (popup_client)
      sendClientToDesktop(popup_client,d);
    setCurrentDesktop(d);
}

/*!
  Move window to previous desktop
 */
void Workspace::slotWindowPreviousDesktop(){
    int d = currentDesktop() - 1;
    if ( d <= 0 )
        d = numberOfDesktops();
    if (popup_client)
      sendClientToDesktop(popup_client,d);
    setCurrentDesktop(d);
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
    for ( ; it != stacking_order.end(); --it) {
        Client *client = (*it);
        if ( client->frameGeometry().contains(QPoint(x, y)) &&
             client->isOnDesktop( currentDesktop() ) &&
             !client->isMenu() && !client->isDesktop() &&
             !client->isIconified() ) {
            client->killWindow();
            return;
        }
    }
}

/*!
  Takes a screenshot of the current window and puts it in the clipboard.
*/
void Workspace::slotGrabWindow()
{
    if ( active_client ) {
	QPixmap p = QPixmap::grabWindow( active_client->winId() );
	QClipboard *cb = QApplication::clipboard();
	cb->setPixmap( p );
    }
    else
	slotGrabDesktop();
}

/*!
  Takes a screenshot of the whole desktop and puts it in the clipboard.
*/
void Workspace::slotGrabDesktop()
{
    QPixmap p = QPixmap::grabWindow( qt_xrootwin() );
    QClipboard *cb = QApplication::clipboard();
    cb->setPixmap( p );
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
    const int BASE = 10;
    for ( int i = 1; i <= numberOfDesktops(); i++ ) {
        QString basic_name("%1  %2");
        if (i<BASE) {
            basic_name.prepend('&');
        }
        id = desk_popup->insertItem( basic_name.arg(i).arg( desktopName(i) ), i );
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
    popup->setItemEnabled( Options::ToggleStoreSettingsOp, !popup_client->isTransient() );
    popup->setItemChecked( Options::ToggleStoreSettingsOp, popup_client->storeSettings() );
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

    if ( c->isOnDesktop( currentDesktop() ) ) {
        c->show();
        if ( c->wantsTabFocus() && options->focusPolicyIsReasonable() ) {
            requestFocus( c );
        }
    }
    else {
        c->hide();
        raiseClient( c );
        focus_chain.remove( c );
        if ( c->wantsTabFocus() )
            focus_chain.append( c );
    }

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
    Client* c = active_client;
    p->exec( active_client->mapToGlobal( active_client->windowWrapper()->geometry().topLeft() ) );
    if ( hasClient( c ) )
        requestFocus( c );
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

void Workspace::slotWindowCloseAll()
{
    for ( ClientList::ConstIterator it = clients.begin(); it != clients.end(); ++it) {
        if( (*it)->isOnDesktop( currentDesktop() ) )
            performWindowOperation( *it, Options::CloseOp );
    }
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
   //aleXXX 02Nov2000 added second snapping mode
   if (options->windowSnapZone || options->borderSnapZone )
   {
      bool sOWO=options->snapOnlyWhenOverlapping;
      QRect maxRect = clientArea(MovementArea);
      int xmin = maxRect.left();
      int xmax = maxRect.right()+1;               //desk size
      int ymin = maxRect.top();
      int ymax = maxRect.bottom()+1;

      int cx(pos.x());
      int cy(pos.y());
      int cw(c->width());
      int ch(c->height());
      int rx(cx+cw);
      int ry(cy+ch);                 //these don't change

      int nx(cx), ny(cy);                         //buffers
      int deltaX(xmax);
      int deltaY(ymax);   //minimum distance to other clients

      int lx, ly, lrx, lry; //coords and size for the comparison client, l

      // border snap
      int snap = options->borderSnapZone; //snap trigger
      if (snap)
      {
         if ((sOWO?(cx<xmin):true) && (QABS(xmin-cx)<snap))
         {
            deltaX = xmin-cx;
            nx = xmin;
         }
         if ((sOWO?(rx>xmax):true) && (QABS(rx-xmax)<snap) && (QABS(xmax-rx) < deltaX))
         {
            deltaX = rx-xmax;
            nx = xmax - cw;
         }

         if ((sOWO?(cy<ymin):true) && (QABS(ymin-cy)<snap))
         {
            deltaY = ymin-cy;
            ny = ymin;
         }
         if ((sOWO?(ry>ymax):true) && (QABS(ry-ymax)<snap) && (QABS(ymax-ry) < deltaY))
         {
            deltaY =ry-ymax;
            ny = ymax - ch;
         }
      }

      // windows snap
      snap = options->windowSnapZone;
      if (snap)
      {
         QValueList<Client *>::ConstIterator l;
         for (l = clients.begin();l != clients.end();++l )
         {
            if ((*l)->isOnDesktop(currentDesktop()) && (*l) != desktop_client &&
               !(*l)->isIconified()
#if 0
		&& (*l)->transientFor() == None
#endif
		&& (*l) != c )
            {
               lx = (*l)->x();
               ly = (*l)->y();
               lrx = lx + (*l)->width();
               lry = ly + (*l)->height();

               if ( (( cy <= lry ) && ( cy  >= ly  ))  ||
                    (( ry >= ly  ) && ( ry  <= lry ))  ||
                    (( cy <= ly  ) && ( ry >= lry  )) )
               {
                  if ((sOWO?(cx<lrx):true) && (QABS(lrx-cx)<snap) && ( QABS(lrx -cx) < deltaX) )
                  {
                     deltaX = QABS( lrx - cx );
                     nx = lrx;
                  }
                  if ((sOWO?(rx>lx):true) && (QABS(rx-lx)<snap) && ( QABS( rx - lx )<deltaX) )
                  {
                     deltaX = QABS(rx - lx);
                     nx = lx - cw;
                  }
               }

               if ( (( cx <= lrx ) && ( cx  >= lx  ))  ||
                    (( rx >= lx  ) && ( rx  <= lrx ))  ||
                    (( cx <= lx  ) && ( rx >= lrx  )) )
               {
                  if ((sOWO?(cy<lry):true) && (QABS(lry-cy)<snap) && (QABS( lry -cy ) < deltaY))
                  {
                     deltaY = QABS( lry - cy );
                     ny = lry;
                  }
                  //if ( (QABS( ry-ly ) < snap) && (QABS( ry - ly ) < deltaY ))
                  if ((sOWO?(ry>ly):true) && (QABS(ry-ly)<snap) && (QABS( ry - ly ) < deltaY ))
                  {
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
    case XK_KP_Space: {
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

void Workspace::slotResetAllClientsDelayed()
{
    resetTimer.start(200, true);
}

/*!
  Puts a new decoration frame around every client. Used to react on
  style changes.
 */
void Workspace::slotResetAllClients()
{
    resetTimer.stop();
    ClientList stack = stacking_order;
    Client* active = activeClient();
    block_focus = TRUE;
    Client* prev = 0;
    for (ClientList::Iterator it = stack.fromLast(); it != stack.end(); --it) {
        Client *oldClient = (*it);
        Client::MaximizeMode oldMaxMode = oldClient->maximizeMode();
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
        newClient->maximize(oldMaxMode);
        prev = newClient;
    }
    block_focus = FALSE;
    if ( active )
        requestFocus( active );
    else if( desktop_client )
        requestFocus( desktop_client );
    else
        focusToNull();

    // Add a dcop signal to allow other apps to know when the kwin client
    // has been changed by via the titlebar decoration selection.
    kapp->dcopClient()->emitDCOPSignal("dcopResetAllClients()", QByteArray() );
}


/*
 * Legacy session management
 */

#ifndef NO_LEGACY_SESSION_MANAGEMENT
#define WM_SAVE_YOURSELF_TIMEOUT 4000

typedef QMap<WId,int> WindowMap;
#define HAS_ERROR          0
#define HAS_WMCOMMAND      1
#define HAS_WMSAVEYOURSELF 2

static WindowMap *windowMapPtr = 0;

static int winsErrorHandler(Display *, XErrorEvent *ev)
{
    if (windowMapPtr) {
        WindowMap::Iterator it = windowMapPtr->find(ev->resourceid);
        if (it != windowMapPtr->end())
            it.data() = HAS_ERROR;
    }
    return 0;
}

/*!
  Stores legacy session management data
*/
void Workspace::storeLegacySession( KConfig* config )
{
    // Setup error handler
    WindowMap wins;
    windowMapPtr = &wins;
    XErrorHandler oldHandler = XSetErrorHandler(winsErrorHandler);
    // Compute set of leader windows that need legacy session management
    // and determine which style (WM_COMMAND or WM_SAVE_YOURSELF)
    for (ClientList::Iterator it = clients.begin(); it != clients.end(); ++it) {
        Client* c = (*it);
        WId leader = c->wmClientLeader();
        if (!wins.contains(leader) && c->sessionId().isEmpty()) {
            int wtype = HAS_WMCOMMAND;
            int nprotocols = 0;
            Atom *protocols = 0;
            XGetWMProtocols(qt_xdisplay(), leader, &protocols, &nprotocols);
            for (int i=0; i<nprotocols; i++)
                if (protocols[i] == atoms->wm_save_yourself) {
                    wtype = HAS_WMSAVEYOURSELF;
                    break;
                }
            XFree((void*) protocols);
            wins.insert(leader, wtype);
        }
    }
    // Open fresh display for sending WM_SAVE_YOURSELF
    XSync(qt_xdisplay(), False);
    Display *newdisplay = XOpenDisplay(DisplayString(qt_xdisplay()));
    if (!newdisplay) return;
    WId root = DefaultRootWindow(newdisplay);
    XGrabKeyboard(newdisplay, root, False,
                  GrabModeAsync, GrabModeAsync, CurrentTime);
    XGrabPointer(newdisplay, root, False, Button1Mask|Button2Mask|Button3Mask,
                 GrabModeAsync, GrabModeAsync, None, None, CurrentTime);
    // Send WM_SAVE_YOURSELF messages
    XEvent ev;
    int awaiting_replies = 0;
    for (WindowMap::Iterator it = wins.begin(); it != wins.end(); ++it) {
        if ( it.data() == HAS_WMSAVEYOURSELF ) {
            WId w = it.key();
            awaiting_replies += 1;
            memset(&ev, 0, sizeof(ev));
            ev.xclient.type = ClientMessage;
            ev.xclient.window = w;
            ev.xclient.message_type = atoms->wm_protocols;
            ev.xclient.format = 32;
            ev.xclient.data.l[0] = atoms->wm_save_yourself;
            ev.xclient.data.l[1] = kwin_time;
            XSelectInput(newdisplay, w, PropertyChangeMask|StructureNotifyMask);
            XSendEvent(newdisplay, w, False, 0, &ev);
        }
    }
    // Wait for change in WM_COMMAND with timeout
    XFlush(newdisplay);
    QTime start = QTime::currentTime();
    while (awaiting_replies > 0) {
        if (XPending(newdisplay)) {
            /* Process pending event */
            XNextEvent(newdisplay, &ev);
            if ( ( ev.xany.type == UnmapNotify ) ||
                 ( ev.xany.type == PropertyNotify && ev.xproperty.atom == XA_WM_COMMAND ) ) {
                WindowMap::Iterator it = wins.find( ev.xany.window );
                if ( it != wins.end() && it.data() != HAS_WMCOMMAND ) {
                    awaiting_replies -= 1;
                    if ( it.data() != HAS_ERROR )
                        it.data() = HAS_WMCOMMAND;
                }
            }
        } else {
            /* Check timeout */
            int msecs = start.elapsed();
            if (msecs >= WM_SAVE_YOURSELF_TIMEOUT)
                break;
            /* Wait for more events */
            fd_set fds;
            FD_ZERO(&fds);
            int fd = ConnectionNumber(newdisplay);
            FD_SET(fd, &fds);
            struct timeval tmwait;
            tmwait.tv_sec = (WM_SAVE_YOURSELF_TIMEOUT - msecs) / 1000;
            tmwait.tv_usec = ((WM_SAVE_YOURSELF_TIMEOUT - msecs) % 1000) * 1000;
            ::select(fd+1, &fds, NULL, &fds, &tmwait);
        }
    }
    // Terminate work in new display
    XAllowEvents(newdisplay, ReplayPointer, CurrentTime);
    XAllowEvents(newdisplay, ReplayKeyboard, CurrentTime);
    XSync(newdisplay, False);
    XCloseDisplay(newdisplay);
    // Write LegacySession data
    config->setGroup("LegacySession" );
    int count = 0;
    for (WindowMap::Iterator it = wins.begin(); it != wins.end(); ++it) {
        if (it.data() != HAS_ERROR) {
            WId w = it.key();
            QCString wmCommand = Client::staticWmCommand(w);
            QCString wmClientMachine = Client::staticWmClientMachine(w);
            if ( !wmCommand.isEmpty() && !wmClientMachine.isEmpty() ) {
                count++;
                QString n = QString::number(count);
                config->writeEntry( QString("command")+n, wmCommand.data() );
                config->writeEntry( QString("clientMachine")+n, wmClientMachine.data() );
            }
        }
    }
    config->writeEntry( "count", count );
    // Restore old error handler
    XSync(qt_xdisplay(), False);
    XSetErrorHandler(oldHandler);
    // Process a few events to update the client list.
    // All events should be there because of the XSync above.
    kapp->processEvents(10);
}
#endif

/*!
  Restores legacy session management data (i.e. restart applications)
*/
void Workspace::restoreLegacySession( KConfig* config )
{
    if (config) {
        config->setGroup("LegacySession" );
        int count =  config->readNumEntry( "count" );
        for ( int i = 1; i <= count; i++ ) {
            QString n = QString::number(i);
            QCString wmCommand = config->readEntry( QString("command")+n ).latin1();
            QCString wmClientMachine = config->readEntry( QString("clientMachine")+n ).latin1();
            if ( !wmCommand.isEmpty() && !wmClientMachine.isEmpty() ) {
                KShellProcess proc;
                if ( wmClientMachine != "localhost" )
                    proc << "xon" << wmClientMachine;
                proc << QString::fromLatin1( wmCommand );
                proc.start(KShellProcess::DontCare);
            }
        }
    }
}

/*!
  Stores the current session in the config file

  \sa loadSessionInfo()
 */
void Workspace::storeSession( KConfig* config )
{
#ifndef NO_LEGACY_SESSION_MANAGEMENT
    storeLegacySession(config);
#endif
    config->setGroup("Session" );
    int count =  0;
    for (ClientList::Iterator it = clients.begin(); it != clients.end(); ++it) {
        Client* c = (*it);
        QCString sessionId = c->sessionId();
        QCString wmCommand = c->wmCommand();
        if ( sessionId.isEmpty() )
#ifndef NO_LEGACY_SESSION_MANAGEMENT
            // This is the only connection between the determination of legacy
            // session managed applications (storeLegacySession) and the
            // recollection of the window geometries (this function).
            if ( wmCommand.isEmpty() )
#endif
                continue;
        count++;
        QString n = QString::number(count);
        config->writeEntry( QString("sessionId")+n, sessionId.data() );
        config->writeEntry( QString("windowRole")+n, c->windowRole().data() );
        config->writeEntry( QString("wmCommand")+n, wmCommand.data() );
        config->writeEntry( QString("wmClientMachine")+n, c->wmClientMachine().data() );
        config->writeEntry( QString("resourceName")+n, c->resourceName().data() );
        config->writeEntry( QString("resourceClass")+n, c->resourceClass().data() );
        config->writeEntry( QString("geometry")+n,  QRect( c->pos(), c->windowWrapper()->size() ) );
        config->writeEntry( QString("restore")+n, c->geometryRestore() );
        config->writeEntry( QString("maximize")+n, (int) c->maximizeMode() );
        config->writeEntry( QString("desktop")+n, c->desktop() );
        config->writeEntry( QString("iconified")+n, c->isIconified() );
        config->writeEntry( QString("sticky")+n, c->isSticky() );
        config->writeEntry( QString("shaded")+n, c->isShade() );
        config->writeEntry( QString("staysOnTop")+n, c->staysOnTop() );
        config->writeEntry( QString("skipTaskbar")+n, c->skipTaskbar() );
        config->writeEntry( QString("skipPager")+n, c->skipPager() );
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
        info->wmClientMachine = config->readEntry( QString("wmClientMachine")+n ).latin1();
        info->resourceName = config->readEntry( QString("resourceName")+n ).latin1();
        info->resourceClass = config->readEntry( QString("resourceClass")+n ).latin1();
        info->geometry = config->readRectEntry( QString("geometry")+n );
        info->restore = config->readRectEntry( QString("restore")+n );
        info->maximize = config->readNumEntry( QString("maximize")+n, 0 );
        info->desktop = config->readNumEntry( QString("desktop")+n, 0 );
        info->iconified = config->readBoolEntry( QString("iconified")+n, FALSE );
        info->sticky = config->readBoolEntry( QString("sticky")+n, FALSE );
        info->shaded = config->readBoolEntry( QString("shaded")+n, FALSE );
        info->staysOnTop = config->readBoolEntry( QString("staysOnTop")+n, FALSE  );
 	info->skipTaskbar = config->readBoolEntry( QString("skipTaskbar")+n, FALSE  );
 	info->skipPager = config->readBoolEntry( QString("skipPager")+n, FALSE  );
    }
}

void Workspace::loadFakeSessionInfo()
{
    fakeSession.clear();
    KConfig *config = KGlobal::config();
    config->setGroup("FakeSession" );
    int count =  config->readNumEntry( "count" );
    for ( int i = 1; i <= count; i++ ) {
        QString n = QString::number(i);
        SessionInfo* info = new SessionInfo;
        fakeSession.append( info );
        info->resourceName = config->readEntry( QString("resourceName")+n ).latin1();
        info->resourceClass = config->readEntry( QString("resourceClass")+n ).latin1();
        info->wmClientMachine = config->readEntry( QString("clientMachine")+n ).latin1();
        info->geometry = config->readRectEntry( QString("geometry")+n );
        info->restore = config->readRectEntry( QString("restore")+n );
        info->maximize = config->readNumEntry( QString("maximize")+n, 0 );
        info->desktop = config->readNumEntry( QString("desktop")+n, 0 );
        info->iconified = config->readBoolEntry( QString("iconified")+n, FALSE );
        info->sticky = config->readBoolEntry( QString("sticky")+n, FALSE );
        info->shaded = config->readBoolEntry( QString("shaded")+n, FALSE );
        info->staysOnTop = config->readBoolEntry( QString("staysOnTop")+n, FALSE  );
 	info->skipTaskbar = config->readBoolEntry( QString("skipTaskbar")+n, FALSE  );
 	info->skipPager = config->readBoolEntry( QString("skipPager")+n, FALSE  );
    }
}

void Workspace::storeFakeSessionInfo( Client* c )
{
    if ( !c->storeSettings() )
        return;
    SessionInfo* info = new SessionInfo;
    fakeSession.append( info );
    info->resourceName = c->resourceName();
    info->resourceClass = c->resourceClass();
    info->wmClientMachine = c->wmClientMachine();
    info->geometry = QRect( c->pos(), c->windowWrapper()->size() ) ;
    info->restore = c->geometryRestore();
    info->maximize = (int)c->maximizeMode();
    info->desktop = c->desktop();
    info->iconified = c->isIconified();
    info->sticky = c->isSticky();
    info->shaded = c->isShade();
    info->staysOnTop = c->staysOnTop();
    info->skipTaskbar = c->skipTaskbar();
    info->skipPager = c->skipPager();
}

void Workspace::writeFakeSessionInfo()
{
    KConfig *config = KGlobal::config();
    config->setGroup("FakeSession" );
    int count = 0;
    for ( SessionInfo* info = fakeSession.first(); info; info = fakeSession.next() ) {
        count++;
        QString n = QString::number(count);
        config->writeEntry( QString("resourceName")+n, info->resourceName.data() );
        config->writeEntry( QString("resourceClass")+n, info->resourceClass.data() );
        config->writeEntry( QString("clientMachine")+n, info->wmClientMachine.data() );
        config->writeEntry( QString("geometry")+n,  info->geometry );
        config->writeEntry( QString("restore")+n, info->restore );
        config->writeEntry( QString("maximize")+n, info->maximize );
        config->writeEntry( QString("desktop")+n, info->desktop );
        config->writeEntry( QString("iconified")+n, info->iconified );
        config->writeEntry( QString("sticky")+n, info->sticky );
        config->writeEntry( QString("shaded")+n, info->shaded );
        config->writeEntry( QString("staysOnTop")+n, info->staysOnTop );
        config->writeEntry( QString("skipTaskbar")+n, info->skipTaskbar );
        config->writeEntry( QString("skipPager")+n, info->skipPager );
    }
    config->writeEntry( "count", count );
}

/*!
  Returns a SessionInfo for client \a c. The returned session
  info is removed from the storage. It's up to the caller to delete it.

  This function is called when a new window is mapped and must be managed.
  We try to find a matching entry in the session.  We also try to find
  a matching entry in the fakeSession to see if the user had seclected the
  ``store settings'' menu entry.

  May return 0 if there's no session info for the client.
 */
SessionInfo* Workspace::takeSessionInfo( Client* c )
{
    SessionInfo *realInfo = 0;
    SessionInfo *fakeInfo = 0;
    QCString sessionId = c->sessionId();
    QCString windowRole = c->windowRole();
    QCString wmCommand = c->wmCommand();
    QCString wmClientMachine = c->wmClientMachine();
    QCString resourceName = c->resourceName();
    QCString resourceClass = c->resourceClass();

    // First search ``session''
    if (! sessionId.isEmpty() ) {
        // look for a real session managed client (algorithm suggested by ICCCM)
        for (SessionInfo* info = session.first(); info && !realInfo; info = session.next() )
            if ( info->sessionId == sessionId ) {
                if ( ! windowRole.isEmpty() ) {
                    if ( info->windowRole == windowRole )
                        realInfo = session.take();
                } else {
                    if ( info->windowRole.isEmpty() &&
                         info->resourceName == resourceName &&
                         info->resourceClass == resourceClass )
                        realInfo = session.take();
                }
            }
    } else {
        // look for a sessioninfo with matching features.
        for (SessionInfo* info = session.first(); info && !realInfo; info = session.next() )
            if ( info->resourceName == resourceName &&
                 info->resourceClass == resourceClass &&
                 info->wmClientMachine == wmClientMachine )
                if ( wmCommand.isEmpty() || info->wmCommand == wmCommand )
                    realInfo = session.take();
    }

    // Now search ``fakeSession''
    for (SessionInfo* info = fakeSession.first(); info && !fakeInfo; info = fakeSession.next() )
        if ( info->resourceName == resourceName &&
             info->resourceClass == resourceClass &&
             info->wmClientMachine == wmClientMachine )
            fakeInfo = fakeSession.take();

    // Reconciliate
    if (fakeInfo)
        c->setStoreSettings( TRUE );
    if (fakeInfo && realInfo)
        delete fakeInfo;
    if (realInfo)
        return realInfo;
    if (fakeInfo)
        return fakeInfo;
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
        {
            rootInfo->setWorkArea( i, r );
        }

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
QRect Workspace::clientArea(clientAreaOption opt)
{
    QRect rect = QApplication::desktop()->geometry();
#if QT_VERSION < 300
    KDesktopWidget *desktop = KApplication::desktop();
#else
    QDesktopWidget *desktop = KApplication::desktop();
#endif

    switch (opt) {
	case MaximizeArea:
	    if (options->xineramaMaximizeEnabled)
		rect = desktop->screenGeometry(desktop->screenNumber(QCursor::pos()));
	    break;
	case PlacementArea:
	    if (options->xineramaPlacementEnabled)
		rect = desktop->screenGeometry(desktop->screenNumber(QCursor::pos()));
	    break;
	case MovementArea:
	    if (options->xineramaMovementEnabled)
		rect = desktop->screenGeometry(desktop->screenNumber(QCursor::pos()));
	    break;
    }
    if (area.isNull()) {
	return rect;
    }
    return area.intersect(rect);
}

// ### KDE3: remove me!
QRect Workspace::clientArea()
{
    return clientArea( MaximizeArea );
}

void Workspace::loadDesktopSettings()
{
    KConfig c("kdeglobals");

    QCString groupname;
    if (kwin_screen_number == 0)
        groupname = "Desktops";
    else
        groupname.sprintf("Desktops-screen-%d", kwin_screen_number);
    c.setGroup(groupname);

    int n = c.readNumEntry("Number", 4);
    number_of_desktops = n;
    rootInfo->setNumberOfDesktops( number_of_desktops );
    desktop_focus_chain.resize( n );
    for(int i = 1; i <= n; i++) {
        QString s = c.readEntry(QString("Name_%1").arg(i),
                                i18n("Desktop %1").arg(i));
        rootInfo->setDesktopName( i, s.utf8().data() );
        desktop_focus_chain[i-1] = i;
    }
}

void Workspace::saveDesktopSettings()
{
    KConfig c("kdeglobals");

    QCString groupname;
    if (kwin_screen_number == 0)
        groupname = "Desktops";
    else
        groupname.sprintf("Desktops-screen-%d", kwin_screen_number);
    c.setGroup(groupname);

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


/*!
  Checks whether focus is in nirvana, and activates a client instead.
 */
void Workspace::focusEnsurance()
{
    Window focus;
    int revert;
    XGetInputFocus( qt_xdisplay(), &focus, &revert );
    if ( focus == None || focus == PointerRoot ) {
	
	Window root_return;
	Window child = root;
	int root_x, root_y, lx, ly;
	uint state;
	Window w;
	if ( ! XQueryPointer( qt_xdisplay(), root, &root_return, &child,
			      &root_x, &root_y, &lx, &ly, &state ) )
	    return; // cursor is on another screen, so do not play with focus
	
	if ( !last_active_client )
	    last_active_client = topClientOnDesktop();
	if ( last_active_client && last_active_client->isVisible() ) {
	    kwin_time = CurrentTime;
	    requestFocus( last_active_client );
	}
    }
}

void Workspace::configureWM()
{
    QStringList args;
    args << "kwinoptions" << "kwindecoration";
    KApplication::kdeinitExec( "kcmshell", args );
}

void Workspace::checkStartOnDesktop( WId w )
{
    KStartupInfoData data;
    if( d->startup->checkStartup( w, data ) != KStartupInfo::Match || data.desktop() == 0 )
        return;
    NETWinInfo info( qt_xdisplay(),  w, root, NET::WMDesktop );
    if( info.desktop() == 0 )
        info.setDesktop( data.desktop());
}

// Electric Borders
//========================================================================//
// Electric Border Window management. Electric borders allow a user
// to change the virtual desktop by moving the mouse pointer to the
// borders. Technically this is done with input only windows. Since
// electric borders can be switched on and off, we have these two
// functions to create and destroy them.
void Workspace::createBorderWindows()
{
    if ( d->electric_have_borders )
        return;

    d->electric_have_borders = true;
    d->electric_current_border = None;

    QRect r = QApplication::desktop()->geometry();

    XSetWindowAttributes attributes;
    unsigned long valuemask;
    attributes.override_redirect = True;
    attributes.event_mask =  (EnterWindowMask | LeaveWindowMask |
			      VisibilityChangeMask);
    valuemask=  (CWOverrideRedirect | CWEventMask | CWCursor );
    attributes.cursor = XCreateFontCursor(qt_xdisplay(),
					  XC_sb_up_arrow);
    d->electric_top_border = XCreateWindow (qt_xdisplay(), qt_xrootwin(),
				0,0,
				r.width(),1,
				0,
				CopyFromParent, InputOnly,
				CopyFromParent,
				valuemask, &attributes);
    XMapWindow(qt_xdisplay(), d->electric_top_border);

    attributes.cursor = XCreateFontCursor(qt_xdisplay(),
					  XC_sb_down_arrow);
    d->electric_bottom_border = XCreateWindow (qt_xdisplay(), qt_xrootwin(),
				   0,r.height()-1,
				   r.width(),1,
				   0,
				   CopyFromParent, InputOnly,
				   CopyFromParent,
				   valuemask, &attributes);
    XMapWindow(qt_xdisplay(), d->electric_bottom_border);

    attributes.cursor = XCreateFontCursor(qt_xdisplay(),
					  XC_sb_left_arrow);
    d->electric_left_border = XCreateWindow (qt_xdisplay(), qt_xrootwin(),
				 0,0,
				 1,r.height(),
				 0,
				 CopyFromParent, InputOnly,
				 CopyFromParent,
				 valuemask, &attributes);
    XMapWindow(qt_xdisplay(), d->electric_left_border);

    attributes.cursor = XCreateFontCursor(qt_xdisplay(),
					  XC_sb_right_arrow);
    d->electric_right_border = XCreateWindow (qt_xdisplay(), qt_xrootwin(),
				  r.width()-1,0,
				  1,r.height(),
				  0,
				  CopyFromParent, InputOnly,
				  CopyFromParent,
				  valuemask, &attributes);
    XMapWindow(qt_xdisplay(),  d->electric_right_border);
}


// Electric Border Window management. Electric borders allow a user
// to change the virtual desktop by moving the mouse pointer to the
// borders. Technically this is done with input only windows. Since
// electric borders can be switched on and off, we have these two
// functions to create and destroy them.
void Workspace::destroyBorderWindows()
{
  if( !d->electric_have_borders)
    return;

  d->electric_have_borders = false;

  if(d->electric_top_border)
    XDestroyWindow(qt_xdisplay(),d->electric_top_border);
  if(d->electric_bottom_border)
    XDestroyWindow(qt_xdisplay(),d->electric_bottom_border);
  if(d->electric_left_border)
    XDestroyWindow(qt_xdisplay(),d->electric_left_border);
  if(d->electric_right_border)
    XDestroyWindow(qt_xdisplay(),d->electric_right_border);

  d->electric_top_border    = None;
  d->electric_bottom_border = None;
  d->electric_left_border   = None;
  d->electric_right_border  = None;
}

// Do we have a proper timediff function??
static int TimeDiff(Time a, Time b)
{
   if (a > b)
     return a-b;
   else
     return b-a;
}

// this function is called when the user entered an electric border
// with the mouse. It may switch to another virtual desktop
void Workspace::electricBorder(XEvent *e)
{
  Window border = e->xcrossing.window;
  Time now = e->xcrossing.time;
  int treshold_set = 150; // set timeout
  int treshold_reset = 150; // reset timeout

  if ((d->electric_current_border == border) &&
      (TimeDiff(d->electric_time_last, now) < treshold_reset))
  {
     d->electric_time_last = now;

     if (TimeDiff(d->electric_time_first, now) > treshold_set)
     {
        d->electric_current_border = 0;

        QRect r = QApplication::desktop()->geometry();
        int offset;
        
        if (border == d->electric_top_border){
           offset = r.height() / 3;
           QCursor::setPos(e->xcrossing.x_root, r.height() - offset);
           slotSwitchDesktopUp();
        }
        else if (border == d->electric_bottom_border){
           offset = r.height() / 3;
           QCursor::setPos(e->xcrossing.x_root, offset);
           slotSwitchDesktopDown();
        }
        else if (border == d->electric_left_border){
           offset = r.width() / 3;
           QCursor::setPos(r.width() - offset, e->xcrossing.y_root);
           slotSwitchDesktopLeft();
        }
        else if (border == d->electric_right_border) {
           offset = r.width() / 3;
           QCursor::setPos(offset, e->xcrossing.y_root);
           slotSwitchDesktopRight();
        }
        return;
     }
  }
  else {
    d->electric_current_border = border;
    d->electric_time_first = now;
    d->electric_time_last = now;
  }

  int mouse_warp = 1;

  // reset the pointer to find out wether the user is really pushing
  if (d->electric_current_border == d->electric_top_border){
    QCursor::setPos(e->xcrossing.x_root, e->xcrossing.y_root+mouse_warp);
  }
  else if (d->electric_current_border == d->electric_bottom_border){
    QCursor::setPos(e->xcrossing.x_root, e->xcrossing.y_root-mouse_warp);
  }
  else if (d->electric_current_border == d->electric_left_border){
    QCursor::setPos(e->xcrossing.x_root+mouse_warp, e->xcrossing.y_root);
  }
  else if (d->electric_current_border == d->electric_right_border){
    QCursor::setPos(e->xcrossing.x_root-mouse_warp, e->xcrossing.y_root);
  }
}

// electric borders (input only windows) have to be always on the
// top. For that reason kwm calls this function always after some
// windows have been raised.
void Workspace::raiseElectricBorders(){

  if(d->electric_have_borders){
    XRaiseWindow(qt_xdisplay(), d->electric_top_border);
    XRaiseWindow(qt_xdisplay(), d->electric_left_border);
    XRaiseWindow(qt_xdisplay(), d->electric_bottom_border);
    XRaiseWindow(qt_xdisplay(), d->electric_right_border);
  }
}



#include "workspace.moc"
