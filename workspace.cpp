/*****************************************************************
kwin - the KDE window manager

Copyright (C) 1999, 2000 Matthias Ettrich <ettrich@kde.org>
******************************************************************/

//#define QT_CLEAN_NAMESPACE

#include <config.h>
#include <kconfig.h>
#include <kglobal.h>
#include <kglobalsettings.h>
#include <kglobalaccel.h>
#include <kkeynative.h>
#include <klocale.h>
#include <stdlib.h>
#include <qwhatsthis.h>
#include <qdatastream.h>
#include <qregexp.h>
#include <qclipboard.h>
#include <kapplication.h>
#include <dcopclient.h>
#include <kprocess.h>
#include <kiconloader.h>
#include <kstartupinfo.h>
#include <qdesktopwidget.h>
#include "placement.h"
#include "workspace.h"
#include "client.h"
#include "tabbox.h"
#include "popupinfo.h"
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
#include <sys/time.h>
#ifdef HAVE_SYS_SELECT_H
#include <sys/select.h>
#endif
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

const int XIconicState = IconicState;
#undef IconicState

#include <kwin.h>
#include <kdebug.h>

namespace KWinInternal {

// NET WM Protocol handler class
class RootInfo : public NETRootInfo
{
public:
    RootInfo( KWinInternal::Workspace* ws, Display *dpy, Window w, const char *name, unsigned long pr[], int pr_num, int scr= -1)
        : NETRootInfo( dpy, w, name, pr, pr_num, scr ) {
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
    void moveResize(Window w, int x_root, int y_root, unsigned long direction) {
        KWinInternal::Client* c = workspace->findClient( (WId) w );
        if ( c ) {
            c->NETMoveResize( x_root, y_root, (Direction)direction);
        }
    }

private:
    KWinInternal::Workspace* workspace;
};

class WorkspacePrivate
{
public:
    WorkspacePrivate()
     : startup(0), electric_have_borders(false),
       electric_current_border(0),
       electric_top_border(None),
       electric_bottom_border(None),
       electric_left_border(None),
       electric_right_border(None),
       electric_time_first(0),
       electric_time_last(0),
       movingClient(0),
       layoutOrientation(Qt::Vertical),
       layoutX(-1),
       layoutY(2),
       workarea(NULL)
    { };
    ~WorkspacePrivate() { delete[] workarea; };
    KStartupInfo* startup;
    bool electric_have_borders;
    int electric_current_border;
    WId electric_top_border;
    WId electric_bottom_border;
    WId electric_left_border;
    WId electric_right_border;
    int electricLeft;
    int electricRight;
    int electricTop;
    int electricBottom;
    Time electric_time_first;
    Time electric_time_last;
    QPoint electric_push_point;
    Client *movingClient;
    Qt::Orientation layoutOrientation;
    int layoutX;
    int layoutY;
    Placement *initPositioning;
    QRect* workarea; //  array of workareas for virtual desktops
};

};

using namespace KWinInternal;

extern int kwin_screen_number;

QString Workspace::desktopName( int desk )
{
    return QString::fromUtf8( rootInfo->desktopName( desk ) );
}

extern Time qt_x_time;
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
    int boundingShaped = 0, clipShaped = 0;
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
        if ( hints->flags & MWM_HINTS_DECORATIONS ) {
            if ( hints->decorations == 0 )
                result = TRUE;
        }
        XFree( data );
    }
    return result;
}

bool Motif::funcFlags( WId w, bool& resize, bool& move, bool& minimize,
    bool& maximize, bool& close )
{
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
    if ( hints ) {
    // To quote from Metacity 'We support those MWM hints deemed non-stupid'
        if ( hints->flags & MWM_HINTS_FUNCTIONS ) {
            // if MWM_FUNC_ALL is set, other flags say what to turn _off_
            bool set_value = (( hints->functions & MWM_FUNC_ALL ) == 0 );
            resize = move = minimize = maximize = close = !set_value;
            if( hints->functions & MWM_FUNC_RESIZE )
                resize = set_value;
            if( hints->functions & MWM_FUNC_MOVE )
                move = set_value;
            if( hints->functions & MWM_FUNC_MINIMIZE )
                minimize = set_value;
            if( hints->functions & MWM_FUNC_MAXIMIZE )
                maximize = set_value;
            if( hints->functions & MWM_FUNC_CLOSE )
                close = set_value;
            XFree( data );
            return true;
        }
        XFree( data );
    }
    return false;
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
    // when adding new window types, add a fallback for them in PluginMgr::createClient()
    case NET::Desktop:
        {
            XLowerWindow( qt_xdisplay(), w );
            Client * c = new NoBorderClient( this, w);
            c->setSticky( TRUE ); // TODO remove this?
            return c;
        }

    case NET::Dock:
        {
            Client * c = new NoBorderClient( this, w );
            return c;
        }

    case NET::TopMenu:
        {
            Client* c = new NoBorderClient( this, w );
            c->setStaysOnTop( true );
            return c;
        }

    case NET::Override:
        return new NoBorderClient( this, w);

    case NET::Menu:
        {
            Window dummy1;
            int x, y;
            unsigned int width, height, dummy2, dummy3;
            XGetGeometry( qt_xdisplay(), w, &dummy1, &x, &y, &width, &height,
                &dummy2, &dummy3 );
            // ugly hack to support the times when NET::Menu meant NET::TopMenu
            // if it's as wide as the screen, not very high and has its upper-left
            // corner a bit above the screen's upper-left cornet, it's a topmenu
            if( x == 0 && y < 0 && y > -10 && height < 100
                && abs( int(width) - geometry().width()) < 10 ) {
                Client* c = new NoBorderClient( this, w);
                c->setStaysOnTop( true );
                return c;
            }
        break;
        }
    case NET::Tool:
        break;
    default:
        break;
    }

    if ( Shape::hasShape( w ) ){
        return new NoBorderClient( this, w );
    }
    return ( mgr->createClient( this, w, ni.windowType()) );
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
    popup_client      (0),
    desktop_widget    (0),
    active_client     (0),
    last_active_client     (0),
    should_get_focus  (0),
    most_recently_raised (0),
    control_grab      (false),
    tab_grab          (false),
    mouse_emulation   (false),
    focus_change      (true),
    tab_box           (0),
    popupinfo         (0),
    popup             (0),
    desk_popup        (0),
    keys              (0),
    root              (0)
{
    _self = this;
    d = new WorkspacePrivate;
    mgr = new PluginMgr;
    root = qt_xrootwin();
    default_colormap = DefaultColormap(qt_xdisplay(), qt_xscreen() );
    installed_colormap = default_colormap;
    session.setAutoDelete( TRUE );

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
    d->startup = new KStartupInfo( false, true, this, NULL );

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

    initShortcuts();
    tab_box = new TabBox( this );
    popupinfo = new PopupInfo( );

    init();
}


void Workspace::init()
{
    QRect r = QApplication::desktop()->geometry();
    d->electricTop = r.top();
    d->electricBottom = r.bottom();
    d->electricLeft = r.left();
    d->electricRight = r.right();
    d->electric_current_border = 0;

    if (options->electricBorders() == Options::ElectricAlways)
       createBorderWindows();

    supportWindow = new QWidget;

    unsigned long protocols[ 3 ] =
        {
        NET::Supported |
        NET::SupportingWMCheck |
        NET::ClientList |
        NET::ClientListStacking |
        NET::DesktopGeometry |
        NET::NumberOfDesktops |
        NET::CurrentDesktop |
        NET::ActiveWindow |
        NET::WorkArea |
        NET::CloseWindow |
        NET::DesktopNames |
        NET::KDESystemTrayWindows |
        NET::WMName |
        NET::WMVisibleName |
        NET::WMDesktop |
        NET::WMWindowType |
        NET::WMState |
        NET::WMStrut |
        NET::WMIconGeometry |
        NET::WMIcon |
        NET::WMPid |
        NET::WMMoveResize |
        NET::WMKDESystemTrayWinFor |
        NET::WMKDEFrameStrut
        ,
        NET::NormalMask |
        NET::DesktopMask |
        NET::DockMask |
        NET::ToolbarMask |
        NET::MenuMask |
        NET::DialogMask |
        NET::OverrideMask |
        NET::TopMenuMask |
//        NET::UtilityMask |  TODO
//        NET::SplashMask | TODO
        0
        ,
//        NET::Modal | TODO
//        NET::Sticky |
        NET::MaxVert |
        NET::MaxHoriz |
        NET::Shaded |
        NET::SkipTaskbar |
        NET::KeepAbove |
//        NET::StaysOnTop |  the same like KeepAbove
        NET::SkipPager |
//        NET::Hidden |  TODO
//        NET::FullScreen | TODO
//        NET::KeepBelow | TODO
        0
        };
        
    rootInfo = new RootInfo( this, qt_xdisplay(), supportWindow->winId(), "KWin",
        protocols, 3, qt_xscreen() );

    loadDesktopSettings();
    setCurrentDesktop( 1 );

    // now we know how many desktops we'll, thus, we initialise the positioning object
    d->initPositioning = new Placement(this);

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
            SLOT(slotReconfigure()));
    connect(kapp, SIGNAL(settingsChanged(int)), this,
            SLOT(slotSettingsChanged(int)));

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
            addClient( c );
            c->manage( TRUE );
            if ( !c->wantsTabFocus() )
                focus_chain.remove( c );
            if ( root != qt_xrootwin() ) {
                // TODO may use QWidget:.create
                XReparentWindow( qt_xdisplay(), c->winId(), root, 0, 0 );
                c->move(0,0);
            }
        }
    }
    if ( wins )
        XFree((void *) wins);
    propagateClients( false, false );

    updateClientArea();
    raiseElectricBorders();

    // NETWM spec says we have to set it to (0,0) if we don't support it
    NETPoint* viewports = new NETPoint[ number_of_desktops ];
    rootInfo->setDesktopViewport( number_of_desktops, *viewports );
    delete[] viewports;
    QRect geom = QApplication::desktop()->geometry();
    NETSize desktop_geometry;
    desktop_geometry.width = geom.width();
    desktop_geometry.height = geom.height();
    // TODO update also after gaining XRANDR support
    rootInfo->setDesktopGeometry( -1, desktop_geometry );
}

Workspace::~Workspace()
{
    for ( ClientList::ConstIterator it = desktops.fromLast(); it != desktops.end(); --it) {
        WId win = (*it)->window();
        delete (*it);
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
    delete popupinfo;
    delete popup;
    if ( root == qt_xrootwin() )
        XDeleteProperty(qt_xdisplay(), qt_xrootwin(), atoms->kwin_running);

    writeFakeSessionInfo();
    KGlobal::config()->sync();

    delete rootInfo;
    delete supportWindow;
    delete mgr;
    delete d->startup;
    delete d->initPositioning;
    delete d;
    _self = 0;
}


/*!
  Handles workspace specific XEvents
 */
bool Workspace::workspaceEvent( XEvent * e )
{
    if ( mouse_emulation && (e->type == ButtonPress || e->type == ButtonRelease ) ) {
        mouse_emulation = FALSE;
        XUngrabKeyboard( qt_xdisplay(), qt_x_time );
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
            XUngrabKeyboard(qt_xdisplay(), qt_x_time);
            XUngrabPointer( qt_xdisplay(), qt_x_time);
            tab_box->hide();
            keys->setEnabled( true );
            tab_grab = control_grab = false;
            return TRUE;
        }
    case ButtonRelease:
    case MotionNotify:
        break;

    case CreateNotify:
        if ( e->xcreatewindow.parent == root &&
             !QWidget::find( e->xcreatewindow.window) &&
             !e->xcreatewindow.override_redirect ) {
            timeval tv;
            gettimeofday( &tv, NULL );
            unsigned long now = tv.tv_sec * 10 + tv.tv_usec / 100000;
            XChangeProperty(qt_xdisplay(), e->xcreatewindow.window,
                            atoms->kde_net_user_time, XA_CARDINAL,
                            32, PropModeReplace, (unsigned char *)&now, 1);
        }
        break;
    case UnmapNotify:
        // this is special due to
        // SubstructureNotifyMask. e->xany.window is the window the
        // event is reported to. Take care not to confuse Qt.
        c = findClient( e->xunmap.window );

        if ( c )
            return c->windowEvent( e );

        // check for system tray windows
        if ( removeSystemTrayWin( e->xunmap.window ) ) {
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
					 ReparentNotify, &ev) ){
		if ( ev.xreparent.parent != root ) {
		    XReparentWindow( qt_xdisplay(), w, root, 0, 0 );
		    addSystemTrayWin( w );
		}
	    }
	    return TRUE;
        }

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
            if ( e->xmaprequest.parent == root ) { //###TODO store previously destroyed client ids
                if ( addSystemTrayWin( e->xmaprequest.window ) )
                    return TRUE;
                c = clientFactory( e->xmaprequest.window );
                if ( root != qt_xrootwin() ) {
                    // TODO may use QWidget:.create
                    XReparentWindow( qt_xdisplay(), c->winId(), root, 0, 0 );
                }
                addClient( c );
            }
        }
        if ( c ) {
            bool b =  c->windowEvent( e );
            if ( !c->wantsTabFocus() || c->isWithdrawn() )
                focus_chain.remove( c );
            return b;
        }
        break;
    case EnterNotify:
        if ( QWhatsThis::inWhatsThisMode() )
        {
            QWidget* w = QWidget::find( e->xcrossing.window );
            if ( w )
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
        c = findClientWithId( e->xcrossing.window );
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

    for ( ClientList::ConstIterator it = desktops.begin(); it != desktops.end(); ++it) {
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
    for ( ClientList::ConstIterator it = desktops.begin(); it != desktops.end(); ++it) {
        if ( (*it)->window() == w )
            return *it;
    }
    return 0;
}

/*!
  Finds the client with window id \a w
 */
Client* Workspace::findClientWithId( WId w ) const
{
    for ( ClientList::ConstIterator it = clients.begin(); it != clients.end(); ++it) {
        if ( (*it)->winId() == w )
            return *it;
    }
    for ( ClientList::ConstIterator it = desktops.begin(); it != desktops.end(); ++it) {
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
    desktops.remove( c );
    propagateClients( false, false ); // no need for XRestack
}

/*
  Destroys the client \a c
 */
bool Workspace::destroyClient( Client* c)
{
    if ( !c )
        return FALSE;

    if (c == active_client && popup)
        popup->close();
    if( c == popup_client )
        popup_client = 0;

    storeFakeSessionInfo( c );

    removeClient(c);

    c->invalidateWindow();
    clientHidden( c );
    if ( desktops.contains(c) )
        desktops.remove(c);
    if ( c == most_recently_raised )
        most_recently_raised = 0;
    if ( c == should_get_focus )
        should_get_focus = 0;
    Q_ASSERT( c != active_client );
    if ( c == last_active_client )
        last_active_client = 0;
    delete c;

    if (tab_grab)
       tab_box->repaint();

    updateClientArea();
    return TRUE;
}



/*!
  Handles alt-tab / control-tab
 */

#include <stdarg.h>

static
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

static
bool areModKeysDepressed( const KShortcut& cut )
{

    uint rgKeySyms[10];
    int nKeySyms = 0;
    int mod = cut.seq(0).key(0).modFlags();

    if ( mod & KKey::SHIFT ) {
        rgKeySyms[nKeySyms++] = XK_Shift_L;
        rgKeySyms[nKeySyms++] = XK_Shift_R;
    }
    if ( mod & KKey::CTRL ) {
        rgKeySyms[nKeySyms++] = XK_Control_L;
        rgKeySyms[nKeySyms++] = XK_Control_R;
    }
    if( mod & KKey::ALT ) {
        rgKeySyms[nKeySyms++] = XK_Alt_L;
        rgKeySyms[nKeySyms++] = XK_Alt_R;
    }
    if( mod & KKey::WIN ) {
        // HACK: it would take a lot of code to determine whether the Win key
        //  is associated with Super or Meta, so check for both
        rgKeySyms[nKeySyms++] = XK_Super_L;
        rgKeySyms[nKeySyms++] = XK_Super_R;
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
    if ( tab_grab || control_grab )
        return;
    if ( options->altTabStyle == Options::CDE  || !options->focusPolicyIsReasonable() ) {
        //XUngrabKeyboard(qt_xdisplay(), qt_x_time); // need that because of accelerator raw mode
        // CDE style raise / lower
        CDEWalkThroughWindows( true );
    } else {
        if ( areModKeysDepressed( cutWalkThroughWindows ) ) {
            if ( startKDEWalkThroughWindows() )
                KDEWalkThroughWindows( true );
        }
        else
            // if the shortcut has no modifiers, don't show the tabbox,
            // don't grab, but simply go to the next window
            // use the CDE style, because with KDE style it would cycle
            // between the active and previously active window
            CDEWalkThroughWindows( true );
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
        CDEWalkThroughWindows( false );
    } else {
        if ( areModKeysDepressed( cutWalkThroughWindowsReverse ) ) {
            if ( startKDEWalkThroughWindows() )
                KDEWalkThroughWindows( false );
        } else {
            CDEWalkThroughWindows( false );
        }
    }
}

void Workspace::slotWalkThroughDesktops()
{
    if ( root != qt_xrootwin() )
        return;
    if( tab_grab || control_grab )
        return;
    if ( areModKeysDepressed( cutWalkThroughDesktops ) ) {
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
    if ( areModKeysDepressed( cutWalkThroughDesktopsReverse ) ) {
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
    if ( areModKeysDepressed( cutWalkThroughDesktopList ) ) {
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
    if ( areModKeysDepressed( cutWalkThroughDesktopListReverse ) ) {
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
                       None, None, qt_x_time ) != GrabSuccess ) {
        return FALSE;
    }
    if ( XGrabKeyboard(qt_xdisplay(),
                       root, FALSE,
                       GrabModeAsync, GrabModeAsync,
                       qt_x_time) != GrabSuccess ) {
        XUngrabPointer( qt_xdisplay(), qt_x_time);
        return FALSE;
    }
    tab_grab        = TRUE;
    keys->setEnabled( false );
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
                       None, None, qt_x_time ) != GrabSuccess ) {
        return FALSE;
    }
    if ( XGrabKeyboard(qt_xdisplay(),
                       root, FALSE,
                       GrabModeAsync, GrabModeAsync,
                       qt_x_time) != GrabSuccess ) {
        XUngrabPointer( qt_xdisplay(), qt_x_time);
        return FALSE;
    }
    control_grab = TRUE;
    keys->setEnabled( false );
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
                  !nc->isNormal() || !nc->wantsTabFocus() ) );
    }
    else
        do {
            nc = nextStaticClient(nc);
        } while (nc && nc != c &&
                 (!nc->isOnDesktop(currentDesktop()) ||
                  !nc->isNormal() || !nc->wantsTabFocus() ) );
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
bool Workspace::keyPress(XKeyEvent& ev)
{
    if ( root != qt_xrootwin() )
        return FALSE;

    KKeyNative keyX( (XEvent*)&ev );
    uint keyQt = keyX.keyCodeQt();

    kdDebug(125) << "Workspace::keyPress( " << keyX.key().toString() << " )" << endl;
    if (d->movingClient)
    {
        d->movingClient->keyPressEvent(keyQt);
        return TRUE;
    }

    if (tab_grab){
        bool forward = cutWalkThroughWindows.contains( keyX );
        bool backward = cutWalkThroughWindowsReverse.contains( keyX );
        if (forward || backward){
            kdDebug(125) << "== " << cutWalkThroughWindows.toStringInternal()
                << " or " << cutWalkThroughWindowsReverse.toStringInternal() << endl;
            KDEWalkThroughWindows( forward );
        }
    }
    else if (control_grab){
        bool forward = cutWalkThroughDesktops.contains( keyX ) ||
                       cutWalkThroughDesktopList.contains( keyX );
        bool backward = cutWalkThroughDesktopsReverse.contains( keyX ) ||
                        cutWalkThroughDesktopListReverse.contains( keyX );
        if (forward || backward)
            walkThroughDesktops(forward);
    }

    if (control_grab || tab_grab){
        if ((keyQt & 0xffff) == Qt::Key_Escape){
            XUngrabKeyboard(qt_xdisplay(), qt_x_time);
            XUngrabPointer( qt_xdisplay(), qt_x_time);
            tab_box->hide();
            keys->setEnabled( true );
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
bool Workspace::keyRelease(XKeyEvent& ev)
{
    if ( root != qt_xrootwin() )
        return FALSE;
    if( !tab_grab && !control_grab )
        return FALSE;
    unsigned int mk = ev.state &
        (KKeyNative::modX(KKey::SHIFT) |
         KKeyNative::modX(KKey::CTRL) |
         KKeyNative::modX(KKey::ALT) |
         KKeyNative::modX(KKey::WIN));
    // ev.state is state before the key release, so just checking mk being 0 isn't enough
    // using XQueryPointer() also doesn't seem to work well, so the check that all
    // modifiers are released: only one modifier is active and the currently released
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
                == ev.keycode)
                release = true;
        XFreeModifiermap(xmk);
    }
    if( !release )
         return FALSE;
    if (tab_grab){
                XUngrabPointer( qt_xdisplay(), qt_x_time);
                XUngrabKeyboard(qt_xdisplay(), qt_x_time);
                tab_box->hide();
                keys->setEnabled( true );
                tab_grab = false;
                if ( tab_box->currentClient() ){
                    activateClient( tab_box->currentClient() );
                }
    }
    if (control_grab){
                XUngrabPointer( qt_xdisplay(), qt_x_time);
                XUngrabKeyboard(qt_xdisplay(), qt_x_time);
                tab_box->hide();
                keys->setEnabled( true );
                control_grab = False;
                if ( tab_box->currentDesktop() != -1 ) {
                    setCurrentDesktop( tab_box->currentDesktop() );
                    // popupinfo->showInfo( desktopName(currentDesktop()) ); // AK - not sure
                }
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

void Workspace::circulateDesktopApplications()
{
    if ( desktops.count() <= 1 )
        return;
    Client* first = desktops.first();
    desktops.remove( first );
    desktops.append( first );
    Window* new_stack = new Window[ desktops.count() + 1 ];
    int i = 0;
    for ( ClientList::ConstIterator it = desktops.fromLast(); it != desktops.end(); --it)
        new_stack[i++] = (*it)->winId();
    XRestackWindows(qt_xdisplay(), new_stack, i);
    delete [] new_stack;
    propagateClients( true, false );
}

void Workspace::addClient( Client* c )
{
    if ( c->isDesktop() ) {
        if ( !desktops.isEmpty() ) {
            Client* first = desktops.first();
            Window stack[2];
            stack[0] = first->winId();
            stack[1] = c->winId();
            XRestackWindows(  qt_xdisplay(), stack, 2 );
            desktops.prepend( c );
            circulateDesktopApplications();
        } else {
            c->lower();
            desktops.append( c );
        }
    } else {
        if ( c->wantsTabFocus() )
            focus_chain.append( c );
        clients.append( c );
        stacking_order.append( c );
    }
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
    if ( !c || clients.isEmpty() )
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
    if ( !c || clients.isEmpty() )
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
    if( popup && popup_client != c ) {
        popup->close();
        popup_client = 0;
    }
    if ( active_client ) {
        active_client->setActive( FALSE );
        if ( active_client->isFullScreen() && active_client->staysOnTop()
             && c && c->mainClient() != active_client->mainClient() ) {
            active_client->setStaysOnTop( FALSE );
            lowerClient( active_client );
        }
    }
    active_client = c;
    last_active_client = active_client;
    if ( active_client ) {
        if ( active_client->isFullScreen() && !active_client->staysOnTop() ) {
            active_client->setStaysOnTop( TRUE );
            raiseClient( active_client );
        }
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
    bool has_full_screen = false;
    for ( ClientList::ConstIterator it = clients.begin(); it != clients.end(); ++it) {
        if ( (*it)->isTopMenu() && (*it)->mainClient() == main ) {
            menubar = *it;
        }
        if ( (*it)->isVisible() && (*it)->isFullScreen() &&
            !(*it)->isDesktop() && (*it)->staysOnTop() ) {
            has_full_screen = true;
        }
    }
    if ( !menubar && !has_full_screen)
    {
        // Find the menubar of the desktop
        if ( !desktops.isEmpty() ) {
            for ( ClientList::ConstIterator it = clients.begin(); it != clients.end(); ++it) {
                if ( (*it)->isTopMenu() && (*it)->mainClient()->isDesktop() ) {
                    menubar = *it;
                    break;
                }
            }
        }
#if 0 // I don't like this - why to show a menubar belonging to another application?
      // either show the app's menubar, or the desktop's one, otherwise none at all
        else
        {
            for ( ClientList::ConstIterator it = clients.begin(); it != clients.end(); ++it) {
                if ( (*it)->isTopMenu() && (*it)->mainClient() == (*it)
                    && (*it)->isOnDesktop( currentDesktop())) {
                    menubar = *it;
                    break;
                }
            }
        }
#endif
    }

    if ( menubar ) {
        menubar->show();
	// do not cover a client that wants to stay on top with the desktop menu
	if ( active_client == NULL
            || menubar->mainClient() == active_client->mainClient()
	         || !active_client->staysOnTop())
	    menubar->raise(); // better for FocusFollowsMouse than raiseClient(menubar)

        raiseElectricBorders();
    }

    // ... then hide the other ones. Avoids flickers.
    // Leave the desktop menubars always visible. Helps visually when the app doesn't show
    // its menubar immediately.
    for ( ClientList::ConstIterator it = clients.begin(); it != clients.end(); ++it) {
        if ( (*it)->isTopMenu() && (*it) != menubar && !(*it)->mainClient()->isDesktop() )
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
        // popupinfo->showInfo( desktopName(currentDesktop()) ); // AK - not sure
    }
    c->updateUserTime();

}

void Workspace::iconifyOrDeiconifyTransientsOf( Client* c )
{
    if ( c->isIconified() || c->isShade() ) {
        bool exclude_topmenu = !c->isIconified();
        for ( ClientList::ConstIterator it = clients.begin(); it != clients.end(); ++it) {
            if ( (*it)->transientFor() == c->window()
                 && !(*it)->isIconified()
                 && !(*it)->isShade()
                 && ( !exclude_topmenu || !(*it)->isTopMenu() ) ) {
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

    if( popup )
        popup->close();
    if( c == active_client )
        setActiveClient( NULL );
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
        if ( !c->isDesktop() && !desktops.isEmpty() )
            requestFocus( desktops.last() );
        else
            focusToNull();
    } else {
        // if blocking focus, move focus to the desktop later if needed
        // in order to avoid flickering
        focusToNull();
    }
}


// KDE4 - remove the unused argument
QPopupMenu* Workspace::clientPopup( Client* cl )
{
    if( cl != active_client ) {
        kdWarning( 1212 ) << "Using Workspace::clientPopup() with an argument is deprecated" << endl;
        activateClient( cl );
    }
    return clientPopup();
}

QPopupMenu* Workspace::clientPopup()
{
    if ( !popup ) {
        popup = new QPopupMenu;
        popup->setCheckable( TRUE );
        popup->setFont(KGlobalSettings::menuFont());
        connect( popup, SIGNAL( aboutToShow() ), this, SLOT( clientPopupAboutToShow() ) );
        connect( popup, SIGNAL( activated(int) ), this, SLOT( clientPopupActivated(int) ) );

        // PluginMenu *deco = new PluginMenu(mgr, popup);
        // deco->setFont(KGlobalSettings::menuFont());

        popup->insertItem( SmallIconSet( "move" ), i18n("&Move")+'\t'+keys->shortcut("Window Move").seq(0).toString(), Options::MoveOp );
        popup->insertItem( i18n("&Size")+'\t'+keys->shortcut("Window Resize").seq(0).toString(), Options::ResizeOp );
        popup->insertItem( i18n("Mi&nimize")+'\t'+keys->shortcut("Window Minimize").seq(0).toString(), Options::IconifyOp );
        popup->insertItem( i18n("Ma&ximize")+'\t'+keys->shortcut("Window Maximize").seq(0).toString(), Options::MaximizeOp );
        popup->insertItem( i18n("Sh&ade")+'\t'+keys->shortcut("Window Shade").seq(0).toString(), Options::ShadeOp );
        popup->insertItem( SmallIconSet( "attach" ), i18n("Always &on Top"), Options::StaysOnTopOp );
        popup->insertItem( SmallIconSet( "filesave" ), i18n("Sto&re Window Settings"), Options::ToggleStoreSettingsOp );

        popup->insertSeparator();

        popup->insertItem(SmallIconSet( "configure" ), i18n("Configur&e Window Behavior..."), this, SLOT( configureWM() ));

        popup->insertSeparator();

        popup->insertItem( SmallIconSet( "fileclose" ), i18n("&Close")+'\t'+keys->shortcut("Window Close").seq(0).toString(), Options::CloseOp );
    }
    return popup;
}

void Workspace::initDesktopPopup()
{
    if (desk_popup)
        return;

    desk_popup = new QPopupMenu( popup );
    desk_popup->setCheckable( TRUE );
    desk_popup->setFont(KGlobalSettings::menuFont());
    connect( desk_popup, SIGNAL( activated(int) ),
             this, SLOT( sendToDesktop(int) ) );
    connect( desk_popup, SIGNAL( aboutToShow() ),
             this, SLOT( desktopPopupAboutToShow() ) );

    popup->insertItem(i18n("To &Desktop"), desk_popup, -1, 8 );
}

// KDE4 remove me
void Workspace::showWindowMenuAt( unsigned long, int, int )
{
    slotWindowOperations();
}

void Workspace::performWindowOperation( Client* c, Options::WindowOperation op ) {
    if ( !c )
        return;

    if (op == Options::MoveOp)
        QCursor::setPos( c->geometry().center() );
    if (op == Options::ResizeOp)
        QCursor::setPos( c->geometry().bottomRight());
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
    performWindowOperation( popup_client ? popup_client : active_client, (Options::WindowOperation) id );
}

/*!
  Asks the internal positioning object to place a client
*/
void Workspace::place(Client* c)
{
    d->initPositioning->place(c);
}

void Workspace::doPlacement(Client* c )
{
    place( c );
}

/*!
  Marks the client as being moved around by the user.
 */
void Workspace::setClientIsMoving( Client *c )
{
    Q_ASSERT(!c || !d->movingClient); // Catch attempts to move a second
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
    ClientList::Iterator it(stacking_order.begin());
    bool re_init_cascade_at_first_client = true;
    for (; it != stacking_order.end(); ++it) {
        if((!(*it)->isOnDesktop(currentDesktop())) ||
           ((*it)->isIconified())                  ||
           ((*it)->isSticky())                     ||
           (!(*it)->isMovable()) )
            continue;
        d->initPositioning->placeCascaded(*it, re_init_cascade_at_first_client);
        //CT is an if faster than an attribution??
        if (re_init_cascade_at_first_client)
          re_init_cascade_at_first_client = false;
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
        d->initPositioning->placeSmart(*it);
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
    kdDebug(1212) << "Workspace::slotReconfigure()" << endl;
    reconfigureTimer.stop();
    KGlobal::config()->reparseConfiguration();
    options->reload();
    tab_box->reconfigure();
    popupinfo->reconfigure();
    readShortcuts();

    mgr->updatePlugin();

    if (options->electricBorders() == Options::ElectricAlways)
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
        if (r.search(title) != -1) {
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

    if ( c->isDesktop() )
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
        if ( t && !saveset.contains( t ) ) {
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
    
    if( c == active_client ) {
        // don't lower toplevel menubar
        for( ClientList::ConstIterator it = clients.fromLast();
             it != clients.end();
             --it )
            if( (*it)->isTopMenu() && (*it)->mainClient() == c ) {
                stacking_order.remove( *it );
                stacking_order.append( *it );
                break;
            }
    }
            
    propagateClients( true, true );

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

    if ( c->isDesktop() ) {
        saveset.clear();
        saveset.append( c );
        raiseTransientsOf(saveset, c );
        propagateClients( true, true );
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
        if ( t && !saveset.contains( t ) ) {
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

    propagateClients( true, true );

    if ( tab_box->isVisible() )
        tab_box->raise();

    if ( popupinfo->isVisible() )
        popupinfo->raise();

    raiseElectricBorders();
}

void Workspace::stackClientUnderActive( Client* c )
{
    if ( !active_client || !c || active_client == c )
        return;

    ClientList::Iterator it = stacking_order.find( active_client );
    if ( it == stacking_order.end() )
        return;
    stacking_order.remove( c );
    stacking_order.insert( it, c );
    stacking_order = constrainedStackingOrder( stacking_order );
    propagateClients( true, true );
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
  XSetInputFocus(qt_xdisplay(), null_focus_window, RevertToPointerRoot, qt_x_time );
}


/*!
  Refreshes all the client windows
 */
void Workspace::refresh() {
    QWidget w( 0, 0, WX11BypassWM );
    w.setGeometry( QApplication::desktop()->geometry() );
    w.show();
    w.hide();
    QApplication::flushX();
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

QValueList<Window>* ObscuringWindows::cached = 0;
unsigned int ObscuringWindows::max_cache_size = 0;

void ObscuringWindows::create( Client* c )
{
    if( cached == 0 )
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

    if( popup )
        popup->close();
    block_focus = TRUE;


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
            }
        }
        current_desktop = new_desktop;
        rootInfo->setCurrentDesktop( current_desktop ); // propagate befor the shows below

        for ( ClientList::ConstIterator it = stacking_order.fromLast(); it != stacking_order.end(); --it) {
            if ( (*it)->isOnDesktop( new_desktop ) && (*it)->isNormal() ) {
                (*it)->show();
            }
        }

        if (d->movingClient && !d->movingClient->isSticky())
        {
            d->movingClient->setDesktop(new_desktop);
        }
    }
    current_desktop = new_desktop;

    rootInfo->setCurrentDesktop( current_desktop );

    // restore the focus on this desktop
    block_focus = FALSE;
    Client* c = 0;

    if ( options->focusPolicyIsReasonable()) {
        // Search in focus chain

        if ( focus_chain.contains( active_client ) && active_client->isVisible() ) {
            c = active_client; // the requestFocus below will fail, as the client is already active
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

    //if "unreasonable focus policy"
    // and active_client is sticky and under mouse (hence == old_active_client),
    // conserve focus (thanks to Volker Schatz <V.Schatz at thphys.uni-heidelberg.de>)
    else if( active_client && active_client->isVisible() )
      c= active_client;

    if( c != active_client )
        setActiveClient( NULL );
        
    if ( c ) {
        requestFocus( c );
        // don't let the panel cover fullscreen windows on desktop switches
        if ( c->isFullScreen() && !c->isDesktop() && c->staysOnTop() )
            raiseClient( c );
    } else {
        focusToNull();
    }
    if( !desktops.isEmpty() ) {
        Window w_tmp;
        int i_tmp;
        XGetInputFocus( qt_xdisplay(), &w_tmp, &i_tmp );
        if( w_tmp == null_focus_window )
            requestFocus( desktops.last() );
    }

    // Update focus chain:
    //  If input: chain = { 1, 2, 3, 4 } and current_desktop = 3,
    //   Output: chain = { 3, 1, 2, 4 }.
//    kdDebug(1212) << QString("Switching to desktop #%1, at focus_chain index %2\n")
//      .arg(current_desktop).arg(desktop_focus_chain.find( current_desktop ));
    for( int i = desktop_focus_chain.find( current_desktop ); i > 0; i-- )
        desktop_focus_chain[i] = desktop_focus_chain[i-1];
    desktop_focus_chain[0] = current_desktop;

//    QString s = "desktop_focus_chain[] = { ";
//    for( uint i = 0; i < desktop_focus_chain.size(); i++ )
//        s += QString::number(desktop_focus_chain[i]) + ", ";
//    kdDebug(1212) << s << "}\n";
}

void Workspace::nextDesktop()
{
    int desktop = currentDesktop() + 1;
    setCurrentDesktop(desktop > numberOfDesktops() ? 1 : desktop);
    popupinfo->showInfo( desktopName(currentDesktop()) );
}

void Workspace::previousDesktop()
{
    int desktop = currentDesktop() - 1;
    setCurrentDesktop(desktop ? desktop : numberOfDesktops());
    popupinfo->showInfo( desktopName(currentDesktop()) );
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
    int old_number_of_desktops = number_of_desktops;
    number_of_desktops = n;

    if( currentDesktop() > numberOfDesktops())
        setCurrentDesktop( numberOfDesktops());

    // if increasing the number, do the resizing now,
    // otherwise after the moving of windows to still existing desktops
    if( old_number_of_desktops < number_of_desktops ) {
        rootInfo->setNumberOfDesktops( number_of_desktops );
        NETPoint* viewports = new NETPoint[ number_of_desktops ];
        rootInfo->setDesktopViewport( number_of_desktops, *viewports );
        delete[] viewports;
        updateClientArea( true );
    }

    // if the number of desktops decreased, move all
    // windows that would be hidden to the last visible desktop
    if( old_number_of_desktops > number_of_desktops ) {
        for( ClientList::ConstIterator it = clients.begin();
              it != clients.end();
              ++it) {
            if( !(*it)->isSticky() && (*it)->desktop() > numberOfDesktops())
                sendClientToDesktop( *it, numberOfDesktops());
        }
    }
    if( old_number_of_desktops > number_of_desktops ) {
        rootInfo->setNumberOfDesktops( number_of_desktops );
        NETPoint* viewports = new NETPoint[ number_of_desktops ];
        rootInfo->setDesktopViewport( number_of_desktops, *viewports );
        delete[] viewports;
        updateClientArea( true );
    }

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
    propagateClients( false, false );
}

/*!
  Propagates the managed clients to the world
 */
void Workspace::propagateClients( bool onlyStacking, bool alsoXRestack )
{
    Window *cl; // MW we should not assume WId and Window to be compatible
                                // when passig pointers around.

    if( alsoXRestack ) {
        Window* new_stack = new Window[ stacking_order.count() ];
        int i = 0;
        for ( ClientList::ConstIterator it = stacking_order.fromLast(); it != stacking_order.end(); --it)
            new_stack[i++] = (*it)->winId();
        XRestackWindows(qt_xdisplay(), new_stack, i);
        delete [] new_stack;
    }

    int i;
    if ( !onlyStacking ) {
        cl = new Window[ desktops.count() + clients.count()];
        i = 0;
        for ( ClientList::ConstIterator it = desktops.begin(); it != desktops.end(); ++it )
            cl[i++] =  (*it)->window();
        for ( ClientList::ConstIterator it = clients.begin(); it != clients.end(); ++it )
            cl[i++] =  (*it)->window();
        rootInfo->setClientList( cl, i );
        delete [] cl;
    }

    cl = new Window[ desktops.count() + stacking_order.count()];
    i = 0;
    for ( ClientList::ConstIterator it = desktops.begin(); it != desktops.end(); ++it )
        cl[i++] =  (*it)->window();
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
void Workspace::initShortcuts(){
    keys = new KGlobalAccel( this );
#include "kwinbindings.cpp"
    readShortcuts();
}

void Workspace::readShortcuts(){
    keys->readSettings();

    cutWalkThroughDesktops = keys->shortcut("Walk Through Desktops");
    cutWalkThroughDesktopsReverse = keys->shortcut("Walk Through Desktops (Reverse)");
    cutWalkThroughDesktopList = keys->shortcut("Walk Through Desktop List");
    cutWalkThroughDesktopListReverse = keys->shortcut("Walk Through Desktop List (Reverse)");
    cutWalkThroughWindows = keys->shortcut("Walk Through Windows");
    cutWalkThroughWindowsReverse = keys->shortcut("Walk Through Windows (Reverse)");

    keys->updateConnections();
}

void Workspace::slotSwitchDesktopNext(){
   int d = currentDesktop() + 1;
    if ( d > numberOfDesktops() ) {
      if ( options->rollOverDesktops ) {
        d = 1;
      }
      else {
        return;
      }
    }
    setCurrentDesktop(d);
    popupinfo->showInfo( desktopName(currentDesktop()) );
}

void Workspace::slotSwitchDesktopPrevious(){
    int d = currentDesktop() - 1;
    if ( d <= 0 ) {
      if ( options->rollOverDesktops )
        d = numberOfDesktops();
    else
        return;
    }
    setCurrentDesktop(d);
    popupinfo->showInfo( desktopName(currentDesktop()) );
}

void Workspace::setDesktopLayout(int o, int x, int y)
{
   d->layoutOrientation = (Qt::Orientation) o;
   d->layoutX = x;
   d->layoutY = y;
}

void Workspace::calcDesktopLayout(int &x, int &y)
{
    x = d->layoutX;
    y = d->layoutY;
    if ((x == -1) && (y > 0))
       x = (numberOfDesktops()+y-1) / y;
    else if ((y == -1) && (x > 0))
       y = (numberOfDesktops()+x-1) / x;

    if (x == -1)
       x = 1;
    if (y == -1)
       y = 1;
}

void Workspace::slotSwitchDesktopRight()
{
    int x,y;
    calcDesktopLayout(x,y);
    int dt = currentDesktop()-1;
    if (d->layoutOrientation == Qt::Vertical)
    {
      dt += y;
      if ( dt >= numberOfDesktops() ) {
        if ( options->rollOverDesktops )
          dt -= numberOfDesktops();
        else
          return;
      }
    }
    else
    {
      int d = (dt % x) + 1;
      if ( d >= x ) {
        if ( options->rollOverDesktops )
          d -= x;
        else
          return;
      }
      dt = dt - (dt % x) + d;
    }
    setCurrentDesktop(dt+1);
    popupinfo->showInfo( desktopName(currentDesktop()) );
}

void Workspace::slotSwitchDesktopLeft(){
    int x,y;
    calcDesktopLayout(x,y);
    int dt = currentDesktop()-1;
    if (d->layoutOrientation == Qt::Vertical)
    {
      dt -= y;
      if ( dt < 0 ) {
        if ( options->rollOverDesktops )
          dt += numberOfDesktops();
        else
          return;
      }
    }
    else
    {
      int d = (dt % x) - 1;
      if ( d < 0 ) {
        if ( options->rollOverDesktops )
          d += x;
        else
          return;
      }
      dt = dt - (dt % x) + d;
    }
    setCurrentDesktop(dt+1);
    popupinfo->showInfo( desktopName(currentDesktop()) );
}

void Workspace::slotSwitchDesktopUp(){
    int x,y;
    calcDesktopLayout(x,y);
    int dt = currentDesktop()-1;
    if (d->layoutOrientation == Qt::Horizontal)
    {
      dt -= x;
      if ( dt < 0 ) {
        if ( options->rollOverDesktops )
          dt += numberOfDesktops();
        else
          return;
      }
    }
    else
    {
      int d = (dt % y) - 1;
      if ( d < 0 ) {
        if ( options->rollOverDesktops )
          d += y;
        else
          return;
      }
      dt = dt - (dt % y) + d;
    }
    setCurrentDesktop(dt+1);
    popupinfo->showInfo( desktopName(currentDesktop()) );
}

void Workspace::slotSwitchDesktopDown(){
    int x,y;
    calcDesktopLayout(x,y);
    int dt = currentDesktop()-1;
    if (d->layoutOrientation == Qt::Horizontal)
    {
      dt += x;
      if ( dt >= numberOfDesktops() ) {
        if ( options->rollOverDesktops )
          dt -= numberOfDesktops();
        else
          return;
      }
    }
    else
    {
      int d = (dt % y) + 1;
      if ( d >= y ) {
        if ( options->rollOverDesktops )
          d -= y;
        else
          return;
      }
      dt = dt - (dt % y) + d;
    }
    setCurrentDesktop(dt+1);
    popupinfo->showInfo( desktopName(currentDesktop()) );
}

void Workspace::slotSwitchToDesktop( int i )
{
    setCurrentDesktop( i );
    popupinfo->showInfo( desktopName(currentDesktop()) );
}


void Workspace::slotWindowToDesktop( int i )
{
        if( i >= 1 && i <= numberOfDesktops() && active_client
            && !active_client->isDesktop()
            && !active_client->isDock()
            && !active_client->isTopMenu())
                sendClientToDesktop( active_client, i );
}

/*!
  Maximizes the popup client
 */
void Workspace::slotWindowMaximize()
{
    if ( active_client )
        active_client->maximize( Client::MaximizeFull );
}

/*!
  Maximizes the popup client vertically
 */
void Workspace::slotWindowMaximizeVertical()
{
    if ( active_client )
        active_client->maximize( Client::MaximizeVertical );
}

/*!
  Maximizes the popup client horiozontally
 */
void Workspace::slotWindowMaximizeHorizontal()
{
    if ( active_client )
        active_client->maximize( Client::MaximizeHorizontal );
}


/*!
  Iconifies the popup client
 */
void Workspace::slotWindowIconify()
{
    performWindowOperation( active_client, Options::IconifyOp );
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
    performWindowOperation( active_client, Options::ShadeOp );
}

/*!
  Raises the popup client
 */
void Workspace::slotWindowRaise()
{
    if ( active_client )
        raiseClient( active_client );
}

/*!
  Lowers the popup client
 */
void Workspace::slotWindowLower()
{
    if ( active_client )
        lowerClient( active_client );
}

/*!
  Does a toggle-raise-and-lower on the popup client;
  */
void Workspace::slotWindowRaiseOrLower()
{
    if  ( active_client )
        raiseOrLowerClient( active_client );
}

void Workspace::slotWindowSticky()
{
    if( active_client )
        active_client->toggleSticky();
}

void Workspace::slotWindowStaysOnTop()
{
    if( active_client )
        performWindowOperation( active_client, Options::StaysOnTopOp );
}

/*!
  Move window to next desktop
 */
void Workspace::slotWindowToNextDesktop(){
    int d = currentDesktop() + 1;
    if ( d > numberOfDesktops() )
        d = 1;
    if (active_client && !active_client->isDesktop()
        && !active_client->isDock() && !active_client->isTopMenu())
      sendClientToDesktop(active_client,d);
    setCurrentDesktop(d);
    popupinfo->showInfo( desktopName(currentDesktop()) );
}

/*!
  Move window to previous desktop
 */
void Workspace::slotWindowToPreviousDesktop(){
    int d = currentDesktop() - 1;
    if ( d <= 0 )
        d = numberOfDesktops();
    if (active_client && !active_client->isDesktop()
        && !active_client->isDock() && !active_client->isTopMenu())
      sendClientToDesktop(active_client,d);
    setCurrentDesktop(d);
    popupinfo->showInfo( desktopName(currentDesktop()) );
}

/*!
  Invokes keyboard mouse emulation
 */
void Workspace::slotMouseEmulation()
{

    if ( mouse_emulation ) {
        XUngrabKeyboard(qt_xdisplay(), qt_x_time);
        mouse_emulation = FALSE;
        return;
    }

    if ( XGrabKeyboard(qt_xdisplay(),
                       root, FALSE,
                       GrabModeAsync, GrabModeAsync,
                       qt_x_time) == GrabSuccess ) {
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
void Workspace::killWindowAtPosition(int, int)
{
    kdWarning() << "Obsolete Workspace::killWindowAtPosition() called" << endl;
}

void Workspace::killWindowId( Window window_to_kill )
{
    Window window = window_to_kill;
    Client* client = NULL;
    for(;;) {
        client = findClientWithId( window );
        if( client != NULL ) // found the client
            break;
        Window parent, root;
        Window* children;
        unsigned int children_count;
        XQueryTree( qt_xdisplay(), window, &root, &parent, &children, &children_count );
        if( children != NULL )
            XFree( children );
        if( window == root ) // we didn't find the client, probably an override-redirect window
            break;
        window = parent; // go up
    }
    if( client != NULL )
        client->killWindow();
    else
        XKillClient( qt_xdisplay(), window_to_kill );
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
    desk_popup->insertItem( i18n("&All Desktops"), 0 );
    if ( active_client && active_client->isSticky() )
        desk_popup->setItemChecked( 0, TRUE );
    desk_popup->insertSeparator( -1 );
    int id;
    const int BASE = 10;
    for ( int i = 1; i <= numberOfDesktops(); i++ ) {
        QString basic_name("%1  %2");
        if (i<BASE) {
            basic_name.prepend('&');
        }
        id = desk_popup->insertItem(
                basic_name
                    .arg(i)
                    .arg( desktopName(i).replace( QRegExp("&"), "&&" )),
                i
        );
        if ( active_client &&
             !active_client->isSticky() && active_client->desktop()  == i )
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

    if ( numberOfDesktops() == 1 )
    {
        delete desk_popup;
        desk_popup = 0;
    }
    else
    {
        initDesktopPopup();
    }

    popup->setItemEnabled( Options::ResizeOp, popup_client->isResizable() );
    popup->setItemEnabled( Options::MoveOp, popup_client->isMovable() );
    popup->setItemEnabled( Options::MaximizeOp, popup_client->isMaximizable() );
    popup->setItemChecked( Options::MaximizeOp, popup_client->isMaximized() );
    popup->setItemChecked( Options::ShadeOp, popup_client->isShade() );
    popup->setItemChecked( Options::StaysOnTopOp, popup_client->staysOnTop() );
    popup->setItemEnabled( Options::IconifyOp, popup_client->isMinimizable() );
    popup->setItemEnabled( Options::ToggleStoreSettingsOp, !popup_client->isTransient() );
    popup->setItemChecked( Options::ToggleStoreSettingsOp, popup_client->storeSettings() );
    popup->setItemEnabled( Options::CloseOp, popup_client->isCloseable() );
}


/*!
  Sends client \a c to desktop \a desk.

  Takes care of transients as well.
 */
void Workspace::sendClientToDesktop( Client* c, int desk )
{
    if ( c->desktop() == desk )
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
    updateClientArea();
}

/*!
  Sends the popup client to desktop \a desk

  Internal slot for the window operation menu
 */
void Workspace::sendToDesktop( int desk )
{
    if ( !popup_client )
        return;
    if ( desk == 0 ) { // the 'sticky' menu entry
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
    QPoint pos = active_client->mapToGlobal( active_client->windowWrapper()->geometry().topLeft() );
    showWindowMenu( pos.x(), pos.y(), active_client );
}

void Workspace::showWindowMenu( int x, int y, Client* cl )
{
    if( !cl )
        return;
    if ( cl->isDesktop()
        || cl->isDock()
        || cl->isTopMenu())
        return;
    if( cl != active_client )
        activateClient( cl );

    popup_client = cl;
    QPopupMenu* p = clientPopup();
    p->exec( QPoint( x, y ) );
    popup_client = 0;
}

/*!
  Closes the popup client
 */
void Workspace::slotWindowClose()
{
    if ( tab_box->isVisible() || popupinfo->isVisible() )
        return;
    performWindowOperation( active_client, Options::CloseOp );
}

/*!
  Starts keyboard move mode for the popup client
 */
void Workspace::slotWindowMove()
{
    performWindowOperation( active_client, Options::MoveOp );
}

/*!
  Starts keyboard resize mode for the popup client
 */
void Workspace::slotWindowResize()
{
    performWindowOperation( active_client, Options::ResizeOp );
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
      QRect maxRect = clientArea(MovementArea, pos+c->rect().center(), c->desktop());
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
            if ((*l)->isOnDesktop(currentDesktop()) &&
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
            c = findClientWithId( w );
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
            e.time = qt_x_time;
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
            e.time = qt_x_time;
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
bool Workspace::keyPressMouseEmulation( XKeyEvent& ev )
{
    if ( root != qt_xrootwin() )
        return FALSE;
    int kc = XKeycodeToKeysym(qt_xdisplay(), ev.keycode, 0);
    int km = ev.state & (ControlMask | Mod1Mask | ShiftMask);

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
        XUngrabKeyboard(qt_xdisplay(), qt_x_time);
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
    QWidget curtain( 0, 0, WX11BypassWM );
    curtain.setBackgroundMode( NoBackground );
    curtain.setGeometry( QApplication::desktop()->geometry() );
    curtain.show();

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
        Window stack[2];
        stack[0] = prev ? prev->winId() : curtain.winId();
        stack[1] = newClient->winId();
        XRestackWindows(  qt_xdisplay(), stack, 2 );
        if ( showIt )
            newClient->show();

        if( oldMaxMode != Client::MaximizeRestore ) {
            newClient->maximize(Client::MaximizeRestore); // needed
            newClient->maximize(oldMaxMode);
        }
        prev = newClient;
    }
    block_focus = FALSE;
    if ( active )
        requestFocus( active );
    else if( !desktops.isEmpty() )
        requestFocus( desktops.last() );
    else
        focusToNull();

    // Emit a DCOP signal to allow other apps to know when the kwin client
    // has been changed by via the titlebar decoration selection.
    emit dcopResetAllClients();
}

void Workspace::slotSettingsChanged(int category)
{
    kdDebug(1212) << "Workspace::slotSettingsChanged()" << endl;
    if( category == (int) KApplication::SETTINGS_SHORTCUTS )
        readShortcuts();
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
            ev.xclient.data.l[1] = qt_x_time;
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
                KProcess proc;
                if ( wmClientMachine != "localhost" )
                    proc << "xon" << wmClientMachine;
                proc << QString::fromLatin1( wmCommand );
                proc.start(KProcess::DontCare);
            }
        }
    }
}
#endif

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
//#ifndef NO_LEGACY_SESSION_MANAGEMENT // remember also legacy apps anyway
            // This is the only connection between the determination of legacy
            // session managed applications (storeLegacySession) and the
            // recollection of the window geometries (this function).
            if ( wmCommand.isEmpty() )
//#endif
                continue;
        count++;
        QString n = QString::number(count);
        config->writeEntry( QString("sessionId")+n, sessionId.data() );
        config->writeEntry( QString("windowRole")+n, c->windowRole().data() );
        config->writeEntry( QString("wmCommand")+n, wmCommand.data() );
        config->writeEntry( QString("wmClientMachine")+n, c->wmClientMachine().data() );
        config->writeEntry( QString("resourceName")+n, c->resourceName().data() );
        config->writeEntry( QString("resourceClass")+n, c->resourceClass().data() );
        config->writeEntry( QString("geometry")+n,  QRect( c->gravitate(TRUE), c->windowWrapper()->size() ) );
        config->writeEntry( QString("restore")+n, c->geometryRestore() );
        config->writeEntry( QString("maximize")+n, (int) c->maximizeMode() );
        config->writeEntry( QString("desktop")+n, c->desktop() );
        config->writeEntry( QString("iconified")+n, c->isIconified() );
        config->writeEntry( QString("sticky")+n, c->isSticky() );
        config->writeEntry( QString("shaded")+n, c->isShade() );
        config->writeEntry( QString("staysOnTop")+n, c->staysOnTop() );
        config->writeEntry( QString("skipTaskbar")+n, c->skipTaskbar() );
        config->writeEntry( QString("skipPager")+n, c->skipPager() );
        config->writeEntry( QString("windowType")+n, windowTypeToTxt( c->windowType()));
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
        info->windowType = txtToWindowType( config->readEntry( QString("windowType")+n ).latin1());
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
        info->windowRole = config->readEntry( QString("windowRole")+n ).latin1();
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
        info->windowType = txtToWindowType( config->readEntry( QString("windowType")+n ).latin1());
    }
}

void Workspace::storeFakeSessionInfo( Client* c )
{
    if ( !c->storeSettings() )
        return;
    SessionInfo* info = new SessionInfo;
    fakeSession.append( info );
    info->windowRole = c->windowRole();
    info->resourceName = c->resourceName();
    info->resourceClass = c->resourceClass();
    info->wmClientMachine = c->wmClientMachine();
    info->geometry = QRect( c->gravitate(TRUE), c->windowWrapper()->size() ) ;
    info->restore = c->geometryRestore();
    info->maximize = (int)c->maximizeMode();
    info->desktop = c->desktop();
    info->iconified = c->isIconified();
    info->sticky = c->isSticky();
    info->shaded = c->isShade();
    info->staysOnTop = c->staysOnTop();
    info->skipTaskbar = c->skipTaskbar();
    info->skipPager = c->skipPager();
    info->windowType = c->windowType();
}

void Workspace::writeFakeSessionInfo()
{
    KConfig *config = KGlobal::config();
    config->setGroup("FakeSession" );
    int count = 0;
    for ( SessionInfo* info = fakeSession.first(); info; info = fakeSession.next() ) {
        count++;
        QString n = QString::number(count);
        config->writeEntry( QString("windowRole")+n, info->windowRole.data() );
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
        config->writeEntry( QString("windowType")+n, windowTypeToTxt( info->windowType ));
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
            if ( info->sessionId == sessionId && sessionInfoWindowTypeMatch( c, info )) {
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
                 info->wmClientMachine == wmClientMachine &&
                 sessionInfoWindowTypeMatch( c, info ))
                if ( wmCommand.isEmpty() || info->wmCommand == wmCommand )
                    realInfo = session.take();
    }

    // Now search ``fakeSession''
    for (SessionInfo* info = fakeSession.first(); info && !fakeInfo; info = fakeSession.next() )
        if ( info->resourceName == resourceName &&
             info->resourceClass == resourceClass &&
             ( windowRole.isEmpty() || windowRole == info->windowRole ) &&
             sessionInfoWindowTypeMatch( c, info ))
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

bool Workspace::sessionInfoWindowTypeMatch( Client* c, SessionInfo* info )
{
    if( info->windowType == -2 ) { // undefined (not really part of NET::WindowType)
        return c->windowType() == NET::Unknown || c->windowType() == NET::Normal
            || c->windowType() == NET::Dialog || c->windowType() == NET::Override;
    }
    return info->windowType == c->windowType();
}

// maybe needed later
#if 0
// KMainWindow's without name() given have WM_WINDOW_ROLE in the form
// of <appname>-mainwindow#<number>
// when comparing them for fake session info, it's probably better to check
// them without the trailing number
bool Workspace::windowRoleMatch( const QCString& role1, const QCString& role2 )
{
    if( role1.isEmpty() && role2.isEmpty())
        return true;
    int pos1 = role1.find( '#' );
    int pos2 = role2.find( '#' );
    bool ret;
    if( pos1 < 0 || pos2 < 0 || pos1 != pos2 )
        ret = role1 == role2;
    else
        ret = qstrncmp( role1, role2, pos1 ) == 0;
    kdDebug() << "WR:" << role1 << ":" << pos1 << ":" << role2 << ":" << pos2 << ":::" << ret << endl;
    return ret;
}
#endif

static const char* const window_type_names[] = {
    "Unknown", "Normal" , "Desktop", "Dock", "Toolbar", "Menu", "Dialog",
    "Override", "TopMenu" };

const char* Workspace::windowTypeToTxt( NET::WindowType type )
{
    if( type >= NET::Unknown && type <= NET::TopMenu )
        return window_type_names[ type + 1 ]; // +1 (unknown==-1)
    if( type == -2 ) // undefined (not really part of NET::WindowType)
        return "Undefined";
    kdFatal() << "Unknown Window Type" << endl;
    return NULL;
}

NET::WindowType Workspace::txtToWindowType( const char* txt )
{
    for( int i = NET::Unknown;
         i <= NET::TopMenu;
         ++i )
        if( qstrcmp( txt, window_type_names[ i + 1 ] ) == 0 ) // +1
            return static_cast< NET::WindowType >( i );
    return static_cast< NET::WindowType >( -2 ); // undefined
}

/*!
  Updates the current client areas according to the current clients.

  If the area changes or force is true, the new areas are propagated to the world.

  The client area is the area that is available for clients (that
  which is not taken by windows like panels, the top-of-screen menu
  etc).

  \sa clientArea()
 */
 
void Workspace::updateClientArea( bool force )
{
    QRect* new_areas = new QRect[ numberOfDesktops() + 1 ];
    QRect all = QApplication::desktop()->geometry();
    for( int i = 1;
         i <= numberOfDesktops();
         ++i )
         new_areas[ i ] = all;
    for ( ClientList::ConstIterator it = clients.begin(); it != clients.end(); ++it) {
        QRect r = (*it)->adjustedClientArea( all );
        if( r == all )
            continue;
        if( (*it)->isSticky())
            for( int i = 1;
                 i <= numberOfDesktops();
                 ++i )
                new_areas[ i ] = new_areas[ i ].intersect( r );
        else
            new_areas[ (*it)->desktop() ] = new_areas[ (*it)->desktop() ].intersect( r );
    }

    bool changed = force;
    for( int i = 1;
         !changed && i <= numberOfDesktops();
         ++i )
        if( d->workarea[ i ] != new_areas[ i ] )
            changed = true;
    if ( changed ) {
        delete[] d->workarea;
        d->workarea = new_areas;
        new_areas = NULL;
        NETRect r;
        for( int i = 1; i <= numberOfDesktops(); i++)
        {
            r.pos.x = d->workarea[ i ].x();
            r.pos.y = d->workarea[ i ].y();
            r.size.width = d->workarea[ i ].width();
            r.size.height = d->workarea[ i ].height();
            rootInfo->setWorkArea( i, r );
        }

        for ( ClientList::ConstIterator it = clients.begin(); it != clients.end(); ++it) {
            if ( (*it)->isMaximized() )
                (*it)->maximize( Client::MaximizeAdjust );
        }
    }
    delete[] new_areas;
}

void Workspace::updateClientArea()
{
    updateClientArea( false );
}


/*!
  returns the area available for clients. This is the desktop
  geometry minus windows on the dock.  Placement algorithms should
  refer to this rather than geometry().

  \sa geometry()
 */
QRect Workspace::clientArea(clientAreaOption opt, const QPoint& p, int desktop )
{
    if( desktop == NETWinInfo::OnAllDesktops )
        desktop = currentDesktop();
    QRect rect = QApplication::desktop()->geometry();
    QDesktopWidget *desktopwidget = KApplication::desktop();

    switch (opt) {
        case MaximizeArea:
            if (options->xineramaMaximizeEnabled)
                rect = desktopwidget->screenGeometry(desktopwidget->screenNumber(p));
            break;
        case PlacementArea:
            if (options->xineramaPlacementEnabled)
                rect = desktopwidget->screenGeometry(desktopwidget->screenNumber(p));
            break;
        case MovementArea:
            if (options->xineramaMovementEnabled)
                rect = desktopwidget->screenGeometry(desktopwidget->screenNumber(p));
            break;
    }

    if (d->workarea[ desktop ].isNull())
        return rect;

    return d->workarea[ desktop ].intersect(rect);
}

QRect Workspace::clientArea(clientAreaOption opt, const QPoint& p)
{
    return clientArea( opt, p, currentDesktop());
}

QRect Workspace::clientArea(const QPoint& p, int desktop)
{
    if( desktop == NETWinInfo::OnAllDesktops )
        desktop = currentDesktop();
    int screenNum = QApplication::desktop()->screenNumber(p);
    QRect rect = QApplication::desktop()->screenGeometry(screenNum);

    if (d->workarea[ desktop ].isNull())
        return rect;

    return d->workarea[ desktop ].intersect(rect);
}

QRect Workspace::clientArea(const QPoint& p)
{
    return clientArea( p, currentDesktop());
}

void Workspace::loadDesktopSettings()
{
    KConfig c("kwinrc");

    QCString groupname;
    if (kwin_screen_number == 0)
        groupname = "Desktops";
    else
        groupname.sprintf("Desktops-screen-%d", kwin_screen_number);
    c.setGroup(groupname);

    int n = c.readNumEntry("Number", 4);
    number_of_desktops = n;
    delete d->workarea;
    d->workarea = new QRect[ n + 1 ];
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
    KConfig c("kwinrc");

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
        if ( ! XQueryPointer( qt_xdisplay(), root, &root_return, &child,
                              &root_x, &root_y, &lx, &ly, &state ) )
            return; // cursor is on another screen, so do not play with focus

        if ( !last_active_client )
            last_active_client = topClientOnDesktop();
        if ( last_active_client && last_active_client->isVisible() ) {
            qt_x_time = CurrentTime;
            requestFocus( last_active_client );
        }
    }
}

void Workspace::configureWM()
{
    QStringList args;
    args <<  "kwindecoration" << "kwinactions" << "kwinfocus" <<  "kwinmoving" << "kwinadvanced";
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
    d->electric_current_border = 0;

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
static int TimeDiff(unsigned long a, unsigned long b)
{
   if (a > b)
     return a-b;
   else
     return b-a;
}

void Workspace::clientMoved(const QPoint &pos, unsigned long now)
{
  if ((pos.x() != d->electricLeft) &&
      (pos.x() != d->electricRight) &&
      (pos.y() != d->electricTop) &&
      (pos.y() != d->electricBottom))
     return;

  if (options->electricBorders() == Options::ElectricDisabled)
     return;

  int treshold_set = options->electricBorderDelay(); // set timeout
  int treshold_reset = 250; // reset timeout
  int distance_reset = 10; // Mouse should not move more than this many pixels

  int border = 0;
  if (pos.x() == d->electricLeft)
     border = 1;
  else if (pos.x() == d->electricRight)
     border = 2;
  else if (pos.y() == d->electricTop)
     border = 3;
  else if (pos.y() == d->electricBottom)
     border = 4;

  if ((d->electric_current_border == border) &&
      (TimeDiff(d->electric_time_last, now) < treshold_reset) &&
      ((pos-d->electric_push_point).manhattanLength() < distance_reset))
  {
     d->electric_time_last = now;

     if (TimeDiff(d->electric_time_first, now) > treshold_set)
     {
        d->electric_current_border = 0;

        QRect r = QApplication::desktop()->geometry();
        int offset;

        int desk_before = currentDesktop();
        switch(border)
        {
          case 1:
           slotSwitchDesktopLeft();
           if (currentDesktop() != desk_before) {
               offset = r.width() / 5;
               QCursor::setPos(r.width() - offset, pos.y());
           }
           break;

          case 2:
           slotSwitchDesktopRight();
           if (currentDesktop() != desk_before) {
               offset = r.width() / 5;
               QCursor::setPos(offset, pos.y());
           }
           break;

          case 3:
           slotSwitchDesktopUp();
           if (currentDesktop() != desk_before) {
             offset = r.height() / 5;
             QCursor::setPos(pos.x(), r.height() - offset);
           }
           break;

          case 4:
           slotSwitchDesktopDown();
           if (currentDesktop() != desk_before) {
             offset = r.height() / 5;
             QCursor::setPos(pos.x(), offset);
           }
           break;
        }
        return;
     }
  }
  else {
    d->electric_current_border = border;
    d->electric_time_first = now;
    d->electric_time_last = now;
    d->electric_push_point = pos;
  }

  int mouse_warp = 1;

  // reset the pointer to find out wether the user is really pushing
  switch( border)
  {
    case 1: QCursor::setPos(pos.x()+mouse_warp, pos.y()); break;
    case 2: QCursor::setPos(pos.x()-mouse_warp, pos.y()); break;
    case 3: QCursor::setPos(pos.x(), pos.y()+mouse_warp); break;
    case 4: QCursor::setPos(pos.x(), pos.y()-mouse_warp); break;
  }
}

// this function is called when the user entered an electric border
// with the mouse. It may switch to another virtual desktop
void Workspace::electricBorder(XEvent *e)
{
  Time now = e->xcrossing.time;
  QPoint p(e->xcrossing.x_root, e->xcrossing.y_root);

  clientMoved(p, now);
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
