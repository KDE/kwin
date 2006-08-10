/*****************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 1999, 2000 Matthias Ettrich <ettrich@kde.org>
Copyright (C) 2003 Lubos Lunak <l.lunak@kde.org>

You can Freely distribute this program under the GNU General Public
License. See the file "COPYING" for the exact licensing terms.
******************************************************************/

#include "client.h"

#include <QApplication>
#include <QPainter>
#include <QDateTime>
#include <kprocess.h>
#include <unistd.h>
#include <kstandarddirs.h>
#include <QWhatsThis>
#include <kwin.h>
#include <kiconloader.h>
#include <stdlib.h>

#include "bridge.h"
#include "group.h"
#include "workspace.h"
#include "atoms.h"
#include "notifications.h"
#include "rules.h"

#include <X11/extensions/shape.h>
#include <QX11Info>

// put all externs before the namespace statement to allow the linker
// to resolve them properly

namespace KWinInternal
{

/*

 Creating a client:
     - only by calling Workspace::createClient()
         - it creates a new client and calls manage() for it

 Destroying a client:
     - destroyClient() - only when the window itself has been destroyed
     - releaseWindow() - the window is kept, only the client itself is destroyed

*/


/*!
  \class Client client.h

  \brief The Client class encapsulates a window decoration frame.

*/

/*!
 This ctor is "dumb" - it only initializes data. All the real initialization
 is done in manage().
 */
Client::Client( Workspace *ws )
    :   QObject( NULL ),
        client( None ),
        wrapper( None ),
        frame( None ),
        decoration( NULL ),
        wspace( ws ),
        bridge( new Bridge( this )),
        move_faked_activity( false ),
        move_resize_grab_window( None ),
        transient_for( NULL ),
        transient_for_id( None ),
        original_transient_for_id( None ),
        in_group( NULL ),
        window_group( None ),
        in_layer( UnknownLayer ),
        ping_timer( NULL ),
        process_killer( NULL ),
        user_time( CurrentTime ), // not known yet
        allowed_actions( 0 ),
        postpone_geometry_updates( 0 ),
        pending_geometry_update( false ),
        shade_geometry_change( false ),
        border_left( 0 ),
        border_right( 0 ),
        border_top( 0 ),
        border_bottom( 0 ),
        sm_stacking_order( -1 ),
        demandAttentionKNotifyTimer( NULL )
// SELI do all as initialization
    {
    autoRaiseTimer = 0;
    shadeHoverTimer = 0;

    // set the initial mapping state
    mapping_state = WithdrawnState;
    desk = 0; // no desktop yet

    mode = PositionCenter;
    buttonDown = false;
    moveResizeMode = false;

    info = NULL;

    shade_mode = ShadeNone;
    active = false;
    deleting = false;
    keep_above = false;
    keep_below = false;
    is_shape = false;
    motif_noborder = false;
    motif_may_move = true;
    motif_may_resize = true;
    motif_may_close = true;
    fullscreen_mode = FullScreenNone;
    skip_taskbar = false;
    original_skip_taskbar = false;
    minimized = false;
    hidden = false;
    modal = false;
    noborder = false;
    user_noborder = false;
    not_obscured = false;
    urgency = false;
    ignore_focus_stealing = false;
    demands_attention = false;
    check_active_modal = false;

    Pdeletewindow = 0;
    Ptakefocus = 0;
    Ptakeactivity = 0;
    Pcontexthelp = 0;
    Pping = 0;
    input = false;
    skip_pager = false;

    max_mode = MaximizeRestore;
    maxmode_restore = MaximizeRestore;
    
    cmap = None;
    
    frame_geometry = QRect( 0, 0, 100, 100 ); // so that decorations don't start with size being (0,0)
    client_size = QSize( 100, 100 );

    // SELI initialize xsizehints??
    }

/*!
  "Dumb" destructor.
 */
Client::~Client()
    {
    assert(!moveResizeMode);
    assert( client == None );
    assert( frame == None && wrapper == None );
    assert( decoration == NULL );
    assert( postpone_geometry_updates == 0 );
    assert( !check_active_modal );
    delete info;
    delete bridge;
    }

// use destroyClient() or releaseWindow(), Client instances cannot be deleted directly
void Client::deleteClient( Client* c, allowed_t )
    {
    delete c;
    }

/*!
  Releases the window. The client has done its job and the window is still existing.
 */
void Client::releaseWindow( bool on_shutdown )
    {
    assert( !deleting );
    deleting = true;
    workspace()->discardUsedWindowRules( this, true ); // remove ForceTemporarily rules
    StackingUpdatesBlocker blocker( workspace());
    if (moveResizeMode)
       leaveMoveResize();
    finishWindowRules();
    ++postpone_geometry_updates;
    setMappingState( WithdrawnState );
    setModal( false ); // otherwise its mainwindow wouldn't get focus
    hidden = true; // so that it's not considered visible anymore (can't use hideClient(), it would set flags)
    if( !on_shutdown )
        workspace()->clientHidden( this );
    XUnmapWindow( display(), frameId()); // destroying decoration would cause ugly visual effect
    destroyDecoration();
    cleanGrouping();
    if( !on_shutdown )
        {
        workspace()->removeClient( this, Allowed );
        // only when the window is being unmapped, not when closing down KWin
        // (NETWM sections 5.5,5.7)
        info->setDesktop( 0 );
        desk = 0;
        info->setState( 0, info->state()); // reset all state flags
        }
    XDeleteProperty( display(), client, atoms->kde_net_wm_user_creation_time);
    XDeleteProperty( display(), client, atoms->net_frame_extents );
    XDeleteProperty( display(), client, atoms->kde_net_wm_frame_strut );
    XReparentWindow( display(), client, workspace()->rootWin(), x(), y());
    XRemoveFromSaveSet( display(), client );
    XSelectInput( display(), client, NoEventMask );
    if( on_shutdown )
        { // map the window, so it can be found after another WM is started
        XMapWindow( display(), client );
	// TODO preserve minimized, shaded etc. state?
        }
    else
        {
        // Make sure it's not mapped if the app unmapped it (#65279). The app
        // may do map+unmap before we initially map the window by calling rawShow() from manage().
        XUnmapWindow( display(), client ); 
        }
    client = None;
    XDestroyWindow( display(), wrapper );
    wrapper = None;
    XDestroyWindow( display(), frame );
    frame = None;
    --postpone_geometry_updates; // don't use GeometryUpdatesBlocker, it would now set the geometry
    deleteClient( this, Allowed );
    }

// like releaseWindow(), but this one is called when the window has been already destroyed
// (e.g. the application closed it)
void Client::destroyClient()
    {
    assert( !deleting );
    deleting = true;
    workspace()->discardUsedWindowRules( this, true ); // remove ForceTemporarily rules
    StackingUpdatesBlocker blocker( workspace());
    if (moveResizeMode)
       leaveMoveResize();
    finishWindowRules();
    ++postpone_geometry_updates;
    setModal( false );
    hidden = true; // so that it's not considered visible anymore
    workspace()->clientHidden( this );
    destroyDecoration();
    cleanGrouping();
    workspace()->removeClient( this, Allowed );
    client = None; // invalidate
    XDestroyWindow( display(), wrapper );
    wrapper = None;
    XDestroyWindow( display(), frame );
    frame = None;
    --postpone_geometry_updates; // don't use GeometryUpdatesBlocker, it would now set the geometry
    deleteClient( this, Allowed );
    }

void Client::updateDecoration( bool check_workspace_pos, bool force )
    {
    if( !force && (( decoration == NULL && noBorder())
                    || ( decoration != NULL && !noBorder())))
        return;
    bool do_show = false;
    postponeGeometryUpdates( true );
    if( force )
        destroyDecoration();
    if( !noBorder())
        {
        decoration = workspace()->createDecoration( bridge );
        // TODO check decoration's minimum size?
        decoration->init();
        decoration->widget()->installEventFilter( this );
        XReparentWindow( display(), decoration->widget()->winId(), frameId(), 0, 0 );
        decoration->widget()->lower();
        decoration->borders( border_left, border_right, border_top, border_bottom );
        int save_workarea_diff_x = workarea_diff_x;
        int save_workarea_diff_y = workarea_diff_y;
        move( calculateGravitation( false ));
        plainResize( sizeForClientSize( clientSize()), ForceGeometrySet );
        workarea_diff_x = save_workarea_diff_x;
        workarea_diff_y = save_workarea_diff_y;
        do_show = true;
        }
    else
        destroyDecoration();
    if( check_workspace_pos )
        checkWorkspacePosition();
    postponeGeometryUpdates( false );
    if( do_show )
        decoration->widget()->show();
    updateFrameExtents();
    }

void Client::destroyDecoration()
    {
    if( decoration != NULL )
        {
        delete decoration;
        decoration = NULL;
        QPoint grav = calculateGravitation( true );
        border_left = border_right = border_top = border_bottom = 0;
        setMask( QRegion()); // reset shape mask
        int save_workarea_diff_x = workarea_diff_x;
        int save_workarea_diff_y = workarea_diff_y;
        plainResize( sizeForClientSize( clientSize()), ForceGeometrySet );
        move( grav );
        workarea_diff_x = save_workarea_diff_x;
        workarea_diff_y = save_workarea_diff_y;
        }
    }

void Client::checkBorderSizes()
    {
    if( decoration == NULL )
        return;
    int new_left, new_right, new_top, new_bottom;
    decoration->borders( new_left, new_right, new_top, new_bottom );
    if( new_left == border_left && new_right == border_right
        && new_top == border_top && new_bottom == border_bottom )
        return;
    GeometryUpdatesPostponer blocker( this );
    move( calculateGravitation( true ));
    border_left = new_left;
    border_right = new_right;
    border_top = new_top;
    border_bottom = new_bottom;
    if (border_left != new_left ||
        border_right != new_right ||
        border_top != new_top ||
        border_bottom != new_bottom)
    move( calculateGravitation( false ));
    plainResize( sizeForClientSize( clientSize()), ForceGeometrySet );
    checkWorkspacePosition();
    }

void Client::detectNoBorder()
    {
    if( Shape::hasShape( window()))
        {
        noborder = true;
        return;
        }
    switch( windowType())
        {
        case NET::Desktop :
        case NET::Dock :
        case NET::TopMenu :
        case NET::Splash :
            noborder = true;
          break;
        case NET::Unknown :
        case NET::Normal :
        case NET::Toolbar :
        case NET::Menu :
        case NET::Dialog :
        case NET::Utility :
            noborder = false;
          break;
        default:
            assert( false );
        }
    // NET::Override is some strange beast without clear definition, usually
    // just meaning "noborder", so let's treat it only as such flag, and ignore it as
    // a window type otherwise (SUPPORTED_WINDOW_TYPES_MASK doesn't include it)
    if( info->windowType( SUPPORTED_WINDOW_TYPES_MASK | NET::OverrideMask ) == NET::Override )
        noborder = true;
    }

void Client::updateFrameExtents()
    {
    NETStrut strut;
    strut.left = border_left;
    strut.right = border_right;
    strut.top = border_top;
    strut.bottom = border_bottom;
    info->setFrameExtents( strut );
    }

// Resizes the decoration, and makes sure the decoration widget gets resize event
// even if the size hasn't changed. This is needed to make sure the decoration
// re-layouts (e.g. when options()->moveResizeMaximizedWindows() changes,
// the decoration may turn on/off some borders, but the actual size
// of the decoration stays the same).
void Client::resizeDecoration( const QSize& s )
    {
    if( decoration == NULL )
        return;
    QSize oldsize = decoration->widget()->size();
    decoration->resize( s );
    if( oldsize == s )
        {
        QResizeEvent e( s, oldsize );
        QApplication::sendEvent( decoration->widget(), &e );
        }
    }

bool Client::noBorder() const
    {
    return noborder || isFullScreen() || user_noborder || motif_noborder;
    }

bool Client::userCanSetNoBorder() const
    {
    return !noborder && !isFullScreen() && !isShade();
    }

bool Client::isUserNoBorder() const
    {
    return user_noborder;
    }

void Client::setUserNoBorder( bool set )
    {
    if( !userCanSetNoBorder())
        return;
    set = rules()->checkNoBorder( set );
    if( user_noborder == set )
        return;
    user_noborder = set;
    updateDecoration( true, false );
    updateWindowRules();
    }

void Client::updateShape()
    {
    // workaround for #19644 - shaped windows shouldn't have decoration
    if( shape() && !noBorder()) 
        {
        noborder = true;
        updateDecoration( true );
        }
    if( shape())
        {
        XShapeCombineShape(display(), frameId(), ShapeBounding,
                           clientPos().x(), clientPos().y(),
                           window(), ShapeBounding, ShapeSet);
        }
    else
        {
        XShapeCombineMask( display(), frameId(), ShapeBounding, 0, 0,
                           None, ShapeSet);
        }
    if( Shape::major() > 1 || Shape::minor() >= 1 ) // has input shape support
        { // there appears to be no way to find out if a window has input
          // shape set or not, so always set propagate the input shape
          // (it's the same like the bounding shape by default)
        XShapeCombineMask( display(), frameId(), ShapeInput, 0, 0,
                           None, ShapeSet );
        XShapeCombineShape( display(), frameId(), ShapeInput,
                           clientPos().x(), clientPos().y(),
                           window(), ShapeBounding, ShapeSubtract );
        XShapeCombineShape( display(), frameId(), ShapeInput,
                           clientPos().x(), clientPos().y(),
                           window(), ShapeInput, ShapeUnion );
        }
    }

void Client::setMask( const QRegion& reg, int mode )
    {
    _mask = reg;
    if( reg.isEmpty())
        XShapeCombineMask( display(), frameId(), ShapeBounding, 0, 0,
            None, ShapeSet );
    else if( mode == X::Unsorted )
        XShapeCombineRegion( display(), frameId(), ShapeBounding, 0, 0,
            reg.handle(), ShapeSet );
    else
        {
        QVector< QRect > rects = reg.rects();
        XRectangle* xrects = new XRectangle[ rects.count() ];
        for( int i = 0;
             i < rects.count();
             ++i )
            {
            xrects[ i ].x = rects[ i ].x();
            xrects[ i ].y = rects[ i ].y();
            xrects[ i ].width = rects[ i ].width();
            xrects[ i ].height = rects[ i ].height();
            }
        XShapeCombineRectangles( display(), frameId(), ShapeBounding, 0, 0,
            xrects, rects.count(), ShapeSet, mode );
        delete[] xrects;
        }
    }

QRegion Client::mask() const
    {
    if( _mask.isEmpty())
        return QRegion( 0, 0, width(), height());
    return _mask;
    }
    
void Client::hideClient( bool hide )
    {
    if( hidden == hide )
        return;
    hidden = hide;
    updateVisibility();
    }
    
/*
  Returns whether the window is minimizable or not
 */
bool Client::isMinimizable() const
    {
    if( isSpecialWindow())
        return false;
    if( isTransient())
        { // #66868 - let other xmms windows be minimized when the mainwindow is minimized
        bool shown_mainwindow = false;
        ClientList mainclients = mainClients();
        for( ClientList::ConstIterator it = mainclients.begin();
             it != mainclients.end();
             ++it )
            {
            if( (*it)->isShown( true ))
                shown_mainwindow = true;
            }
        if( !shown_mainwindow )
            return true;
        }
    // this is here because kicker's taskbar doesn't provide separate entries
    // for windows with an explicitly given parent
    // TODO perhaps this should be redone
    if( transientFor() != NULL )
        return false;
    if( !wantsTabFocus()) // SELI - NET::Utility? why wantsTabFocus() - skiptaskbar? ?
        return false;
    return true;
    }

/*!
  Minimizes this client plus its transients
 */
void Client::minimize( bool avoid_animation )
    {
    if ( !isMinimizable() || isMinimized())
        return;

    Notify::raise( Notify::Minimize );

    // SELI mainClients().isEmpty() ??? - and in unminimize() too
    if ( mainClients().isEmpty() && isOnCurrentDesktop() && isShown( true ) && !avoid_animation )
        animateMinimizeOrUnminimize( true ); // was visible or shaded

    minimized = true;

    updateVisibility();
    updateAllowedActions();
    workspace()->updateMinimizedOfTransients( this );
    updateWindowRules();
    workspace()->updateFocusChains( this, Workspace::FocusChainMakeLast );
    }

void Client::unminimize( bool avoid_animation )
    {
    if( !isMinimized())
        return;

    Notify::raise( Notify::UnMinimize );
    minimized = false;
    if( isOnCurrentDesktop() && isShown( true ))
        {
        if( mainClients().isEmpty() && !avoid_animation )
            animateMinimizeOrUnminimize( false );
        }
    updateVisibility();
    updateAllowedActions();
    workspace()->updateMinimizedOfTransients( this );
    updateWindowRules();
    }

extern bool         blockAnimation;

void Client::animateMinimizeOrUnminimize( bool minimize )
    {
#ifdef __GNUC__
    #warning implement kwin animation
#endif
    if ( 1 || blockAnimation )
        return;
    if ( !options->animateMinimize )
        return;

    if( decoration != NULL && decoration->animateMinimize( minimize ))
        return; // decoration did it

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

    QPixmap pm = animationPixmap( minimize ? width() : icongeom.width() );

    QRect before, after;
    if ( minimize ) 
        {
        before = QRect( x(), y(), width(), pm.height() );
        after = QRect( icongeom.x(), icongeom.y(), icongeom.width(), pm.height() );
        }
    else 
        {
        before = QRect( icongeom.x(), icongeom.y(), icongeom.width(), pm.height() );
        after = QRect( x(), y(), width(), pm.height() );
        }

    lf = (after.left() - before.left())/step;
    rf = (after.right() - before.right())/step;
    tf = (after.top() - before.top())/step;
    bf = (after.bottom() - before.bottom())/step;

    grabXServer();

    QRect area = before;
    QRect area2;
    QPixmap pm2;

    QTime t;
    t.start();
    float diff;

    QPainter p ( workspace()->desktopWidget() );
    bool need_to_clear = false;
    QPixmap pm3;
    do 
        {
        if (area2 != area)
            {
            pm = animationPixmap( area.width() );
            pm2 = QPixmap::grabWindow( rootWindow(), area.x(), area.y(), area.width(), area.height() );
            p.drawPixmap( area.x(), area.y(), pm );
            if ( need_to_clear ) 
                {
                p.drawPixmap( area2.x(), area2.y(), pm3 );
                need_to_clear = false;
                }
            area2 = area;
            }
        XFlush(display());
        XSync( display(), false );
        diff = t.elapsed();
        if (diff > step)
            diff = step;
        area.setLeft(before.left() + int(diff*lf));
        area.setRight(before.right() + int(diff*rf));
        area.setTop(before.top() + int(diff*tf));
        area.setBottom(before.bottom() + int(diff*bf));
        if (area2 != area ) 
            {
            if ( area2.intersects( area ) )
                p.drawPixmap( area2.x(), area2.y(), pm2 );
            else 
                { // no overlap, we can clear later to avoid flicker
                pm3 = pm2;
                need_to_clear = true;
                }
            }
        } while ( t.elapsed() < step);
    if (area2 == area || need_to_clear )
        p.drawPixmap( area2.x(), area2.y(), pm2 );

    p.end();
    ungrabXServer();
    }


/*!
  The pixmap shown during (un)minimalization animation
 */
QPixmap Client::animationPixmap( int w )
    {
    QFont font = options->font(isActive());
    QFontMetrics fm( font );
    QPixmap pm( w, fm.lineSpacing() );
    pm.fill( options->color(Options::ColorTitleBar, isActive() || isMinimized() ) );
    QPainter p( &pm );
    p.setPen(options->color(Options::ColorFont, isActive() || isMinimized() ));
    p.setFont(options->font(isActive()));
    p.drawText( pm.rect(), Qt::AlignLeft|Qt::AlignVCenter|Qt::TextSingleLine, caption() );
    return pm;
    }


bool Client::isShadeable() const
    {
    return !isSpecialWindow() && !noBorder();
    }

void Client::setShade( ShadeMode mode )
    {
    if( !isShadeable())
        return;
    mode = rules()->checkShade( mode );
    if( shade_mode == mode )
        return;
    bool was_shade = isShade();
    ShadeMode was_shade_mode = shade_mode;
    shade_mode = mode;
    if( was_shade == isShade())
        {
        if( decoration != NULL ) // decoration may want to update after e.g. hover-shade changes
            decoration->shadeChange();
        return; // no real change in shaded state
        }

    if( shade_mode == ShadeNormal )
        {
        if ( isShown( true ) && isOnCurrentDesktop())
                Notify::raise( Notify::ShadeUp );
        }
    else if( shade_mode == ShadeNone )
        {
        if( isShown( true ) && isOnCurrentDesktop())
                Notify::raise( Notify::ShadeDown );
        }

    assert( decoration != NULL ); // noborder windows can't be shaded
    GeometryUpdatesPostponer blocker( this );
    // decorations may turn off some borders when shaded
    decoration->borders( border_left, border_right, border_top, border_bottom );

    int as = options->animateShade? 10 : 1;
// TODO all this unmapping, resizing etc. feels too much duplicated from elsewhere
    if ( isShade()) 
        { // shade_mode == ShadeNormal
        // we're about to shade, texx xcompmgr to prepare
        long _shade = 1;
        XChangeProperty(display(), frameId(), atoms->net_wm_window_shade, XA_CARDINAL, 32, PropModeReplace, (unsigned char *) &_shade, 1L);
        // shade
        int h = height();
        shade_geometry_change = true;
        QSize s( sizeForClientSize( QSize( clientSize())));
        s.setHeight( border_top + border_bottom );
        XSelectInput( display(), wrapper, ClientWinMask ); // avoid getting UnmapNotify
        XUnmapWindow( display(), wrapper );
        XUnmapWindow( display(), client );
        XSelectInput( display(), wrapper, ClientWinMask | SubstructureNotifyMask );
        //as we hid the unmap event, xcompmgr didn't recognize the client wid has vanished, so we'll extra inform it        
        //done xcompmgr workaround
// FRAME       repaint( false );
//        bool wasStaticContents = testWFlags( WStaticContents );
//        setWFlags( WStaticContents );
        int step = qMax( 4, QABS( h - s.height() ) / as )+1;
        do 
            {
            h -= step;
            XResizeWindow( display(), frameId(), s.width(), h );
            resizeDecoration( QSize( s.width(), h ));
            QApplication::syncX();
            } while ( h > s.height() + step );
//        if ( !wasStaticContents )
//            clearWFlags( WStaticContents );
        plainResize( s );
        shade_geometry_change = false;
        if( isActive())
            {
            if( was_shade_mode == ShadeHover )
                workspace()->activateNextClient( this );
            else
                workspace()->focusToNull();
            }
        // tell xcompmgr shade's done
        _shade = 2;
        XChangeProperty(display(), frameId(), atoms->net_wm_window_shade, XA_CARDINAL, 32, PropModeReplace, (unsigned char *) &_shade, 1L);    
        }
    else 
        {
        int h = height();
        shade_geometry_change = true;
        QSize s( sizeForClientSize( clientSize()));
// FRAME       bool wasStaticContents = testWFlags( WStaticContents );
//        setWFlags( WStaticContents );
        int step = qMax( 4, QABS( h - s.height() ) / as )+1;
        do 
            {
            h += step;
            XResizeWindow( display(), frameId(), s.width(), h );
            resizeDecoration( QSize( s.width(), h ));
            // assume a border
            // we do not have time to wait for X to send us paint events
// FRAME           repaint( 0, h - step-5, width(), step+5, true);
            QApplication::syncX();
            } while ( h < s.height() - step );
//        if ( !wasStaticContents )
//            clearWFlags( WStaticContents );
        shade_geometry_change = false;
        plainResize( s );
        if( shade_mode == ShadeHover || shade_mode == ShadeActivated )
            setActive( true );
        XMapWindow( display(), wrapperId());
        XMapWindow( display(), window());
        XDeleteProperty (display(), client, atoms->net_wm_window_shade);
        if ( isActive() )
            workspace()->requestFocus( this );
        }
    checkMaximizeGeometry();
    info->setState( isShade() ? NET::Shaded : 0, NET::Shaded );
    info->setState( isShown( false ) ? 0 : NET::Hidden, NET::Hidden );
    updateVisibility();
    updateAllowedActions();
    workspace()->updateMinimizedOfTransients( this );
    decoration->shadeChange();
    updateWindowRules();
    }

void Client::shadeHover()
    {
    setShade( ShadeHover );
    cancelShadeHover();
    }

void Client::cancelShadeHover()
    {
    delete shadeHoverTimer;
    shadeHoverTimer = 0;
    }

void Client::toggleShade()
    {
    // if the mode is ShadeHover or ShadeActive, cancel shade too
    setShade( shade_mode == ShadeNone ? ShadeNormal : ShadeNone );
    }

void Client::updateVisibility()
    {
    if( deleting )
        return;
    bool show = true;
    if( hidden )
        {
        setMappingState( IconicState );
        info->setState( NET::Hidden, NET::Hidden );
        setSkipTaskbar( true, false ); // also hide from taskbar
        rawHide();
        show = false;
        }
    else
        {
        setSkipTaskbar( original_skip_taskbar, false );
        }
    if( minimized )
        {
        setMappingState( IconicState );
        info->setState( NET::Hidden, NET::Hidden );
        rawHide();
        show = false;
        }
    if( show )
        info->setState( 0, NET::Hidden );
    if( !isOnCurrentDesktop())
        {
        setMappingState( IconicState );
        rawHide();
        show = false;
        }
    if( show )
        {
        bool belongs_to_desktop = false;
        for( ClientList::ConstIterator it = group()->members().begin();
             it != group()->members().end();
             ++it )
            if( (*it)->isDesktop())
                {
                belongs_to_desktop = true;
                break;
                }
        if( !belongs_to_desktop && workspace()->showingDesktop())
            workspace()->resetShowingDesktop( true );
        if( isShade())
            setMappingState( IconicState );
        else
            setMappingState( NormalState );
        rawShow();
        }
    }

/*!
  Sets the client window's mapping state. Possible values are
  WithdrawnState, IconicState, NormalState.
 */
void Client::setMappingState(int s)
    {
    assert( client != None );
    assert( !deleting || s == WithdrawnState );
    if( mapping_state == s )
        return;
    bool was_unmanaged = ( mapping_state == WithdrawnState );
    mapping_state = s;
    if( mapping_state == WithdrawnState )
        {
        XDeleteProperty( display(), window(), atoms->wm_state );
        return;
        }
    assert( s == NormalState || s == IconicState );

    unsigned long data[2];
    data[0] = (unsigned long) s;
    data[1] = (unsigned long) None;
    XChangeProperty(display(), window(), atoms->wm_state, atoms->wm_state, 32,
        PropModeReplace, (unsigned char *)data, 2);

    if( was_unmanaged ) // manage() did postpone_geometry_updates = 1, now it's ok to finally set the geometry
        postponeGeometryUpdates( false );
    }

/*!
  Reimplemented to map the managed window in the window wrapper.
  Proper mapping state should be set before showing the client.
 */
void Client::rawShow()
    {
    if( decoration != NULL )
        decoration->widget()->show(); // not really necessary, but let it know the state
    XMapWindow( display(), frame );
    if( !isShade())
        {
        XMapWindow( display(), wrapper );
        XMapWindow( display(), client );
        }
    }

/*!
  Reimplemented to unmap the managed window in the window wrapper.
  Also informs the workspace.
  Proper mapping state should be set before hiding the client.
*/
void Client::rawHide()
    {
// Here it may look like a race condition, as some other client might try to unmap
// the window between these two XSelectInput() calls. However, they're supposed to
// use XWithdrawWindow(), which also sends a synthetic event to the root window,
// which won't be missed, so this shouldn't be a problem. The chance the real UnmapNotify
// will be missed is also very minimal, so I don't think it's needed to grab the server
// here.
    XSelectInput( display(), wrapper, ClientWinMask ); // avoid getting UnmapNotify
    XUnmapWindow( display(), frame );
    XUnmapWindow( display(), wrapper );
    XUnmapWindow( display(), client );
    XSelectInput( display(), wrapper, ClientWinMask | SubstructureNotifyMask );
    if( decoration != NULL )
        decoration->widget()->hide(); // not really necessary, but let it know the state
    workspace()->clientHidden( this );
    }

void Client::sendClientMessage(Window w, Atom a, Atom protocol, long data1, long data2, long data3)
    {
    XEvent ev;
    long mask;

    memset(&ev, 0, sizeof(ev));
    ev.xclient.type = ClientMessage;
    ev.xclient.window = w;
    ev.xclient.message_type = a;
    ev.xclient.format = 32;
    ev.xclient.data.l[0] = protocol;
    ev.xclient.data.l[1] = xTime();
    ev.xclient.data.l[2] = data1;
    ev.xclient.data.l[3] = data2;
    ev.xclient.data.l[4] = data3;
    mask = 0L;
    if (w == rootWindow())
      mask = SubstructureRedirectMask;        /* magic! */
    XSendEvent(display(), w, False, mask, &ev);
    }

/*
  Returns whether the window may be closed (have a close button)
 */
bool Client::isCloseable() const
    {
    return rules()->checkCloseable( motif_may_close && !isSpecialWindow());
    }

/*!
  Closes the window by either sending a delete_window message or
  using XKill.
 */
void Client::closeWindow()
    {
    if( !isCloseable())
        return;
    // Update user time, because the window may create a confirming dialog.
    updateUserTime(); 
    if ( Pdeletewindow )
        {
        Notify::raise( Notify::Close );
        sendClientMessage( window(), atoms->wm_protocols, atoms->wm_delete_window);
        pingWindow();
        }
    else 
        {
        // client will not react on wm_delete_window. We have not choice
        // but destroy his connection to the XServer.
        killWindow();
        }
    }


/*!
  Kills the window via XKill
 */
void Client::killWindow()
    {
    kDebug( 1212 ) << "Client::killWindow():" << caption() << endl;
    // not sure if we need an Notify::Kill or not.. until then, use
    // Notify::Close
    Notify::raise( Notify::Close );

    if( isDialog())
        Notify::raise( Notify::TransDelete );
    if( isNormalWindow())
        Notify::raise( Notify::Delete );
    killProcess( false );
    // always kill this client at the server
    XKillClient(display(), window() );
    destroyClient();
    }

// send a ping to the window using _NET_WM_PING if possible
// if it doesn't respond within a reasonable time, it will be
// killed
void Client::pingWindow()
    {
    if( !Pping )
        return; // can't ping :(
    if( options->killPingTimeout == 0 )
        return; // turned off
    if( ping_timer != NULL )
        return; // pinging already
    ping_timer = new QTimer( this );
    connect( ping_timer, SIGNAL( timeout()), SLOT( pingTimeout()));
    ping_timer->setSingleShot( true );
    ping_timer->start( options->killPingTimeout );
    ping_timestamp = xTime();
    workspace()->sendPingToWindow( window(), ping_timestamp );
    }

void Client::gotPing( Time timestamp )
    {
    if( timestamp != ping_timestamp )
        return;
    delete ping_timer;
    ping_timer = NULL;
    if( process_killer != NULL )
        {
        process_killer->kill();
        delete process_killer;
        process_killer = NULL;
        }
    }

void Client::pingTimeout()
    {
    kDebug( 1212 ) << "Ping timeout:" << caption() << endl;
    delete ping_timer;
    ping_timer = NULL;
    killProcess( true, ping_timestamp );
    }

void Client::killProcess( bool ask, Time timestamp )
    {
    if( process_killer != NULL )
        return;
    Q_ASSERT( !ask || timestamp != CurrentTime );
    QByteArray machine = wmClientMachine( true );
    pid_t pid = info->pid();
    if( pid <= 0 || machine.isEmpty()) // needed properties missing
        return;
    kDebug( 1212 ) << "Kill process:" << pid << "(" << machine << ")" << endl;
    if( !ask )
        {
        if( machine != "localhost" )
            {
            KProcess proc;
            proc << "xon" << machine << "kill" << QString::number( pid );
            proc.start( KProcess::DontCare );
            }
        else
            ::kill( pid, SIGTERM );
        }
    else
        { // SELI TODO handle the window created by handler specially (on top,urgent?)
        process_killer = new KProcess( this );
        *process_killer << KStandardDirs::findExe( "kwin_killer_helper" )
            << "--pid" << QByteArray().setNum( pid ) << "--hostname" << machine
            << "--windowname" << caption().toUtf8()
            << "--applicationname" << resourceClass()
            << "--wid" << QString::number( window() )
            << "--timestamp" << QString::number( timestamp );
        connect( process_killer, SIGNAL( processExited( KProcess* )),
            SLOT( processKillerExited()));
        if( !process_killer->start( KProcess::NotifyOnExit ))
            {
            delete process_killer;
            process_killer = NULL;
            return;
            }
        }
    }

void Client::processKillerExited()
    {
    kDebug( 1212 ) << "Killer exited" << endl;
    delete process_killer;
    process_killer = NULL;
    }

void Client::setSkipTaskbar( bool b, bool from_outside )
    {
    int was_wants_tab_focus = wantsTabFocus();
    if( from_outside )
        {
        b = rules()->checkSkipTaskbar( b );
        original_skip_taskbar = b;
        }
    if ( b == skipTaskbar() )
        return;
    skip_taskbar = b;
    info->setState( b?NET::SkipTaskbar:0, NET::SkipTaskbar );
    updateWindowRules();
    if( was_wants_tab_focus != wantsTabFocus())
        workspace()->updateFocusChains( this,
            isActive() ? Workspace::FocusChainMakeFirst : Workspace::FocusChainUpdate );
    }

void Client::setSkipPager( bool b )
    {
    b = rules()->checkSkipPager( b );
    if ( b == skipPager() )
        return;
    skip_pager = b;
    info->setState( b?NET::SkipPager:0, NET::SkipPager );
    updateWindowRules();
    }

void Client::setModal( bool m )
    { // Qt-3.2 can have even modal normal windows :(
    if( modal == m )
        return;
    modal = m;
    if( !modal )
        return;
    // changing modality for a mapped window is weird (?)
    // _NET_WM_STATE_MODAL should possibly rather be _NET_WM_WINDOW_TYPE_MODAL_DIALOG
    }

void Client::setDesktop( int desktop )
    {
    if( desktop != NET::OnAllDesktops ) // do range check
        desktop = qMax( 1, qMin( workspace()->numberOfDesktops(), desktop ));
    desktop = rules()->checkDesktop( desktop );
    if( desk == desktop )
        return;
    int was_desk = desk;
    desk = desktop;
    info->setDesktop( desktop );
    if(( was_desk == NET::OnAllDesktops ) != ( desktop == NET::OnAllDesktops ))
        { // onAllDesktops changed
        if ( isShown( true ))
            Notify::raise( isOnAllDesktops() ? Notify::OnAllDesktops : Notify::NotOnAllDesktops );
        workspace()->updateOnAllDesktopsOfTransients( this );
        }
    if( decoration != NULL )
        decoration->desktopChange();
    workspace()->updateFocusChains( this, Workspace::FocusChainMakeFirst );
    updateVisibility();
    updateWindowRules();
    }

void Client::setOnAllDesktops( bool b )
    {
    if(( b && isOnAllDesktops())
        || ( !b && !isOnAllDesktops()))
        return;
    if( b )
        setDesktop( NET::OnAllDesktops );
    else
        setDesktop( workspace()->currentDesktop());
    }

bool Client::isOnCurrentDesktop() const
    {
    return isOnDesktop( workspace()->currentDesktop());
    }

// performs activation and/or raising of the window
void Client::takeActivity( int flags, bool handled, allowed_t )
    {
    if( !handled || !Ptakeactivity )
        {
        if( flags & ActivityFocus )
            takeFocus( Allowed );
        if( flags & ActivityRaise )
            workspace()->raiseClient( this );
        return;
        }

#ifndef NDEBUG
    static Time previous_activity_timestamp;
    static Client* previous_client;
    if( previous_activity_timestamp == xTime() && previous_client != this )
        {
        kDebug( 1212 ) << "Repeated use of the same X timestamp for activity" << endl;
        kDebug( 1212 ) << kBacktrace() << endl;
        }
    previous_activity_timestamp = xTime();
    previous_client = this;
#endif
    workspace()->sendTakeActivity( this, xTime(), flags );
    }

// performs the actual focusing of the window using XSetInputFocus and WM_TAKE_FOCUS
void Client::takeFocus( allowed_t )
    {
#ifndef NDEBUG
    static Time previous_focus_timestamp;
    static Client* previous_client;
    if( previous_focus_timestamp == xTime() && previous_client != this )
        {
        kDebug( 1212 ) << "Repeated use of the same X timestamp for focus" << endl;
        kDebug( 1212 ) << kBacktrace() << endl;
        }
    previous_focus_timestamp = xTime();
    previous_client = this;
#endif
    if ( rules()->checkAcceptFocus( input ))
        {
        XSetInputFocus( display(), window(), RevertToPointerRoot, xTime() );
        }
    if ( Ptakefocus )
        sendClientMessage(window(), atoms->wm_protocols, atoms->wm_take_focus);
    workspace()->setShouldGetFocus( this );
    }

/*!
  Returns whether the window provides context help or not. If it does,
  you should show a help menu item or a help button like '?' and call
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
void Client::showContextHelp()
    {
    if ( Pcontexthelp ) 
        {
        sendClientMessage(window(), atoms->wm_protocols, atoms->net_wm_context_help);
        QWhatsThis::enterWhatsThisMode(); // SELI?
        }
    }


/*!
  Fetches the window's caption (WM_NAME property). It will be
  stored in the client's caption().
 */
void Client::fetchName()
    {
    setCaption( readName());
    }

QString Client::readName() const
    {
    if ( info->name() && info->name()[ 0 ] != '\0' ) 
        return QString::fromUtf8( info->name() );
    else 
        return KWin::readNameProperty( window(), XA_WM_NAME );
    }
    
KWIN_COMPARE_PREDICATE( FetchNameInternalPredicate, const Client*, (!cl->isSpecialWindow() || cl->isToolbar()) && cl != value && cl->caption() == value->caption());

void Client::setCaption( const QString& _s, bool force )
    {
	QString s = _s;
    if ( s != cap_normal || force ) 
        {
        bool reset_name = force;
        for( int i = 0;
             i < s.length();
             ++i )
            if( !s[ i ].isPrint())
                s[ i ] = QChar( ' ' );
        cap_normal = s;
        bool was_suffix = ( !cap_suffix.isEmpty());
        QString machine_suffix;
        if( wmClientMachine( false ) != "localhost" && !isLocalMachine( wmClientMachine( false )))
            machine_suffix = " <@" + wmClientMachine( true ) + '>';
        QString shortcut_suffix = !shortcut().isNull() ? ( " {" + shortcut().toString() + '}' ) : "";
        cap_suffix = machine_suffix + shortcut_suffix;
        if ( ( !isSpecialWindow() || isToolbar()) && workspace()->findClient( FetchNameInternalPredicate( this ))) 
            {
            int i = 2;
            do 
                {
                cap_suffix = machine_suffix + " <" + QString::number(i) + '>' + shortcut_suffix;
                i++;
                } while ( workspace()->findClient( FetchNameInternalPredicate( this )));
            info->setVisibleName( caption().toUtf8() );
            reset_name = false;
            }
        if(( was_suffix && cap_suffix.isEmpty()
            || reset_name )) // if it was new window, it may have old value still set, if the window is reused
            {
            info->setVisibleName( "" ); // remove
            info->setVisibleIconName( "" ); // remove
            }
        else if( !cap_suffix.isEmpty() && !cap_iconic.isEmpty()) // keep the same suffix in iconic name if it's set
            info->setVisibleIconName( ( cap_iconic + cap_suffix ).toUtf8() );

        if( isManaged() && decoration != NULL )
                decoration->captionChange();
        }
    }

void Client::updateCaption()
    {
    setCaption( cap_normal, true );
    }

void Client::fetchIconicName()
    {
    QString s;
    if ( info->iconName() && info->iconName()[ 0 ] != '\0' ) 
        s = QString::fromUtf8( info->iconName() );
    else 
        s = KWin::readNameProperty( window(), XA_WM_ICON_NAME );
    if ( s != cap_iconic ) 
        {
	bool was_set = !cap_iconic.isEmpty();
        cap_iconic = s;
        if( !cap_suffix.isEmpty())
	    {
	    if( !cap_iconic.isEmpty()) // keep the same suffix in iconic name if it's set
        	info->setVisibleIconName( ( s + cap_suffix ).toUtf8() );
	    else if( was_set )
		info->setVisibleIconName( "" ); //remove
	    }
        }
    }

/*!\reimp
 */
QString Client::caption( bool full ) const
    {
    return full ? cap_normal + cap_suffix : cap_normal;
    }

void Client::getWMHints()
    {
    XWMHints *hints = XGetWMHints(display(), window() );
    input = true;
    window_group = None;
    urgency = false;
    if ( hints )
        {
        if( hints->flags & InputHint )
            input = hints->input;
        if( hints->flags & WindowGroupHint )
            window_group = hints->window_group;
        urgency = ( hints->flags & UrgencyHint ) ? true : false; // true/false needed, it's uint bitfield
        XFree( (char*)hints );
        }
    checkGroup();
    updateUrgency();
    updateAllowedActions(); // group affects isMinimizable()
    }

void Client::getMotifHints()
    {
    bool mnoborder, mresize, mmove, mminimize, mmaximize, mclose;
    Motif::readFlags( client, mnoborder, mresize, mmove, mminimize, mmaximize, mclose );
    motif_noborder = mnoborder;
    if( !hasNETSupport()) // NETWM apps should set type and size constraints
        {
        motif_may_resize = mresize; // this should be set using minsize==maxsize, but oh well
        motif_may_move = mmove;
        }
    else
        motif_may_resize = motif_may_move = true;
    // mminimize; - ignore, bogus - e.g. shading or sending to another desktop is "minimizing" too
    // mmaximize; - ignore, bogus - maximizing is basically just resizing
    motif_may_close = mclose; // motif apps like to crash when they set this hint and WM closes them anyway
    if( isManaged())
        updateDecoration( true ); // check if noborder state has changed
    }

void Client::readIcons( Window win, QPixmap* icon, QPixmap* miniicon )
    {    
    // get the icons, allow scaling
    if( icon != NULL )
        *icon = KWin::icon( win, 32, 32, true, KWin::NETWM | KWin::WMHints );
    if( miniicon != NULL )
        if( icon == NULL || !icon->isNull())
            *miniicon = KWin::icon( win, 16, 16, true, KWin::NETWM | KWin::WMHints );
        else
            *miniicon = QPixmap();
    }

void Client::getIcons()
    {
    // first read icons from the window itself
    readIcons( window(), &icon_pix, &miniicon_pix );
    if( icon_pix.isNull())
        { // then try window group
        icon_pix = group()->icon();
        miniicon_pix = group()->miniIcon();
        }
    if( icon_pix.isNull() && isTransient())
        { // then mainclients
        ClientList mainclients = mainClients();
        for( ClientList::ConstIterator it = mainclients.begin();
             it != mainclients.end() && icon_pix.isNull();
             ++it )
            {
            icon_pix = (*it)->icon();
            miniicon_pix = (*it)->miniIcon();
            }
        }
    if( icon_pix.isNull())
        { // and if nothing else, load icon from classhint or xapp icon
        icon_pix = KWin::icon( window(), 32, 32, true, KWin::ClassHint | KWin::XApp );
        miniicon_pix = KWin::icon( window(), 16, 16, true, KWin::ClassHint | KWin::XApp );
        }
    if( isManaged() && decoration != NULL )
        decoration->iconChange();
    }

void Client::getWindowProtocols()
    {
    Atom *p;
    int i,n;

    Pdeletewindow = 0;
    Ptakefocus = 0;
    Ptakeactivity = 0;
    Pcontexthelp = 0;
    Pping = 0;

    if (XGetWMProtocols(display(), window(), &p, &n))
        {
        for (i = 0; i < n; i++)
            if (p[i] == atoms->wm_delete_window)
                Pdeletewindow = 1;
            else if (p[i] == atoms->wm_take_focus)
                Ptakefocus = 1;
            else if (p[i] == atoms->net_wm_take_activity)
                Ptakeactivity = 1;
            else if (p[i] == atoms->net_wm_context_help)
                Pcontexthelp = 1;
            else if (p[i] == atoms->net_wm_ping)
                Pping = 1;
        if (n>0)
            XFree(p);
        }
    }

static int nullErrorHandler(Display *, XErrorEvent *)
    {
    return 0;
    }

/*!
  Returns WM_WINDOW_ROLE property for a given window.
 */
QByteArray Client::staticWindowRole(WId w)
    {
    return getStringProperty(w, atoms->wm_window_role).toLower();
    }

/*!
  Returns SM_CLIENT_ID property for a given window.
 */
QByteArray Client::staticSessionId(WId w)
    {
    return getStringProperty(w, atoms->sm_client_id);
    }

/*!
  Returns WM_COMMAND property for a given window.
 */
QByteArray Client::staticWmCommand(WId w)
    {
    return getStringProperty(w, XA_WM_COMMAND, ' ');
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
    status = XGetWindowProperty( display(), w, atoms->wm_client_leader, 0, 10000,
                                 false, XA_WINDOW, &type, &format,
                                 &nitems, &extra, &data );
    XSetErrorHandler(oldHandler);
    if (status  == Success ) 
        {
        if (data && nitems > 0)
            result = *((Window*) data);
        XFree(data);
        }
    return result;
    }


void Client::getWmClientLeader()
    {
    wmClientLeaderWin = staticWmClientLeader(window());
    }

/*!
  Returns sessionId for this client,
  taken either from its window or from the leader window.
 */
QByteArray Client::sessionId()
    {
    QByteArray result = staticSessionId(window());
    if (result.isEmpty() && wmClientLeaderWin && wmClientLeaderWin!=window())
        result = staticSessionId(wmClientLeaderWin);
    return result;
    }

/*!
  Returns command property for this client,
  taken either from its window or from the leader window.
 */
QByteArray Client::wmCommand()
    {
    QByteArray result = staticWmCommand(window());
    if (result.isEmpty() && wmClientLeaderWin && wmClientLeaderWin!=window())
        result = staticWmCommand(wmClientLeaderWin);
    return result;
    }

void Client::getWmClientMachine()
    {
    client_machine = getStringProperty(window(), XA_WM_CLIENT_MACHINE);
    if( client_machine.isEmpty() && wmClientLeaderWin && wmClientLeaderWin!=window())
        client_machine = getStringProperty(wmClientLeaderWin, XA_WM_CLIENT_MACHINE);
    if( client_machine.isEmpty())
        client_machine = "localhost";
    }

/*!
  Returns client machine for this client,
  taken either from its window or from the leader window.
*/
QByteArray Client::wmClientMachine( bool use_localhost ) const
    {
    QByteArray result = client_machine;
    if( use_localhost )
        { // special name for the local machine (localhost)
        if( result != "localhost" && isLocalMachine( result ))
            result = "localhost";
        }
    return result;
    }

/*!
  Returns client leader window for this client.
  Returns the client window itself if no leader window is defined.
*/
Window Client::wmClientLeader() const
    {
    if (wmClientLeaderWin)
        return wmClientLeaderWin;
    return window();
    }

bool Client::wantsTabFocus() const
    {
    return ( isNormalWindow() || isDialog()) && wantsInput();
    }


bool Client::wantsInput() const
    {
    return rules()->checkAcceptFocus( input || Ptakefocus );
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
    return windowType() == NET::Menu && !isTopMenu(); // because of backwards comp.
    }

bool Client::isToolbar() const
    {
    return windowType() == NET::Toolbar;
    }

bool Client::isSplash() const
    {
    return windowType() == NET::Splash;
    }

bool Client::isUtility() const
    {
    return windowType() == NET::Utility;
    }

bool Client::isDialog() const
    {
    return windowType() == NET::Dialog;
    }

bool Client::isNormalWindow() const
    {
    return windowType() == NET::Normal;
    }

bool Client::isSpecialWindow() const
    {
    return isDesktop() || isDock() || isSplash() || isTopMenu()
        || isToolbar(); // TODO
    }

NET::WindowType Client::windowType( bool direct, int supported_types ) const
    {
    NET::WindowType wt = info->windowType( supported_types );
    if( direct )
        return wt;
    NET::WindowType wt2 = rules()->checkType( wt );
    if( wt != wt2 )
        {
        wt = wt2;
        info->setWindowType( wt ); // force hint change
        }
    // hacks here
    if( wt == NET::Menu )
        {
        // ugly hack to support the times when NET::Menu meant NET::TopMenu
        // if it's as wide as the screen, not very high and has its upper-left
        // corner a bit above the screen's upper-left cornet, it's a topmenu
        if( x() == 0 && y() < 0 && y() > -10 && height() < 100
            && abs( width() - workspace()->clientArea( FullArea, this ).width()) < 10 )
            wt = NET::TopMenu;
        }
    // TODO change this to rule
    const char* const oo_prefix = "openoffice.org"; // QByteArray has no startsWith()
    // oo_prefix is lowercase, because resourceClass() is forced to be lowercase
    if( qstrncmp( resourceClass(), oo_prefix, strlen( oo_prefix )) == 0 && wt == NET::Dialog )
        wt = NET::Normal; // see bug #66065
    if( wt == NET::Unknown ) // this is more or less suggested in NETWM spec
        wt = isTransient() ? NET::Dialog : NET::Normal;
    return wt;
    }

/*!
  Sets an appropriate cursor shape for the logical mouse position \a m

 */
void Client::setCursor( Position m )
    {
    if( !isResizable() || isShade())
        {
        m = PositionCenter;
        }
    switch ( m ) 
        {
        case PositionTopLeft:
        case PositionBottomRight:
            setCursor( Qt::SizeFDiagCursor );
            break;
        case PositionBottomLeft:
        case PositionTopRight:
            setCursor( Qt::SizeBDiagCursor );
            break;
        case PositionTop:
        case PositionBottom:
            setCursor( Qt::SizeVerCursor );
            break;
        case PositionLeft:
        case PositionRight:
            setCursor( Qt::SizeHorCursor );
            break;
        default:
            if( buttonDown && isMovable())
                setCursor( Qt::SizeAllCursor );
            else
                setCursor( Qt::ArrowCursor );
            break;
        }
    }

// TODO mit nejake checkCursor(), ktere se zavola v manage() a pri vecech, kdy by se kurzor mohl zmenit?
void Client::setCursor( const QCursor& c )
    {
    if( c.handle() == cursor.handle())
        return;
    cursor = c;
    if( decoration != NULL )
        decoration->widget()->setCursor( cursor );
    XDefineCursor( display(), frameId(), cursor.handle());
    }

Client::Position Client::mousePosition( const QPoint& p ) const
    {
    if( decoration != NULL )
        return decoration->mousePosition( p );
    return PositionCenter;
    }

void Client::updateAllowedActions( bool force )
    {
    if( !isManaged() && !force )
        return;
    unsigned long old_allowed_actions = allowed_actions;
    allowed_actions = 0;
    if( isMovable())
        allowed_actions |= NET::ActionMove;
    if( isResizable())
        allowed_actions |= NET::ActionResize;
    if( isMinimizable())
        allowed_actions |= NET::ActionMinimize;
    if( isShadeable())
        allowed_actions |= NET::ActionShade;
    // sticky state not supported
    if( isMaximizable())
        allowed_actions |= NET::ActionMax;
    if( userCanSetFullScreen())
        allowed_actions |= NET::ActionFullScreen;
    allowed_actions |= NET::ActionChangeDesktop; // always (pagers shouldn't show Docks etc.)
    if( isCloseable())
        allowed_actions |= NET::ActionClose;
    if( old_allowed_actions == allowed_actions )
        return;
    // TODO this could be delayed and compressed - it's only for pagers etc. anyway
    info->setAllowedActions( allowed_actions );
    // TODO this should also tell the decoration, so that it can update the buttons
    }

void Client::autoRaise()
    {
    workspace()->raiseClient( this );
    cancelAutoRaise();
    }
    
void Client::cancelAutoRaise()
    {
    delete autoRaiseTimer;
    autoRaiseTimer = 0;
    }

#ifndef NDEBUG
kdbgstream& operator<<( kdbgstream& stream, const Client* cl )
    {
    if( cl == NULL )
        return stream << "\'NULL_CLIENT\'";
    return stream << "\'ID:" << cl->window() << ";WMCLASS:" << cl->resourceClass() << ":" << cl->resourceName() << ";Caption:" << cl->caption() << "\'";
    }
kdbgstream& operator<<( kdbgstream& stream, const ClientList& list )
    {
    stream << "LIST:(";
    bool first = true;
    for( ClientList::ConstIterator it = list.begin();
         it != list.end();
         ++it )
        {
        if( !first )
            stream << ":";
        first = false;
        stream << *it;
        }
    stream << ")";
    return stream;
    }
kdbgstream& operator<<( kdbgstream& stream, const ConstClientList& list )
    {
    stream << "LIST:(";
    bool first = true;
    for( ConstClientList::ConstIterator it = list.begin();
         it != list.end();
         ++it )
        {
        if( !first )
            stream << ":";
        first = false;
        stream << *it;
        }
    stream << ")";
    return stream;
    }
#endif

QPixmap * kwin_get_menu_pix_hack()
    {
    static QPixmap p;
    if ( p.isNull() )
        p = SmallIcon( "bx2" );
    return &p;
    }

} // namespace

#include "client.moc"
