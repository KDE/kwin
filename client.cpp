/*****************************************************************
kwin - the KDE window manager

Copyright (C) 1999, 2000 Matthias Ettrich <ettrich@kde.org>
******************************************************************/
//#define QT_CLEAN_NAMESPACE
#include <klocale.h>
#include <kapplication.h>
#include <kdebug.h>
#include <kkeynative.h>
#include <kshortcut.h>
#include <dcopclient.h>
#include <qapplication.h>
#include <qcursor.h>
#include <qbitmap.h>
#include <qimage.h>
#include <qwmatrix.h>
#include <qlayout.h>
#include <qpainter.h>
#include <qwhatsthis.h>
#include <qobjectlist.h>
#include <qdatetime.h>
#include <qtimer.h>
#include <kwin.h>
#include <kiconloader.h>
#include "workspace.h"
#include "client.h"
#include "events.h"
#include "atoms.h"
#include <netwm.h>
#include <stdlib.h>
#include <X11/X.h>
#include <X11/Xos.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <X11/extensions/shape.h>

// Needed for --enable-final
// XIconincState is defined in workspace.cpp
#ifndef IconicState
#define IconicState XIconicState
#endif

namespace KWinInternal {

// NET WM Protocol handler class
class WinInfo : public NETWinInfo
{
public:
    WinInfo(KWinInternal::Client * c, Display * display, Window window,
            Window rwin, unsigned long pr )
        : NETWinInfo( display, window, rwin, pr, NET::WindowManager ) {
        m_client = c;
    }

    virtual void changeDesktop(int desktop) {
        m_client->workspace()->sendClientToDesktop( m_client, desktop );
    }
    virtual void changeState( unsigned long state, unsigned long mask ) {
        // state : kwin.h says: possible values are or'ed combinations of NET::Modal,
        // NET::Sticky, NET::MaxVert, NET::MaxHoriz, NET::Shaded, NET::SkipTaskbar, NET::SkipPager

        mask &= ~NET::Sticky; // KWin doesn't support large desktops, ignore
        mask &= ~NET::Hidden; // clients are not allowed to change this directly
        state &= mask; // for safety, clear all other bits

        if ( (mask & NET::Max) == NET::Max ) {
            m_client->maximizeRaw( state & NET::MaxVert, state & NET::MaxHoriz );
        } else if ( mask & NET::MaxVert ) {
            m_client->maximizeRaw( state & NET::MaxVert, m_client->maximizeMode() & KWinInternal::Client::MaximizeHorizontal );
        } else if ( mask & NET::MaxHoriz ) {
            m_client->maximizeRaw( m_client->maximizeMode() & KWinInternal::Client::MaximizeVertical, state & NET::MaxHoriz );
        }
        if ( mask & NET::Shaded )
            m_client->setShade( state & NET::Shaded );
        if ( mask & NET::StaysOnTop) {
            m_client->setStaysOnTop( (state & NET::StaysOnTop) != 0 );
            if ( m_client->staysOnTop() )
                m_client->workspace()->raiseClient( m_client );
        }
        if( mask & NET::SkipTaskbar )
            m_client->setSkipTaskbar( ( state & NET::SkipTaskbar ) != 0 );
        if( mask & NET::SkipPager )
            m_client->setSkipPager( ( state & NET::SkipPager ) != 0 );
    }
private:
    KWinInternal::Client * m_client;
};

class WindowWrapperPrivate
{
public:
    WindowWrapperPrivate() {};
};

class ClientPrivate
{
public:
    ClientPrivate() {};
    QCString windowRole;
};

};

// put all externs before the namespace statement to allow the linker
// to resolve them properly

extern Atom qt_wm_state;
extern Time qt_x_time;
extern Atom qt_window_role;
extern Atom qt_sm_client_id;

static int nullErrorHandler(Display *, XErrorEvent *)
{
    return 0;
}

using namespace KWinInternal;

static bool blockAnimation = FALSE;

static QRect* visible_bound = 0;

static QCString getStringProperty(WId w, Atom prop, char separator=0);

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
  ev.xclient.data.l[1] = qt_x_time;
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

const long ClientWinMask = KeyPressMask | KeyReleaseMask |
                                  ButtonPressMask | ButtonReleaseMask |
                  KeymapStateMask |
                  ButtonMotionMask |
                  PointerMotionMask | // need this, too!
                  EnterWindowMask | LeaveWindowMask |
                  FocusChangeMask |
                  ExposureMask |
                  StructureNotifyMask |
                  SubstructureRedirectMask;


WindowWrapper::WindowWrapper( WId w, Client *parent, const char* name)
    : QWidget( parent, name )
{
    d = new WindowWrapperPrivate;
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
    XSelectInput( qt_xdisplay(), winId(), ClientWinMask | SubstructureNotifyMask );

    XSelectInput( qt_xdisplay(), w,
                  FocusChangeMask |
                  PropertyChangeMask |
                  ColormapChangeMask |
                  EnterWindowMask | LeaveWindowMask
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
    delete d;
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
        if ( ((Client*)parentWidget())->isResize() ) {
            QTimer::singleShot( 0, this, SLOT( deferredResize() ) );
        } else {
            XMoveResizeWindow( qt_xdisplay(), win,
                               0, 0, width(), height() );
            if ( ((Client*)parentWidget())->shape() )
                ((Client*)parentWidget())->updateShape();
        }
    }
}

void WindowWrapper::deferredResize()
{
    XMoveResizeWindow( qt_xdisplay(), win,
                       0, 0, width(), height() );
    ((Client*)parentWidget())->sendSyntheticConfigureNotify();
    if ( ((Client*)parentWidget())->shape() )
        ((Client*)parentWidget())->updateShape();
    QApplication::syncX(); // process our own configure events synchronously.
}


/*!
  Reimplemented to do map() as well
 */
void WindowWrapper::show()
{
    map();
    QWidget::show();
}



/*!
  Reimplemented to do unmap() as well
 */
void WindowWrapper::hide()
{
    QWidget::hide();
    unmap();
}

/*!
  Maps the managed window.
 */
void WindowWrapper::map()
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
        XSelectInput( qt_xdisplay(), winId(), ClientWinMask );
        XMapRaised( qt_xdisplay(), win );
        XSelectInput( qt_xdisplay(), winId(), ClientWinMask  | SubstructureNotifyMask );
    }
}

/*!
  Unmaps the managed window.
 */
void WindowWrapper::unmap()
{
    if ( win ) {
        XSelectInput( qt_xdisplay(), winId(), ClientWinMask );
        XUnmapWindow( qt_xdisplay(), win );
        XSelectInput( qt_xdisplay(), winId(), ClientWinMask  | SubstructureNotifyMask );
    }
}


/*!
  Invalidates the managed window. After that, window() returns 0.
*/
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
        XDeleteProperty( qt_xdisplay(),  win, atoms->kde_net_user_time);
        XRemoveFromSaveSet( qt_xdisplay(), win );
        XSelectInput( qt_xdisplay(), win, NoEventMask );
        invalidateWindow();
    }
}




bool WindowWrapper::x11Event( XEvent * e)
{
    switch ( e->type ) {
    case ButtonPress: {
        ((Client*)parentWidget())->updateUserTime();
        uint keyModX = (options->keyCmdAllModKey() == Qt::Key_Meta) ?
            KKeyNative::modX(KKey::WIN) :
            KKeyNative::modX(KKey::ALT);
        bool bModKeyHeld = ( e->xbutton.state & KKeyNative::accelModMaskX()) == keyModX;

        if ( ((Client*)parentWidget())->isActive()
             && ( options->focusPolicy != Options::ClickToFocus
                  &&  options->clickRaise && !bModKeyHeld ) ) {
            if ( e->xbutton.button < 4 ) // exclude wheel
                ((Client*)parentWidget())->autoRaise();
            ungrabButton( winId(), None );
        }

        Options::MouseCommand com = Options::MouseNothing;
        if ( bModKeyHeld ){
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

        if ( ((Client*)parentWidget())->windowType() != NET::Normal &&
             ((Client*)parentWidget())->windowType() != NET::Dialog &&
             ((Client*)parentWidget())->windowType() != NET::Menu &&
             ((Client*)parentWidget())->windowType() != NET::Override )
            replay = TRUE;

        XAllowEvents(qt_xdisplay(), replay? ReplayPointer : SyncPointer, CurrentTime ); //qt_x_time);
        return TRUE;
    } break;
    case ButtonRelease:
        XAllowEvents(qt_xdisplay(), SyncPointer, CurrentTime ); //qt_x_time);
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
    : QWidget( parent, name, f | WX11BypassWM )
{
    d = new ClientPrivate;
    wspace = ws;
    win = w;
    autoRaiseTimer = 0;
    shadeHoverTimer = 0;

    unsigned long properties =
        NET::WMDesktop |
        NET::WMState |
        NET::WMWindowType |
        NET::WMStrut |
        NET::WMName |
        NET::WMIconGeometry
        ;

    info = new WinInfo( this, qt_xdisplay(), win, qt_xrootwin(), properties );

    wwrap = new WindowWrapper( w, this );
    wwrap->installEventFilter( this );

    // set the initial mapping state
    setMappingState( WithdrawnState );
    desk = 0; // and no desktop yet
    is_sticky_ = FALSE;

    mode = Nowhere;
    buttonDown = FALSE;
    moveResizeMode = FALSE;
    setMouseTracking( TRUE );


    shaded = FALSE;
    hover_unshade = FALSE;
    active = FALSE;
    stays_on_top = FALSE;
    is_shape = FALSE;
    may_move = TRUE;
    may_resize = TRUE;
    may_minimize = TRUE;
    may_maximize = TRUE;
    may_close = TRUE;
    is_fullscreen = FALSE;
    skip_taskbar = FALSE;

    Pdeletewindow = 0;
    Ptakefocus = 0;
    Pcontexthelp = 0;
    input = FALSE;
    store_settings = FALSE;
    skip_pager = FALSE;

    transient_for = None;
    transient_for_defined = FALSE;

    max_mode = MaximizeRestore;

    cmap = None;

    // TODO shouldn't all this go to manage() ?
    
    bool mresize, mmove, mminimize, mmaximize, mclose;
    if( Motif::funcFlags( win, mresize, mmove, mminimize, mmaximize, mclose )) {
        may_resize = mresize;
        may_move = mmove;
        may_minimize = mminimize;
        may_maximize = mmaximize;
        may_close = mclose;
    }

    Window ww;
    if ( !XGetTransientForHint( qt_xdisplay(), (Window) win, &ww ) )
        transient_for = None;
    else {
        transient_for = (WId) ww;
        transient_for_defined = TRUE;
        verifyTransientFor();
    }

    XClassHint classHint;
    if ( XGetClassHint( qt_xdisplay(), win, &classHint ) ) {
        resource_name = classHint.res_name;
        resource_class = classHint.res_class;
        XFree( classHint.res_name );
        XFree( classHint.res_class );
    }

    getWMHints();
    getWindowProtocols();
    getWmNormalHints(); // get xSizeHint
    getWmClientLeader();
    fetchName();
    d->windowRole = getStringProperty( w, qt_window_role );

    // window wants to stay on top?
    stays_on_top = ( info->state() & NET::StaysOnTop) != 0 || ( transient_for == None && transient_for_defined );

    // window does not want a taskbar entry?
    skip_taskbar = ( info->state() & NET::SkipTaskbar) != 0;

    skip_pager = ( info->state() & NET::SkipPager) != 0;
}

/*!
  Destructor, nothing special.
 */
Client::~Client()
{
    if (moveResizeMode)
       stopMoveResize();
    releaseWindow();
    if( workspace()->activeClient() == this ) // this really shouldn't happen,
        workspace()->setActiveClient( NULL ); // but just in case
    delete info;
    delete d;
}

void Client::startMoveResize()
{
    moveResizeMode = true;
    workspace()->setClientIsMoving(this);
    grabMouse( cursor() );
    grabKeyboard();
    if ( ( isMove() && options->moveMode != Options::Opaque )
      || ( isResize() && options->resizeMode != Options::Opaque ) )
        XGrabServer( qt_xdisplay() );
}

void Client::stopMoveResize()
{
    if ( ( isMove() && options->moveMode != Options::Opaque )
      || ( isResize() && options->resizeMode != Options::Opaque ) )
        XUngrabServer( qt_xdisplay() );
    releaseKeyboard();
    releaseMouse();
    workspace()->setClientIsMoving(0);
    moveResizeMode = false;
}

/*!
  Manages the clients. This means handling the very first maprequest:
  reparenting, initial geometry, initial state, placement, etc.
 */
bool Client::manage( bool isMapped, bool doNotShow, bool isInitial )
{

    if (layout())
      layout()->setResizeMode( QLayout::Minimum );

    XWindowAttributes attr;
    if (XGetWindowAttributes(qt_xdisplay(), win, &attr)) {
        cmap = attr.colormap;
        original_geometry.setRect(attr.x, attr.y, attr.width, attr.height );
    }

    geom = original_geometry;
    bool placementDone = FALSE;

    SessionInfo* session = workspace()->takeSessionInfo( this );
    if ( session )
        geom = session->geometry;

    QRect area = workspace()->clientArea( geom.center() );

    if ( geom == workspace()->geometry() && inherits( "KWinInternal::NoBorderClient" ) ) {
        if ( !stays_on_top )
            is_fullscreen = TRUE;
        may_move = FALSE; // don't let fullscreen windows be moved around
        may_resize = FALSE;
        may_minimize = FALSE;
        may_maximize = FALSE;
        may_close = FALSE;
    }

    if ( isDesktop() ) {
        // desktops are treated slightly special
        geom = workspace()->geometry();
        may_move = FALSE;
        may_resize = FALSE;
        may_minimize = FALSE;
        may_maximize = FALSE;
        may_close = FALSE;
        isMapped = TRUE;
    }

    if ( isMapped  || session || isTransient() ) {
        placementDone = TRUE;
    }  else {

        bool ignorePPosition = false;
        XClassHint classHint;
        if ( XGetClassHint(qt_xdisplay(), win, &classHint) != 0 ) {
            if ( classHint.res_class )
                ignorePPosition = ( options->ignorePositionClasses.find(QString::fromLatin1(classHint.res_class)) != options->ignorePositionClasses.end() );
            XFree(classHint.res_name);
            XFree(classHint.res_class);
        }

        if ((xSizeHint.flags & PPosition) && ! ignorePPosition) {
            int tx = geom.x();
            int ty = geom.y();

            if (tx < 0)
                tx = area.right() + tx;
            if (ty < 0)
                ty = area.bottom() + ty;
            geom.moveTopLeft(QPoint(tx, ty));
        }

        if ( ( (xSizeHint.flags & PPosition) && !ignorePPosition ) ||
             (xSizeHint.flags & USPosition) ) {
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

    if ( placementDone && ( windowType() == NET::Normal || windowType() == NET::Dialog || windowType() == NET::Unknown
            || windowType() == NET::Menu )
        && !area.contains( geom.topLeft() ) && may_move ) {
        int tx = geom.x();
        int ty = geom.y();
        if ( tx >= 0 && tx < area.x() )
                tx = area.x();
        if ( ty >= 0 && ty < area.y() )
            ty = area.y();
        if ( tx > area.right() || ty > area.bottom() )
            placementDone = FALSE; // weird, do not trust.
        else
            geom.moveTopLeft( QPoint( tx, ty ) );
    }

    windowWrapper()->resize( geom.size() );
    // the clever activate() trick is necessary
    activateLayout();
    resize ( sizeForWindowSize( geom.size() ) );
    activateLayout();

    // inform clients about the frame geometry
    NETStrut strut;
    QRect wr = windowWrapper()->geometry();
    QRect mr = rect();
    strut.left = wr.left();
    strut.right = mr.right() - wr.right();
    strut.top = wr.top();
    strut.bottom = mr.bottom() - wr.bottom();
    info->setKDEFrameStrut( strut );

    move( geom.x(), geom.y() );
    move(gravitate( FALSE ) ); // take the decoration size into account

    if ( !placementDone ) {
        workspace()->place( this );
        placementDone = TRUE;
    } else if ( ( windowType() == NET::Normal || windowType() == NET::Dialog || windowType() == NET::Unknown
            || windowType() == NET::Menu ) && may_move ) {
        if ( geometry().right() > area.right() && width() < area.width() )
            move( area.right() - width(), y() );
        if ( geometry().bottom() > area.bottom() && height() < area.height() )
            move( x(), area.bottom() - height() );
    }

    XShapeSelectInput( qt_xdisplay(), win, ShapeNotifyMask );
    if ( (is_shape = Shape::hasShape( win )) ) {
        updateShape();
    }

    // initial state
    int init_state = NormalState;
    if ( isInitial)
    {
        if ( session ) {
            if ( session->iconified )
                init_state = IconicState;
        } else {
            // find out the initial state. Several possibilities exist
            XWMHints * hints = XGetWMHints(qt_xdisplay(), win );
            if (hints && (hints->flags & StateHint)) {
                init_state = hints->initial_state;
                if ( init_state == WithdrawnState )
                    info->setState( NET::SkipTaskbar, NET::SkipTaskbar );
                //CT extra check for stupid jdk 1.3.1. But should make sense in general
                // if client has initial state set to Iconic and is transient with a parent
                // window that is not Iconic, set init_state to Normal
                if ((init_state == IconicState) && isTransient() && transientFor() != 0) {
		  Client* client = workspace()->findClient(transientFor());
                  if(client == 0 || !client->isIconified()) {
                    init_state = NormalState;
                  }
                }
            }
            if (hints)
                XFree(hints);
        }
    }

    // initial desktop placement
    if ( info->desktop() )
        desk = info->desktop(); // window had the initial desktop property!
    if ( mainClient()->isSticky() )
        desk = NET::OnAllDesktops;
    
    if ( session ) {
        desk = session->desktop;
        if( session->sticky )
            desk = NET::OnAllDesktops;
    } else if ( desk == 0 ) {
        // if this window is transient, ensure that it is opened on the
        // same window as its parent.  this is necessary when an application
        // starts up on a different desktop than is currently displayed
        //
        if ( isTransient() && !mainClient()->isSticky() )
            desk = mainClient()->desktop();

        if ( desk != 0 && !isMapped && !doNotShow && desk != workspace()->currentDesktop()
                    && !isTopMenu() ) {
            //window didn't specify any specific desktop but will appear
            //somewhere else. This happens for example with "save data?"
            //dialogs on shutdown. Switch to the respective desktop in
            //that case.
            // TODO check what is this supposed to do, looks like unnecessary
            // focus stealing to me
            workspace()->setCurrentDesktop( desk );
        }
    }
    if ( desk == 0 ) // assume window wants to be visible on the current desktop
        desk = workspace()->currentDesktop();
    info->setDesktop( desk );
    is_sticky_ = ( desk == NET::OnAllDesktops );
    workspace()->setStickyTransientsOf( this, isSticky());
    stickyChange( isSticky());

    if (isInitial) {
        setMappingState( init_state );
    }

    if ( workspace()->isNotManaged( caption() ) )
        doNotShow = TRUE;

    // other settings from the previous session
    if ( session ) {
        setStaysOnTop( session->staysOnTop );
        setSkipTaskbar( session->skipTaskbar );
        setSkipPager( session->skipPager );
        maximize( (MaximizeMode) session->maximize );
        setShade( session->shaded );
        geom_restore = session->restore;
    } else if ( !is_fullscreen ){
        // window may want to be maximized
        if ( (info->state() & NET::Max) == NET::Max )
            maximize( Client::MaximizeFull );
        else if ( info->state() & NET::MaxVert )
            maximize( Client::MaximizeVertical );
        else if ( info->state() & NET::MaxHoriz )
            maximize( Client::MaximizeHorizontal );

        if ( isMaximizable() && !isMaximized()
             && ( width() >= area.width() || height() >= area.height() ) ) {
            // window is too large for the screen, maximize in the
            // directions necessary and generate a suitable restore
            // geometry.
            QSize s = adjustedSize( area.size());
            QPoint orig_pos( x(), y());
            if ( width() >= area.width() && height() >= area.height() ) {
                maximize( Client::MaximizeFull );
                geom_restore.setSize( s );
                geom_restore.moveTopLeft( orig_pos );
            } else if ( width() >= area.width() ) {
                maximize( Client::MaximizeHorizontal );
                geom_restore.setWidth( s.width() );
                geom_restore.moveTopLeft( orig_pos );
            } else if ( height() >= area.height() ) {
                maximize( Client::MaximizeVertical );
                geom_restore.setHeight( s.height() );
                geom_restore.moveTopLeft( orig_pos );
            }
        }
    }

    bool showMe = (state == NormalState) && isOnDesktop( workspace()->currentDesktop() );

    workspace()->clientReady( this ); // will call Workspace::propagateClients()

    if ( showMe && !doNotShow ) {
        if( isDialog())
            Events::raise( Events::TransNew );
        if( isNormalWindow())
            Events::raise( Events::New );
        if ( isMapped ) {
            show();
        } else {
            // we only raise (and potentially activate) new clients if
            // the user does not actively work in the currently active
            // client. We can safely drop the activation when the new
            // window is not a dialog of the active client and
            // NET_KDE_USER_TIME of the currently active client is
            // defined and more recent than the one of the new client
            // (which we set ourselves in CreateNotify in
            // workspace.cpp).  Of course we only do that magic if the
            // window does not stem from a restored session.
            Client* ac = workspace()->activeClient();

            unsigned long usertime = 0;
            if ( !isTransient() && !session && ac && !ac->isDesktop() &&
                   !resourceMatch( ac, this ) &&
                   ( usertime = userTime() ) > 0 && ac->userTime() > usertime ) {
                workspace()->stackClientUnderActive( this );
                show();
            } else {
                workspace()->raiseClient( this );
                show();
                if ( options->focusPolicyIsReasonable() && wantsTabFocus() )
                    workspace()->requestFocus( this );
            }
        }
    }

    if ( !doNotShow )
      workspace()->updateClientArea();

    sendSyntheticConfigureNotify();

    delete session;
    return showMe;
}



/*!
  Updates the user time on the client window. This is called inside
  kwin for every action with the window that qualifies for user
  interaction (clicking on it, activate it externally, etc.).
 */
void Client::updateUserTime()
{
    if ( window() ) {
        timeval tv;
        gettimeofday( &tv, NULL );
        unsigned long now = tv.tv_sec * 10 + tv.tv_usec / 100000;
        XChangeProperty(qt_xdisplay(), window(),
                        atoms->kde_net_user_time, XA_CARDINAL,
                        32, PropModeReplace, (unsigned char *)&now, 1);
    }
}

unsigned long Client::userTime()
{
    unsigned long result = 0;
    Atom type;
    int format, status;
    unsigned long nitems = 0;
    unsigned long extra = 0;
    unsigned char *data = 0;
    XErrorHandler oldHandler = XSetErrorHandler(nullErrorHandler);
    status = XGetWindowProperty( qt_xdisplay(), window(),
                                 atoms->kde_net_user_time,
                                 0, 10000, FALSE, XA_CARDINAL, &type, &format,
                                 &nitems, &extra, &data );
    XSetErrorHandler(oldHandler);
    if (status  == Success ) {
        if (data && nitems > 0)
            result = *((long*) data);
        XFree(data);
    }
    return result;
}

bool Client::resourceMatch( Client* c1, Client* c2 )
{
    if( qstrncmp( c1->resourceClass(), "XV", 2 ) == 0 && c1->resourceName() == "xv" ) // xv :(
        return qstrncmp( c2->resourceClass(), "XV", 2 ) == 0 && c2->resourceName() == "xv";
    return c1->resourceClass() == c2->resourceClass();
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
    QString s;

    if ( info->name()  ) {
        s = QString::fromUtf8( info->name() );
    } else {
        XTextProperty tp;
        char **text;
        int count;
        if ( XGetTextProperty( qt_xdisplay(), win, &tp, XA_WM_NAME) != 0 && tp.value != NULL ) {
            if ( tp.encoding == XA_STRING )
                s = QString::fromLocal8Bit( (const char*) tp.value );
            else if ( XmbTextPropertyToTextList( qt_xdisplay(), &tp, &text, &count) == Success &&
                      text != NULL && count > 0 ) {
                s = QString::fromLocal8Bit( text[0] );
                XFreeStringList( text );
            }
            XFree( tp.value );
        }
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

    if ( ( dirty & NET::WMName ) != 0 )
        fetchName();
    if ( ( dirty & NET::WMStrut ) != 0 )
        workspace()->updateClientArea();
    if ( ( dirty & NET::WMIcon) != 0 )
        getWMHints();

    switch (e->type) {
    case UnmapNotify:
        return unmapNotify( e->xunmap );
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
        if ( QApplication::activePopupWidget() )
            break;
        setActive( FALSE );
        break;
    case ReparentNotify:
        break;
    case ClientMessage:
        return clientMessage( e->xclient );
    case ColormapChangeMask:
        cmap = e->xcolormap.colormap;
        if ( isActive() )
            workspace()->updateColormap();
    default:
        if ( e->type == Shape::shapeEvent() )
            updateShape();
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
        if ( !windowWrapper()->isVisibleTo( 0 ) && !e.send_event )
            return TRUE; // this event was produced by us as well

        // maybe we will be destroyed soon. Check this first.
        XEvent ev;
        if ( XCheckTypedWindowEvent (qt_xdisplay(), windowWrapper()->winId(),
                                     DestroyNotify, &ev) ){
            if( isDialog())
                Events::raise( Events::TransDelete );
            if( isNormalWindow())
                Events::raise( Events::Delete );
            workspace()->destroyClient( this );
            return TRUE;
        }
        if ( XCheckTypedWindowEvent (qt_xdisplay(), windowWrapper()->winId(),
                                     ReparentNotify, &ev) ){
          if ( ev.xreparent.window == windowWrapper()->window() &&
               ev.xreparent.parent != windowWrapper()->winId() )
            invalidateWindow();
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
    if( isDialog())
        Events::raise( Events::TransDelete );
    if( isNormalWindow())
        Events::raise( Events::Delete );
    // remove early from client list
    workspace()->removeClient( this );
    info->setDesktop( 0 );
    desk = 0;
    is_sticky_ = false;
    releaseWindow(TRUE);
    workspace()->destroyClient( this );
}

/*!
  Handles configure  requests of the client window
 */
bool Client::configureRequest( XConfigureRequestEvent& e )
{
    if ( isResize() )
        return TRUE; // we have better things to do right now

    if ( isDesktop() ) {
        setGeometry( workspace()->geometry() );
        sendSyntheticConfigureNotify();
        return TRUE;
    }

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
        int ox = 0;
        int oy = 0;
        int gravity = NorthWestGravity;
        if ( xSizeHint.flags & PWinGravity)
            gravity = xSizeHint.win_gravity;
        if ( gravity == StaticGravity ) { // only with StaticGravity according to ICCCM 4.1.5
            ox = windowWrapper()->x();
            oy = windowWrapper()->y();
        }

        int nx = x() + ox;
        int ny = y() + oy;

        if ( e.value_mask & CWX )
            nx = e.x;
        if ( e.value_mask & CWY )
            ny = e.y;


        // clever workaround for applications like xv that want to set
        // the location to the current location but miscalculate the
        // frame size due to kwin being a double-reparenting window
        // manager
        if ( ox == 0 && oy == 0 &&
             nx == x() + windowWrapper()->x() &&
             ny == y() + windowWrapper()->y() ) {
            nx = x();
            ny = y();
        }


        QPoint np( nx-ox, ny-oy);
#if 0
        if ( windowType() == NET::Normal && may_move ) {
            // crap for broken netscape
            QRect area = workspace()->clientArea();
            if ( !area.contains( np ) && width() < area.width()  &&
                 height() < area.height() ) {
                if ( np.x() < area.x() )
                    np.rx() = area.x();
                if ( np.y() < area.y() )
                    np.ry() = area.y();
            }
        }
#endif

        if ( isMaximized() ) {
          geom_restore.moveTopLeft( np );
        } else {
            move( np );
        }
    }

    if ( e.value_mask & (CWWidth | CWHeight ) ) {
        int nw = windowWrapper()->width();
        int nh = windowWrapper()->height();
        if ( e.value_mask & CWWidth )
            nw = e.width;
        if ( e.value_mask & CWHeight )
            nh = e.height;
        QSize ns = sizeForWindowSize( QSize( nw, nh ) );

        //QRect area = workspace()->clientArea();
        if ( isMaximizable() && isMaximized() ) {  //&& ( ns.width() < area.width() || ns.height() < area.height() ) ) {
            if( ns != size()) { // don't restore if some app sets its own size again
                if ( (e.value_mask & (CWX | CWY )) == 0 )
                    geom_restore.moveTopLeft( geometry().topLeft() );
                geom_restore.setSize( ns );
                maximize( Client::MaximizeRestore );
            }
        } else if ( !isMaximized() ) {
            if ( ns == size() )
                return TRUE; // broken xemacs stuff (ediff)
            resize( ns );
        }
    }


    if ( stacking ){
        switch (stack_mode){
        case Above:
        case TopIf:
            if ( isTopMenu() && mainClient() != this )
                break; // in this case, we already do the raise
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

    if ( e.value_mask & (CWX | CWY  | CWWidth | CWHeight ) )
        sendSyntheticConfigureNotify();
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
        if ( !XGetTransientForHint( qt_xdisplay(), (Window) win, &ww ) ) {
            transient_for = None;
            transient_for_defined = FALSE;
        } else {
            transient_for = (WId) ww;
            transient_for_defined = TRUE;
            verifyTransientFor();
        }
        break;
    case XA_WM_HINTS:
        getWMHints();
        break;
    default:
        if ( e.atom == atoms->wm_protocols )
            getWindowProtocols();
        else if (e.atom == atoms->wm_client_leader )
            getWmClientLeader();
        else if( e.atom == qt_window_role )
            d->windowRole = getStringProperty( win, qt_window_role );
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
        } else if ( e.data.l[0] == NormalState && isIconified() ) {
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
void Client::sendSyntheticConfigureNotify()
{
    XConfigureEvent c;
    c.type = ConfigureNotify;
    c.send_event = True;
    c.event = win;
    c.window = win;
    c.x = x() + windowWrapper()->x();
    c.y = y() + windowWrapper()->y();
    c.width = windowWrapper()->width();
    c.height = windowWrapper()->height();
    c.border_width = 0;
    c.above = None;
    c.override_redirect = 0;
    XSendEvent( qt_xdisplay(), c.event, TRUE, StructureNotifyMask, (XEvent*)&c );
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
    } else if ( xSizeHint.flags & PMinSize ) {
        bw = xSizeHint.min_width;
        bh = xSizeHint.min_height;
        if (w < xSizeHint.min_width)
            w = xSizeHint.min_width;
        if (h < xSizeHint.min_height)
            h = xSizeHint.min_height;
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
    int wh = 1;
    if ( !wwrap->isHidden() )
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
    if ( !isMovable() || !may_resize )
        return FALSE;

    if ( ( xSizeHint.flags & PMaxSize) == 0 || (xSizeHint.flags & PMinSize ) == 0 )
        return TRUE;
    return ( xSizeHint.min_width != xSizeHint.max_width  ) ||
          ( xSizeHint.min_height != xSizeHint.max_height  );
}

/*
  Returns whether the window is maximizable or not
 */
bool Client::isMaximizable() const
{
    if ( isMaximized() )
        return TRUE;
    return isResizable() && !isTool() && may_maximize;
}

/*
  Returns whether the window is minimizable or not
 */
bool Client::isMinimizable() const
{
    return ( !isTransient() || !workspace()->findClient( transientFor() ) )
        && wantsTabFocus() && may_minimize;
}

/*
  Returns whether the window may be closed (have a close button)
 */
bool Client::isCloseable() const
{
    return may_close && !isDesktop() && !isDock() && !isTopMenu()
	&&  windowType() != NET::Override;
}




/*!
  Reimplemented to provide move/resize
 */
void Client::mousePressEvent( QMouseEvent * e)
{
    if (buttonDown)
        return;

    Options::MouseCommand com = Options::MouseNothing;

    if (e->state() & AltButton) {
        if ( e->button() == LeftButton ) {
            com = options->commandAll1();
        } else if (e->button() == MidButton) {
            com = options->commandAll2();
        } else if (e->button() == RightButton) {
            com = options->commandAll3();
        }
    }
    else {
        bool active = isActive();
        if ( !wantsInput() ) // we cannot be active, use it anyway
            active = TRUE;

        if ((e->button() == LeftButton  && options->commandActiveTitlebar1() != Options::MouseOperationsMenu) ||
            (e->button() == MidButton   && options->commandActiveTitlebar2() != Options::MouseOperationsMenu) ||
            (e->button() == RightButton && options->commandActiveTitlebar3() != Options::MouseOperationsMenu) ) {
          mouseMoveEvent( e );
          buttonDown = TRUE;
          moveOffset = e->pos();
          invertedMoveOffset = rect().bottomRight() - e->pos();
        }

        if ( e->button() == LeftButton ) {
            com = active ? options->commandActiveTitlebar1() : options->commandInactiveTitlebar1();
        }
        else if ( e->button() == MidButton ) {
            com = active ? options->commandActiveTitlebar2() : options->commandInactiveTitlebar2();
        }
        else if ( e->button() == RightButton ) {
            com = active ? options->commandActiveTitlebar3() : options->commandInactiveTitlebar3();
        }
    }
    performMouseCommand( com, e->globalPos());
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
            stopMoveResize();
            setGeometry( geom );
            mode = mousePosition( e->pos() );
            setMouseCursor( mode );
            Events::raise( isResize() ? Events::ResizeEnd : Events::MoveEnd );
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
        MousePosition newmode = mousePosition( e->pos() );
        if( newmode != mode )
            setMouseCursor( newmode );
        mode = newmode;
        geom = geometry();
        return;
    }

    if ( !isMovable() || (isShade() && mode != Center)) return;

    if ( !moveResizeMode ) {
        QPoint p( e->pos() - moveOffset );
        if (p.manhattanLength() >= 6) {
            if ( isMaximized() ) {
                // in case we were maximized, reset state
                max_mode = MaximizeRestore;
                maximizeChange(FALSE );
                Events::raise( Events::UnMaximize );
                info->setState( 0, NET::Max );
            }
            startMoveResize();
            Events::raise( isResize() ? Events::ResizeStart : Events::MoveStart );
        } else {
            return;
        }
    }

    if ( mode !=  Center && hover_unshade )
        setShade(false);

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

    QRect desktopArea = workspace()->clientArea(e->globalPos());
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

    if ( isMove() )
      workspace()->clientMoved(globalPos, qt_x_time);

//     QApplication::syncX(); // process our own configure events synchronously.
}

// these two aren't called at all ... ?!

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
}



/*!
  Reimplemented to inform the client about the new window position.
 */
void Client::setGeometry( int x, int y, int w, int h )
{
    QWidget::setGeometry(x, y, w, h);
    if ( !isResize() && isVisible() )
        sendSyntheticConfigureNotify();
}

/*!
  Reimplemented to inform the client about the new window position.
 */
void Client::move( int x, int y )
{
    QWidget::move( x, y );
    if ( !isResize() && isVisible() )
        sendSyntheticConfigureNotify();
}


/*!
  Reimplemented to set the mapping state and to map the managed
  window in the window wrapper. Also takes care of deiconification of
  transients.
 */
void Client::show()
{
    if ( isIconified() && ( !isTransient() || mainClient() == this ) ) {
        animateIconifyOrDeiconify( FALSE );
        //CT and unshade it
        if (isShade())
            setShade(false);
    }

    setMappingState( NormalState );
    QWidget::show();
    windowWrapper()->map();
}

/*!
  Reimplemented to unmap the managed window in the window
wrapper. Also informs the workspace.
*/
void Client::hide()
{
    QWidget::hide();
    workspace()->clientHidden( this );
    windowWrapper()->unmap();
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

/*!\fn shadeChange( bool shaded )

  Indicates that the window was shaded or unshaded. \a shaded is
  set respectively. Subclasses may want to indicate the new state
  graphically, for example with a different icon.
 */
void Client::shadeChange( bool )
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

  If withdraw is TRUE, the function also sets the mapping state of the
  window to WithdrawnState
 */
void Client::releaseWindow( bool withdraw )
{
    if ( win ) {
        move(gravitate(TRUE));
        if ( withdraw )
            XUnmapWindow( qt_xdisplay(), win );
        windowWrapper()->releaseWindow();
        if ( withdraw )
            setMappingState( WithdrawnState );
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
    if ( !isMinimizable() )
        return;

    setMappingState( IconicState );
    Events::raise( Events::Iconify );

    if ( (!isTransient() || mainClient() == this ) && isVisible() )
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
    if( !isCloseable())
        return;
    Events::raise( Events::Close );
    if ( Pdeletewindow ){
        sendClientMessage( win, atoms->wm_protocols, atoms->wm_delete_window);
    }
    else {
        // client will not react on wm_delete_window. We have not choice
        // but destroy his connection to the XServer.
        if( isDialog())
            Events::raise( Events::TransDelete );
        if( isNormalWindow())
            Events::raise( Events::Delete );
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



/*!
  Sets the maximization according to \a vertically and \a horizontally
 */
void Client::maximizeRaw( bool vertically, bool horizontally )
{
    if ( !vertically && !horizontally ) {
        maximize ( MaximizeRestore );
    } else {
        MaximizeMode m = MaximizeRestore;
        if ( vertically && horizontally )
            m = MaximizeFull;
        else if ( vertically )
            m = MaximizeVertical;
        else if (horizontally )
            m = MaximizeHorizontal;
        if ( m != max_mode ) {
            if ( isMaximized() )
                max_mode = MaximizeAdjust;
            maximize( m );
        }
    }
}

/*!
  Maximizes the client according to mode \a m. If the client is
  already maximized with the same mode, it gets restored. Does some
  smart magic like vertically + horizontally = full.

  This is the slot to connect to from your client subclass.
 */
void Client::maximize( MaximizeMode m)
{
    if ( !isMaximizable() )
        return;

    QRect clientArea = workspace()->clientArea(geometry().center());

    if (isShade())
        setShade( FALSE );

    if ( m == MaximizeAdjust ) {
        m = max_mode;
    } else {

        if ( max_mode == m )
            m = MaximizeRestore;
        if ( m == max_mode )
            return; // nothing to do

        if ( m != MaximizeRestore && max_mode != MaximizeAdjust ) {
            if ( max_mode == MaximizeRestore )
                geom_restore = geometry();
            else if ( m != MaximizeFull)
                m = (MaximizeMode ) ( (max_mode & MaximizeFull) ^ (m & MaximizeFull) );
            Events::raise( Events::Maximize );
        }
    }


    switch (m) {

    case MaximizeVertical:
        setGeometry(
                    QRect(QPoint(geom_restore.x(), clientArea.top()),
                          adjustedSize(QSize(geom_restore.width(), clientArea.height())))
                    );
        info->setState( NET::MaxVert, NET::Max );
        break;

    case MaximizeHorizontal:

        setGeometry(
                    QRect(
                          QPoint(clientArea.left(), geom_restore.y()),
                          adjustedSize(QSize(clientArea.width(), geom_restore.height())))
                    );
        info->setState( NET::MaxHoriz, NET::Max );
        break;

    case MaximizeRestore: {
        Events::raise( Events::UnMaximize );
        setGeometry(geom_restore);
        max_mode = MaximizeRestore;
        info->setState( 0, NET::Max );
        } break;

    case MaximizeFull: {
        QSize adjSize = adjustedSize(clientArea.size());
        QRect r = QRect(clientArea.topLeft(), adjSize);

        // hide right and left border of maximized windows
        if ( !options->moveResizeMaximizedWindows && adjSize == clientArea.size()) {
            if ( r.left() == 0 )
                r.setLeft( r.left() - windowWrapper()->x() );
            if ( r.right() == workspace()->geometry().right() )
                r.setRight( r.right() + width() -  windowWrapper()->geometry().right() );
        }
        setGeometry( r );
        info->setState( NET::Max, NET::Max );
    } break;
    default:
        break;
    }

    max_mode = m;

    maximizeChange( m != MaximizeRestore );
}


void Client::toggleSticky()
{
    setSticky( !isSticky() );
}

void Client::toggleShade()
{
    setShade( !isShade() );
}

void Client::maximize()
{
    if ( isMaximized() )
        maximize( MaximizeRestore );
    else
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

const QPoint Client::gravitate( bool invert ) const
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
        dx = windowWrapper()->x();
        dy = 0;
        break;
    case NorthEastGravity:
        dx =  width() - windowWrapper()->width();
        dy = 0;
        break;
    case WestGravity:
        dx = 0;
        dy = windowWrapper()->y();
        break;
    case CenterGravity:
    case StaticGravity:
        dx = windowWrapper()->x();
        dy = windowWrapper()->y();
        break;
    case EastGravity:
        dx = width() - windowWrapper()->width();
        dy = windowWrapper()->y();
        break;
    case SouthWestGravity:
        dx = 0;
        dy = height() - windowWrapper()->height();
        break;
    case SouthGravity:
        dx = windowWrapper()->x();
        dy = height() - windowWrapper()->height();
        break;
    case SouthEastGravity:
        dx = width() - windowWrapper()->width();
        dy = height() - windowWrapper()->height();
        break;
    }
    if (invert)
        return QPoint( x() + dx, y() + dy );
    else
        return QPoint( x() - dx, y() - dy );
}

/*!
  Reimplement to handle crossing events (qt should provide xroot, yroot)

  Crossing events are necessary for the focus-follows-mouse focus
  policies, to do proper activation and deactivation.
*/
bool Client::x11Event( XEvent * e)
{
    if ( e->type == EnterNotify &&
         ( e->xcrossing.mode == NotifyNormal ||
           ( !options->focusPolicyIsReasonable() &&
             e->xcrossing.mode == NotifyUngrab ) ) ) {

        if (options->shadeHover && isShade() && !isDesktop()) {
            delete shadeHoverTimer;
            shadeHoverTimer = new QTimer( this );
            connect( shadeHoverTimer, SIGNAL( timeout() ), this, SLOT( shadeHover() ));
            shadeHoverTimer->start( options->shadeHoverInterval, TRUE );
        }

        if ( options->focusPolicy == Options::ClickToFocus )
            return TRUE;

        if ( options->autoRaise && !isDesktop() &&
             !isDock() && !isTopMenu() && workspace()->focusChangeEnabled() &&
             workspace()->topClientOnDesktop() != this ) {
            delete autoRaiseTimer;
            autoRaiseTimer = new QTimer( this );
            connect( autoRaiseTimer, SIGNAL( timeout() ), this, SLOT( autoRaise() ) );
            autoRaiseTimer->start( options->autoRaiseInterval, TRUE  );
        }

        if ( options->focusPolicy !=  Options::FocusStrictlyUnderMouse && ( isDesktop() || isDock() || isTopMenu() ) )
            return TRUE;

        workspace()->requestFocus( this );
        return TRUE;
    }

    if ( e->type == LeaveNotify && e->xcrossing.mode == NotifyNormal ) {
        if ( !buttonDown ) {
            mode = Nowhere;
            setCursor( arrowCursor );
        }
        bool lostMouse = !rect().contains( QPoint( e->xcrossing.x, e->xcrossing.y ) );
        // 'lostMouse' wouldn't work with e.g. B2 or Keramik, which have non-rectangular decorations
        // (i.e. the LeaveNotify event comes before leaving the rect and no LeaveNotify event
        // comes after leaving the rect) - so lets check if the pointer is really outside the window

        // TODO this still sucks if a window appears above this one - it should lose the mouse
        // if this window is another client, but not if it's a popup ... maybe after KDE3.1 :(
        // (repeat after me 'AARGHL!')
        if ( !lostMouse && e->xcrossing.detail != NotifyInferior ) {
            int d1, d2, d3, d4;
            unsigned int d5;
            Window w, child;
            if( XQueryPointer( qt_xdisplay(), winId(), &w, &child, &d1, &d2, &d3, &d4, &d5 ) == False
                || child == None )
                lostMouse = true; // really lost the mouse
        }
        if ( lostMouse ) {
            delete autoRaiseTimer;
            autoRaiseTimer = 0;
            delete shadeHoverTimer;
            shadeHoverTimer = 0;
            if ( hover_unshade && !moveResizeMode && !buttonDown )
               setShade( TRUE, 1 );
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
    if ( !isResizable() || isShade() ) {
        setCursor( arrowCursor );
        return;
    }

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

void Client::setShade( bool s, int hus )
{
    /* This case if when we think we are:
       1. Getting shaded
       2. Were already unshaded because of hover unshading
       3. and this is not a hover shade operation.

       This can happen when a window is hover unshaded and the user
       double clicks on the title bar and wants the window to be unshaded
    */
    if ( s && hover_unshade && !hus) {
        hover_unshade = 0;
        return;
    }

    hover_unshade = hus;

    if ( shaded == s )
    return;

    shaded = s;


    if ( isVisible() )
        Events::raise( s ? Events::ShadeUp : Events::ShadeDown );

    int as = options->animateShade? 10 : 1;

    if ( shaded ) {
        int h = height();
        QSize s( sizeForWindowSize( QSize( windowWrapper()->width(), 0), TRUE ) );
        windowWrapper()->hide();
        repaint( FALSE );
        bool wasStaticContents = testWFlags( WStaticContents );
        setWFlags( WStaticContents );
        int step = QMAX( 4, QABS( h - s.height() ) / as )+1;
        do {
            h -= step;
            resize ( s.width(), h );
            QApplication::syncX();
        } while ( h > s.height() + step );
        if ( !wasStaticContents )
            clearWFlags( WStaticContents );
        resize (s );
    } else {
        int h = height();
        QSize s( sizeForWindowSize( windowWrapper()->size(), TRUE ) );
        bool wasStaticContents = testWFlags( WStaticContents );
        setWFlags( WStaticContents );
        int step = QMAX( 4, QABS( h - s.height() ) / as )+1;
        do {
            h += step;
            resize ( s.width(), h );
            // assume a border
            // we do not have time to wait for X to send us paint events
            repaint( 0, h - step-5, width(), step+5, TRUE);
            QApplication::syncX();
        } while ( h < s.height() - step );
        if ( !wasStaticContents )
            clearWFlags( WStaticContents );
        resize ( s );
        if (hus)
            setActive( TRUE );
        windowWrapper()->show();
        activateLayout();
        if ( isActive() )
            workspace()->requestFocus( this );
    }

    if(!hus)
        info->setState( shaded?NET::Shaded:0, NET::Shaded );

    workspace()->iconifyOrDeiconifyTransientsOf( this );
    shadeChange( shaded );
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
    if ( active )
        Events::raise( Events::Activate );

    if ( !active && autoRaiseTimer ) {
        delete autoRaiseTimer;
        autoRaiseTimer = 0;
    }

    activeChange( active );
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


void Client::setSkipTaskbar( bool b )
{
    if ( b == skipTaskbar() )
        return;
    skip_taskbar = b;
    info->setState( b?NET::SkipTaskbar:0, NET::SkipTaskbar );
}

void Client::setSkipPager( bool b )
{
    if ( b == skipPager() )
        return;
    skip_pager = b;
    info->setState( b?NET::SkipPager:0, NET::SkipPager );
}


void Client::setDesktop( int desktop )
{
    if( desk == desktop )
        return;
    int was_desk = desk;
    desk = desktop;
    is_sticky_ = ( desk == NET::OnAllDesktops );
    info->setDesktop( desktop );
    if(( was_desk == NET::OnAllDesktops ) != ( desktop == NET::OnAllDesktops )) {
        // sticky changed
        if ( isVisible())
            Events::raise( isSticky() ? Events::Sticky : Events::UnSticky );
        workspace()->setStickyTransientsOf( this, isSticky());
        stickyChange( isSticky());
    }
}

void Client::setSticky( bool b )
{
    if(( b && isSticky())
        || ( !b && !isSticky()))
        return;
    if( b )
        setDesktop( NET::OnAllDesktops );
    else
        setDesktop( workspace()->currentDesktop());
}

void Client::getWMHints()
{
    // get the icons, allow scaling
    icon_pix = KWin::icon( win, 32, 32, TRUE );
    miniicon_pix = KWin::icon( win, 16, 16, TRUE );

    if ( icon_pix.isNull() && mainClient() != this ) {
        icon_pix = mainClient()->icon_pix;
        miniicon_pix = mainClient()->miniicon_pix;
    }

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
void Client::takeFocus( bool force )
{
    if ( !force && ( isTopMenu() || isDock() ) ) {
	if ( isDock() && !staysOnTop() && workspace()->activeClient() ) {
	    // the active client might get covered by a dock
	    // window. Re-enable ourselves passive grabs to make
	    // click-raise work again
	    workspace()->activeClient()->windowWrapper()->setActive( FALSE );
	}
        return; // toplevel menus and dock windows don't take focus if not forced
    }

    if ( input ) {
        // Matthias Ettrich says to comment it so that we avoid two consecutive setActive
        //   We will have to look after it anyways, in case people will get problems
        //setActive( TRUE );

        // Qt may delay the mapping which may cause XSetInputFocus to fail, force show window
        QApplication::sendPostedEvents( windowWrapper(), QEvent::ShowWindowRequest );
        XSetInputFocus( qt_xdisplay(), win, RevertToPointerRoot, qt_x_time );
    }
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
    if  ( !isTransient() && transientFor() != 0 )
        return this;
    ClientList saveset;
    Client *n, *c = this;
    do {
        saveset.append( c );
        n = workspace()->findClient( c->transientFor() );
        if ( !n )
            break;
        c = n;
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
    case Options::MouseShade :
        setShade(!isShade());
        break;
    case Options::MouseOperationsMenu:
        if ( isActive() & ( options->focusPolicy != Options::ClickToFocus &&  options->clickRaise ) )
            autoRaise();
        workspace()->showWindowMenu( globalPos, this );
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
        workspace()->lowerClient( this );
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
        replay = TRUE;
        break;
    case Options::MouseMove:
        if (!isMovable())
            break;
        mode = Center;
        geom=geometry();
        if ( isMaximized() ) {
            // in case we were maximized, reset state
            max_mode = MaximizeRestore;
            maximizeChange(FALSE );
            Events::raise( Events::UnMaximize );
            info->setState( 0, NET::Max );
        }
        buttonDown = TRUE;
        moveOffset = mapFromGlobal( globalPos );
        invertedMoveOffset = rect().bottomRight() - moveOffset;
        setMouseCursor( mode );
        startMoveResize();
        break;
    case Options::MouseResize: {
        if (!isMovable())
            break;
        geom=geometry();
        if ( isMaximized() ) {
            // in case we were maximized, reset state
            max_mode = MaximizeRestore;
            maximizeChange(FALSE );
            Events::raise( Events::UnMaximize );
            info->setState( 0, NET::Max );
        }
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
        startMoveResize();
        } break;
    case Options::MouseNothing:
        // fall through
    default:
        replay = TRUE;
        break;
    }
    return replay;
}

// performs _NET_WM_MOVERESIZE
void Client::NETMoveResize( int x_root, int y_root, NET::Direction direction )
{
    if( direction == NET::Move )
        performMouseCommand( Options::MouseMove, QPoint( x_root, y_root ));
    else if( direction >= NET::TopLeft && direction <= NET::Left ) {
        static const MousePosition convert[] = { TopLeft, Top, TopRight, Right, BottomRight, Bottom,
            BottomLeft, Left };
        if (!isMovable())
            return;
        geom=geometry();
        if ( isMaximized() ) {
            // in case we were maximized, reset state
            max_mode = MaximizeRestore;
            maximizeChange(FALSE );
            Events::raise( Events::UnMaximize );
            info->setState( 0, NET::Max );
        }
        buttonDown = TRUE;
        moveOffset = mapFromGlobal( QPoint( x_root, y_root ));
        invertedMoveOffset = rect().bottomRight() - moveOffset;
        mode = convert[ direction ];
        setMouseCursor( mode );
        startMoveResize();
    }
}

void Client::keyPressEvent( uint key_code )
{
    if ( !isMove() && !isResize() )
        return;
    bool is_control = key_code & Qt::CTRL;
    key_code = key_code & 0xffff;
    int delta = is_control?1:8;
    QPoint pos = QCursor::pos();
    switch ( key_code ) {
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
    case Key_Escape:
        clearbound();
        stopMoveResize();
        setGeometry( geom );
        buttonDown = FALSE;
        break;
    default:
        return;
    }
    QCursor::setPos( pos );
}

static QCString getStringProperty(WId w, Atom prop, char separator)
{
    Atom type;
    int format, status;
    unsigned long nitems = 0;
    unsigned long extra = 0;
    unsigned char *data = 0;
    QCString result = "";
    XErrorHandler oldHandler = XSetErrorHandler(nullErrorHandler);
    status = XGetWindowProperty( qt_xdisplay(), w, prop, 0, 10000,
                                 FALSE, XA_STRING, &type, &format,
                                 &nitems, &extra, &data );
    XSetErrorHandler(oldHandler);
    if ( status == Success) {
        if (data && separator) {
            for (int i=0; i<(int)nitems; i++)
                if (!data[i] && i+1<(int)nitems)
                    data[i] = separator;
        }
        if (data)
            result = (const char*) data;
        XFree(data);
    }
    return result;
}

/*!
  Returns WM_WINDOW_ROLE property for a given window.
 */
QCString Client::staticWindowRole(WId w)
{
    return getStringProperty(w, qt_window_role);
}

/*!
  Returns SM_CLIENT_ID property for a given window.
 */
QCString Client::staticSessionId(WId w)
{
    return getStringProperty(w, qt_sm_client_id);
}

/*!
  Returns WM_COMMAND property for a given window.
 */
QCString Client::staticWmCommand(WId w)
{
    return getStringProperty(w, XA_WM_COMMAND, ' ');
}

/*!
  Returns WM_CLIENT_MACHINE property for a given window.
  Local machine is always returned as "localhost".
 */
QCString Client::staticWmClientMachine(WId w)
{
    QCString result = getStringProperty(w, XA_WM_CLIENT_MACHINE);
    if (result.isEmpty()) {
        result = "localhost";
    } else {
        // special name for the local machine (localhost)
        char hostnamebuf[80];
        if (gethostname (hostnamebuf, sizeof hostnamebuf) >= 0) {
            hostnamebuf[sizeof(hostnamebuf)-1] = 0;
            if (result == hostnamebuf)
                result = "localhost";
            char *dot = strchr(hostnamebuf, '.');
            if (dot && !(*dot = 0) && result == hostnamebuf)
                result = "localhost";
        }
    }
    return result;
}

/*!
  Returns WM_CLIENT_LEADER property for a given window.
 */
Window Client::staticWmClientLeader(WId w)
{
    Atom type;
    int format, status;
    unsigned long nitems = 0;
    unsigned long extra = 0;
    unsigned char *data = 0;
    Window result = w;
    XErrorHandler oldHandler = XSetErrorHandler(nullErrorHandler);
    status = XGetWindowProperty( qt_xdisplay(), w, atoms->wm_client_leader, 0, 10000,
                                 FALSE, XA_WINDOW, &type, &format,
                                 &nitems, &extra, &data );
    XSetErrorHandler(oldHandler);
    if (status  == Success ) {
        if (data && nitems > 0)
            result = *((Window*) data);
        XFree(data);
    }
    return result;
}


void Client::getWmClientLeader()
{
    wmClientLeaderWin = staticWmClientLeader(win);
}

/*!
  Returns WM_WINDOW_ROLE for this client
 */
QCString Client::windowRole()
{
    return d->windowRole;
}

/*!
  Returns sessionId for this client,
  taken either from its window or from the leader window.
 */
QCString Client::sessionId()
{
    QCString result = staticSessionId(win);
    if (result.isEmpty() && wmClientLeaderWin && wmClientLeaderWin!=win)
        result = staticSessionId(wmClientLeaderWin);
    return result;
}

/*!
  Returns the classhint resource name for this client,
 */
QCString Client::resourceName()
{
    return resource_name;
}

/*!
  Returns the classhint resource class for this client,
 */
QCString Client::resourceClass()
{
    return resource_class;
}

/*!
  Returns command property for this client,
  taken either from its window or from the leader window.
 */
QCString Client::wmCommand()
{
    QCString result = staticWmCommand(win);
    if (result.isEmpty() && wmClientLeaderWin && wmClientLeaderWin!=win)
        result = staticWmCommand(wmClientLeaderWin);
    return result;
}

/*!
  Returns client machine for this client,
  taken either from its window or from the leader window.
*/
QCString Client::wmClientMachine()
{
    QCString result = staticWmClientMachine(win);
    if (result.isEmpty() && wmClientLeaderWin && wmClientLeaderWin!=win)
        result = staticWmClientMachine(wmClientLeaderWin);
    return result;
}

/*!
  Returns client leader window for this client.
  Returns the client window itself if no leader window is defined.
*/
Window Client::wmClientLeader()
{
    if (wmClientLeaderWin)
        return wmClientLeaderWin;
    return win;
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
    if ( wt == NET::Menu ) {
        // ugly hack to support the times when NET::Menu meant NET::TopMenu
        // if it's as wide as the screen, not very high and has its upper-left
        // corner a bit above the screen's upper-left cornet, it's a topmenu
        if( x() == 0 && y() < 0 && y() > -10 && height() < 100
            && abs( width() - workspace()->geometry().width()) < 10 )
            wt = NET::TopMenu;
    }
    return wt;
}

bool Client::wantsTabFocus() const
{
    return (windowType() == NET::Normal || windowType() == NET::Dialog || windowType() == NET::Override )
                          && ( input || Ptakefocus ) && !skip_taskbar;
}


bool Client::wantsInput() const
{
    return input;
}

/*!
  Returns whether the window is moveable or has a fixed
  position. !isMovable implies !isResizable.
 */
bool Client::isMovable() const
{
    return may_move &&
        ( windowType() == NET::Normal || windowType() == NET::Dialog || windowType() == NET::Toolbar
            || windowType() == NET::Menu || windowType() == NET::Override ) &&
        ( !isMaximized() || ( options->moveResizeMaximizedWindows || max_mode != MaximizeFull ) );
}

bool Client::isDesktop() const
{
    return windowType() == NET::Desktop;
}

bool Client::isDock() const
{
    return windowType() == NET::Dock;
}

bool Client::isTopMenu() const
{
    return windowType() == NET::TopMenu;
}

bool Client::isMenu() const
{
    return windowType() == NET::Menu;
}

bool Client::isToolbar() const
{
    return windowType() == NET::Tool;
}

bool Client::isTool() const
{
    return isToolbar();
}

bool Client::isDialog() const
{
    return windowType() == NET::Dialog
        || ( windowType() == NET::Unknown && isTransient())
         // NET::Normal workaround for Qt<3.1 not setting NET::Dialog
        || ( windowType() == NET::Normal && isTransient());
}

bool Client::isNormalWindow() const
{
         // NET::Normal workaround for Qt<3.1 not setting NET::Dialog
    return ( windowType() == NET::Normal && !isTransient())
        || ( windowType() == NET::Unknown && !isTransient());
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
    if ( !options->animateMinimize )
        return;
    // the function is a bit tricky since it will ensure that an
    // animation action needs always the same time regardless of the
    // performance of the machine or the X-Server.

    float lf,rf,tf,bf,step;

    int speed = options->animateMinimizeSpeed;
    if ( speed > 10 )
        speed = 10;
    if ( speed < 0 )
        speed = 0;

    step = 40. * (11 - speed );

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

void Client::shadeHover()
{
    setShade(FALSE, 1);
    delete shadeHoverTimer;
    shadeHoverTimer = 0;
}

/*!
  Clones settings from other client. Used in
  Workspace::slotResetAllClients()
 */
void Client::cloneMode(Client *client)
{
    shaded = client->shaded;
    geom_restore = client->geom_restore;
    max_mode = client->max_mode;
    state = client->state;
    QString caption = client->caption();
    setCaption(caption);
    info->setVisibleName( caption.utf8() );
}

NETWinInfo * Client::netWinInfo()
{
  return static_cast<NETWinInfo *>(info);
}

/*!
  The transient_for window may be embedded in another application,
  so kwin cannot see it. Try to find the managed client for the
  window and fix the transient_for property if possible.
 */
void Client::verifyTransientFor()
{
    unsigned int nwins;
    Window root_return, parent_return, *wins;
    if ( transient_for == 0 || transient_for == win )
        return;
    WId old_transient_for = transient_for;
    while ( transient_for &&
            transient_for != workspace()->rootWin() &&
            !workspace()->findClient( transient_for ) ) {
        wins = 0;
        int r = XQueryTree(qt_xdisplay(), transient_for, &root_return, &parent_return,  &wins, &nwins);
        if ( wins )
            XFree((void *) wins);
        if ( r == 0)
            break;
        transient_for = parent_return;
    }
    if ( old_transient_for != transient_for && workspace()->findClient( transient_for ) )
        XSetTransientForHint( qt_xdisplay(), win, transient_for );
    else
        transient_for = old_transient_for; // nice try
}


/*!\reimp
 */
QString Client::caption() const
{
    return cap;
}

/*!\reimp
 */
void Client::setCaption( const QString & c)
{
    cap = c;
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

QPixmap * kwin_get_menu_pix_hack()
{
  static QPixmap p;
  if ( p.isNull() )
      p = SmallIcon( "bx2" );
  return &p;
}


#include "client.moc"
