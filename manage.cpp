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

#include <kstartupinfo.h>
#include <X11/extensions/shape.h>

#include "notifications.h"

extern Time qt_x_time;
extern Atom qt_window_role;

namespace KWinInternal
{

/*!
  Manages the clients. This means handling the very first maprequest:
  reparenting, initial geometry, initial state, placement, etc.
  Returns false if KWin is not going to manage this window.
 */
bool Client::manage( Window w, bool isMapped, bool useCursorPos )
    {
    XWindowAttributes attr;
    if( !XGetWindowAttributes(qt_xdisplay(), w, &attr))
        return false;

//    XGrabServer( qt_xdisplay()); // FRAME

    // from this place on, manage() mustn't return false
    block_geometry = 1;

    embedClient( w );

    // SELI order all these things in some sane manner

    bool init_minimize = false;
    XWMHints * hints = XGetWMHints(qt_xdisplay(), w );
    if (hints && (hints->flags & StateHint) && hints->initial_state == IconicState)
        init_minimize = true;
    if (hints)
        XFree(hints);
    if( isMapped )
        init_minimize = false; // if it's already mapped, ignore hint

    unsigned long properties[ 2 ];
    properties[ WinInfo::PROTOCOLS ] =
        NET::WMDesktop |
        NET::WMState |
        NET::WMWindowType |
        NET::WMStrut |
        NET::WMName |
        NET::WMIconGeometry |
        NET::WMIcon |
        NET::WMPid |
        NET::WMIconName |
        0;
    properties[ WinInfo::PROTOCOLS2 ] =
        NET::WM2UserTime |
        NET::WM2StartupId |
        0;

    info = new WinInfo( this, qt_xdisplay(), client, qt_xrootwin(), properties, 2 );

    cmap = attr.colormap;

    bool mresize, mmove, mminimize, mmaximize, mclose;
    if( Motif::funcFlags( client, mresize, mmove, mminimize, mmaximize, mclose )) 
        {
        if( !hasNETSupport()) // NETWM apps should set type and size constraints
            {
            motif_may_resize = mresize; // this should be set using minsize==maxsize, but oh well
            motif_may_move = mmove;
            }
        // mminimize; - ignore, bogus - e.g. shading or sending to another desktop is "minimizing" too
        // mmaximize; - ignore, bogus - maximizing is basically just resizing
        motif_may_close = mclose; // motif apps like to crash when they set this hint and WM closes them anyway
        }

    XClassHint classHint;
    if ( XGetClassHint( qt_xdisplay(), client, &classHint ) ) 
        {
        resource_name = classHint.res_name;
        resource_class = classHint.res_class;
        XFree( classHint.res_name );
        XFree( classHint.res_class );
        }

    detectNoBorder();
    fetchName();
    fetchIconicName();
    getWMHints();
    readTransient();
    getIcons();
    getWindowProtocols();
    getWmNormalHints(); // get xSizeHint
    getWmClientLeader();
    window_role = getStringProperty( w, qt_window_role );

    // TODO try to obey all state information from info->state()

    original_skip_taskbar = skip_taskbar = ( info->state() & NET::SkipTaskbar) != 0;
    skip_pager = ( info->state() & NET::SkipPager) != 0;
    modal = ( info->state() & NET::Modal ) != 0;

    // window wants to stay on top?
    keep_above = ( info->state() & NET::KeepAbove ) != 0;
    // window wants to stay on bottom?
    keep_below = ( info->state() & NET::KeepBelow ) != 0;
    if( keep_above && keep_below )
        keep_above = keep_below = false;

    KStartupInfoData asn_data;
    bool asn_valid = workspace()->checkStartupNotification( this, asn_data );

    workspace()->updateClientLayer( this );

    SessionInfo* session = workspace()->takeSessionInfo( this );
    if ( session )
        {
        if ( session->minimized )
            init_minimize = true;
        if( session->userNoBorder )
            setUserNoBorder( true );
        }

    // initial desktop placement
    if ( info->desktop() )
        desk = info->desktop(); // window had the initial desktop property!
    else if( asn_valid && asn_data.desktop() != 0 )
        desk = asn_data.desktop();
    if ( session ) 
        {
        desk = session->desktop;
        if( session->onAllDesktops )
            desk = NET::OnAllDesktops;
        }
    else if ( desk == 0 ) 
        {
        // if this window is transient, ensure that it is opened on the
        // same window as its parent.  this is necessary when an application
        // starts up on a different desktop than is currently displayed
        if( isTransient())
            {
            ClientList mainclients = mainClients();
            bool on_current = false;
            for( ClientList::ConstIterator it = mainclients.begin();
                 it != mainclients.end();
                 ++it )
                if( (*it)->isOnCurrentDesktop())
                    on_current = true;
            if( on_current )
                desk = workspace()->currentDesktop();
            else if( mainclients.count() > 0 )
                desk = mainclients.first()->desktop();
            }
        }
    if ( desk == 0 ) // assume window wants to be visible on the current desktop
        desk = workspace()->currentDesktop();
    info->setDesktop( desk );
    workspace()->updateOnAllDesktopsOfTransients( this ); // SELI
//    onAllDesktopsChange(); decoration doesn't exist here yet

    QRect geom( attr.x, attr.y, attr.width, attr.height );
    bool placementDone = FALSE;

    if ( session )
        geom = session->geometry;

    QRect area = workspace()->clientArea( PlacementArea, useCursorPos ? QCursor::pos() : geom.center(), desktop());

    // if it's noborder window, and has size of one screen or the whole desktop geometry, it's fullscreen hack
    if( ( geom.size() == workspace()->clientArea( FullArea, geom.center(), desktop()).size()
            || geom.size() == workspace()->clientArea( ScreenArea, geom.center(), desktop()).size())
        && noBorder() && !isUserNoBorder() && isFullScreenable()) 
        {
        fullscreen_mode = FullScreenHack;
        geom = workspace()->clientArea( MaximizeFullArea, geom.center(), desktop());
        placementDone = true;
        }

    if ( isDesktop() ) 
        {
        // desktops are treated slightly special
        geom = workspace()->clientArea( FullArea, geom.center(), desktop());
        placementDone = true;
        }

    if ( isMapped || session || placementDone
         || ( isTransient() && !isUtility() && !isDialog() && !isSplash())) 
        {  // TODO
        placementDone = TRUE;
        }
    else if( isTransient() && !hasNETSupport())
        placementDone = true;
    else if( isDialog() && hasNETSupport()) // see Placement::placeDialog()
        ; // force using placement policy
    else if( isSplash())
        ; // force using placement policy
    else
        {

        bool ignorePPosition = false;
        XClassHint classHint; // APICLEAN tohle je zduplikovane seshora
        if ( XGetClassHint(qt_xdisplay(), window(), &classHint) != 0 ) 
            {
            if ( classHint.res_class )
                ignorePPosition = ( options->ignorePositionClasses.find(QString::fromLatin1(classHint.res_class)) != options->ignorePositionClasses.end() );
            XFree(classHint.res_name);
            XFree(classHint.res_class);
            }

        if ((xSizeHint.flags & PPosition) && ! ignorePPosition) 
            {
            int tx = geom.x();
            int ty = geom.y();

// TODO tyhle testy nepocitaji s dekoraci, ani s gravity
            if (tx < 0)
                tx = area.right() + tx;
            if (ty < 0)
                ty = area.bottom() + ty;
            geom.moveTopLeft(QPoint(tx, ty));
            }

        if ( ( (xSizeHint.flags & PPosition) && !ignorePPosition ) ||
             (xSizeHint.flags & USPosition) ) 
            {
            placementDone = TRUE;
            }
        if ( (xSizeHint.flags & USSize) || (xSizeHint.flags & PSize) ) 
            {
            // keep in mind that we now actually have a size :-)
            }
        }

    if (xSizeHint.flags & PMaxSize)
        geom.setSize( geom.size().boundedTo( QSize(xSizeHint.max_width, xSizeHint.max_height ) ) );
    if (xSizeHint.flags & PMinSize)
        geom.setSize( geom.size().expandedTo( QSize(xSizeHint.min_width, xSizeHint.min_height ) ) );

    if( isMovable())
        {
        if( geom.x() > area.right() || geom.y() > area.bottom())
            placementDone = FALSE; // weird, do not trust.
        }

    if ( placementDone )
        move( geom.x(), geom.y() ); // before gravitating

    updateDecoration( false ); // also gravitates
    // TODO is CentralGravity right here, when resizing is done after gravitating?
    plainResize( sizeForClientSize( geom.size()));

    if( !placementDone ) 
        { // placement needs to be after setting size
        workspace()->place( this );
        placementDone = TRUE;
        }

    if (( !isSpecialWindow() || isToolbar()) && isMovable())
        {
        if ( geometry().right() > area.right() && width() < area.width() )
            move( area.right() - width(), y() );
        if ( geometry().bottom() > area.bottom() && height() < area.height() )
            move( x(), area.bottom() - height() );
        if( !area.contains( geometry().topLeft() ))
            {
            int tx = x();
            int ty = y();
            if ( tx < area.x() )
                tx = area.x();
            if ( ty < area.y() )
                ty = area.y();
            move( tx, ty );
            }
        }

    XShapeSelectInput( qt_xdisplay(), window(), ShapeNotifyMask );
    if ( (is_shape = Shape::hasShape( window())) ) 
        {
        updateShape();
        }

    //CT extra check for stupid jdk 1.3.1. But should make sense in general
    // if client has initial state set to Iconic and is transient with a parent
    // window that is not Iconic, set init_state to Normal
    if( init_minimize && isTransient())
        {
        ClientList mainclients = mainClients();
        for( ClientList::ConstIterator it = mainclients.begin();
             it != mainclients.end();
             ++it )
            if( (*it)->isShown( true ))
                init_minimize = false; // SELI even e.g. for NET::Utility?
        }

    if( init_minimize )
        minimize();

    // SELI this seems to be mainly for kstart and ksystraycmd
    // probably should be replaced by something better
    bool doNotShow = false;
    if ( workspace()->isNotManaged( caption() ) )
        doNotShow = TRUE;

    // other settings from the previous session
    if ( session ) 
        {
        setKeepAbove( session->keepAbove );
        setKeepBelow( session->keepBelow );
        setSkipTaskbar( session->skipTaskbar, true );
        setSkipPager( session->skipPager );
        setShade( session->shaded ? ShadeNormal : ShadeNone );
        if( session->maximized != MaximizeRestore )
            {
            maximize( (MaximizeMode) session->maximized );
            geom_restore = session->restore;
            }
        if( session->fullscreen == FullScreenHack )
            ; // nothing, this should be already set again above
        else if( session->fullscreen != FullScreenNone )
            {
            setFullScreen( true, false );
            geom_fs_restore = session->fsrestore;
            }
        }
    else 
        {
        geom_restore = geometry(); // remember restore geometry
        if ( isMaximizable()
             && ( width() >= area.width() || height() >= area.height() ) ) 
            {
            // window is too large for the screen, maximize in the
            // directions necessary
            if ( width() >= area.width() && height() >= area.height() ) 
                {
                maximize( Client::MaximizeFull );
                geom_restore = QRect(); // use placement when unmaximizing
                }
            else if ( width() >= area.width() ) 
                {
                maximize( Client::MaximizeHorizontal );
                geom_restore = QRect(); // use placement when unmaximizing
                geom_restore.setY( y()); // but only for horizontal direction
                geom_restore.setHeight( height());
                }
            else if ( height() >= area.height() ) 
                {
                maximize( Client::MaximizeVertical );
                geom_restore = QRect(); // use placement when unmaximizing
                geom_restore.setX( x()); // but only for vertical direction
                geom_restore.setWidth( width());
                }
            }
        // window may want to be maximized
        // done after checking that the window isn't larger than the workarea, so that
        // the restore geometry from the checks above takes precedence, and window
        // isn't restored larger than the workarea
        if ( (info->state() & NET::Max) == NET::Max )
            maximize( Client::MaximizeFull );
        else if ( info->state() & NET::MaxVert )
            maximize( Client::MaximizeVertical );
        else if ( info->state() & NET::MaxHoriz )
            maximize( Client::MaximizeHorizontal );

        // read other initial states
        if( info->state() & NET::Shaded )
            setShade( ShadeNormal );
        if( info->state() & NET::KeepAbove )
            setKeepAbove( true );
        if( info->state() & NET::KeepBelow )
            setKeepBelow( true );
        if( info->state() & NET::SkipTaskbar )
            setSkipTaskbar( true, true );
        if( info->state() & NET::SkipPager )
            setSkipPager( true );
        if( info->state() & NET::DemandsAttention )
            demandAttention();
        if( info->state() & NET::Modal )
            setModal( true );
        if( fullscreen_mode != FullScreenHack )
            {
            if(( info->state() & NET::FullScreen ) != 0 && isFullScreenable())
                setFullScreen( true, false );
            }
        }

    updateAllowedActions( true );

    // TODO this should avoid flicker, because real restacking is done
    // only after manage() finishes, but the window is shown sooner
    // - keep it?
    XLowerWindow( qt_xdisplay(), frameId());

    user_time = readUserTimeMapTimestamp( asn_valid ? &asn_data : NULL, session );

    if( isTopMenu()) // they're shown in Workspace::addClient() if their mainwindow
        hideClient( true ); // is the active one

    if ( isShown( true ) && !doNotShow )
        {
        if( isDialog())
            Notify::raise( Notify::TransNew );
        if( isNormalWindow())
            Notify::raise( Notify::New );

        // if session saving, force showing new windows (i.e. "save file?" dialogs etc.)
        if( workspace()->sessionSaving() && !isOnCurrentDesktop())
            workspace()->setCurrentDesktop( desktop());

        if( isOnCurrentDesktop())
            {
            setMappingState( NormalState );

            if( isMapped )
                {
                workspace()->raiseClient( this );
                rawShow();
                }
            else
                {
                if( workspace()->allowClientActivation( this, user_time, false, session && session->active ))
                    {
                    workspace()->raiseClient( this );
                    rawShow();
                    if( !isSpecialWindow() || isOverride())
                        if ( options->focusPolicyIsReasonable() && wantsTabFocus() )
                            workspace()->requestFocus( this );
                    }
                else
                    {
                    workspace()->restackClientUnderActive( this );
                    rawShow();
                    if( ( !session || session->fake ) && ( !isSpecialWindow() || isOverride()))
                        demandAttention();
                    }
                }
            }
        else
            {
            virtualDesktopChange();
            workspace()->raiseClient( this );
            if( ( !session || session->fake ) && !isMapped )
                demandAttention();
            }
        }
    else if( !doNotShow ) // !isShown()
        {
        rawHide();
        setMappingState( IconicState );
        }
    else // doNotShow
        { // SELI HACK !!!
        hideClient( true );
        setMappingState( IconicState );
        }
    assert( mappingState() != WithdrawnState );

    if( user_time == CurrentTime || user_time == -1U ) // no known user time, set something old
        {
        user_time = qt_x_time - 1000000;
        if( user_time == CurrentTime || user_time == -1U ) // let's be paranoid
            user_time = qt_x_time - 1000000 + 10;
        }

    updateWorkareaDiffs();

//    sendSyntheticConfigureNotify(); done when setting mapping state

    delete session;

//    XUngrabServer( qt_xdisplay()); //FRAME

    return true;
    }

// called only from manage()
void Client::embedClient( Window w )
    {
    assert( client == None );
    assert( frame == None );
    assert( wrapper == None );
    client = w;
    // we don't want the window to be destroyed when we are destroyed
    XAddToSaveSet( qt_xdisplay(), client );
    XSelectInput( qt_xdisplay(), client, NoEventMask );
    XUnmapWindow( qt_xdisplay(), client );
    XWindowChanges wc;     // set the border width to 0
    wc.border_width = 0; // TODO possibly save this, and also use it for initial configuring of the window
    XConfigureWindow( qt_xdisplay(), client, CWBorderWidth, &wc );
    frame = XCreateSimpleWindow( qt_xdisplay(), qt_xrootwin(), 0, 0, 1, 1, 0, 0, 0 );
    wrapper = XCreateSimpleWindow( qt_xdisplay(), frame, 0, 0, 1, 1, 0, 0, 0 );
    XDefineCursor( qt_xdisplay(), frame, arrowCursor.handle());
    // some apps are stupid and don't define their own cursor - set the arrow one for them
    XDefineCursor( qt_xdisplay(), wrapper, arrowCursor.handle());
    XReparentWindow( qt_xdisplay(), client, wrapper, 0, 0 );
    XSelectInput( qt_xdisplay(), frame,
            KeyPressMask | KeyReleaseMask |
            ButtonPressMask | ButtonReleaseMask |
            KeymapStateMask |
            ButtonMotionMask |
            PointerMotionMask |
            EnterWindowMask | LeaveWindowMask |
            FocusChangeMask |
            ExposureMask |
            PropertyChangeMask |
            StructureNotifyMask | SubstructureRedirectMask |
            VisibilityChangeMask );
    XSelectInput( qt_xdisplay(), wrapper, ClientWinMask | SubstructureNotifyMask );
    XSelectInput( qt_xdisplay(), client,
                  FocusChangeMask |
                  PropertyChangeMask |
                  ColormapChangeMask |
                  EnterWindowMask | LeaveWindowMask |
                  KeyPressMask | KeyReleaseMask
                  );
    XSetWindowBackgroundPixmap( qt_xdisplay(), frameId(), None );
    XSetWindowBackgroundPixmap( qt_xdisplay(), wrapperId(), None );
    updateMouseGrab();
    }

} // namespace
