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
#include <kglobal.h>
#include <X11/extensions/shape.h>

#include "notifications.h"
#include "rules.h"

extern Time qt_x_time;

namespace KWinInternal
{

/*!
  Manages the clients. This means handling the very first maprequest:
  reparenting, initial geometry, initial state, placement, etc.
  Returns false if KWin is not going to manage this window.
 */
bool Client::manage( Window w, bool isMapped )
    {
    XWindowAttributes attr;
    if( !XGetWindowAttributes(qt_xdisplay(), w, &attr))
        return false;

    grabXServer();

    // from this place on, manage() mustn't return false
    block_geometry = 1;

    embedClient( w, attr );

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
        NET::WM2ExtendedStrut |
        0;

    info = new WinInfo( this, qt_xdisplay(), client, qt_xrootwin(), properties, 2 );

    cmap = attr.colormap;

    XClassHint classHint;
    if ( XGetClassHint( qt_xdisplay(), client, &classHint ) ) 
        {
        // Qt3.2 and older had this all lowercase, Qt3.3 capitalized resource class
        // force lowercase, so that workarounds listing resource classes still work
        resource_name = QCString( classHint.res_name ).lower();
        resource_class = QCString( classHint.res_class ).lower();
        XFree( classHint.res_name );
        XFree( classHint.res_class );
        }
    ignore_focus_stealing = options->checkIgnoreFocusStealing( this ); // TODO change to rules

    window_role = staticWindowRole( w );
    getWmClientLeader();
    getWmClientMachine();
    // first only read the caption text, so that setupWindowRules() can use it for matching,
    // and only then really set the caption using setCaption(), which checks for duplicates etc.
    // and also relies on rules already existing
    cap_normal = readName();
    setupWindowRules( false );
    setCaption( cap_normal, true );

    detectNoBorder();
    fetchIconicName();
    getWMHints(); // needs to be done before readTransient() because of reading the group
    modal = ( info->state() & NET::Modal ) != 0; // needs to be valid before handling groups
    readTransient();
    getIcons();
    getWindowProtocols();
    getWmNormalHints(); // get xSizeHint
    getMotifHints();

    // TODO try to obey all state information from info->state()

    original_skip_taskbar = skip_taskbar = ( info->state() & NET::SkipTaskbar) != 0;
    skip_pager = ( info->state() & NET::SkipPager) != 0;

    KStartupInfoId asn_id;
    KStartupInfoData asn_data;
    bool asn_valid = workspace()->checkStartupNotification( window(), asn_id, asn_data );

    workspace()->updateClientLayer( this );

    SessionInfo* session = workspace()->takeSessionInfo( this );
    
    if ( session )
        {
        if ( session->minimized )
            init_minimize = true;
        if( session->userNoBorder )
            setUserNoBorder( true );
        }

    setShortcut( rules()->checkShortcut( session ? session->shortcut : QString::null, true ));

    init_minimize = rules()->checkMinimize( init_minimize, !isMapped );
    if( rules()->checkNoBorder( false, !isMapped ))
        setUserNoBorder( true );
    
    checkAndSetInitialRuledOpacity();

    // initial desktop placement
    if ( session ) 
        {
        desk = session->desktop;
        if( session->onAllDesktops )
            desk = NET::OnAllDesktops;
        }
    else
        {
        // if this window is transient, ensure that it is opened on the
        // same window as its parent.  this is necessary when an application
        // starts up on a different desktop than is currently displayed
        if( isTransient())
            {
            ClientList mainclients = mainClients();
            bool on_current = false;
            Client* maincl = NULL;
            // this is slightly duplicated from Placement::placeOnMainWindow()
            for( ClientList::ConstIterator it = mainclients.begin();
                 it != mainclients.end();
                 ++it )
                {
                if( (*it)->isSpecialWindow())
                    continue; // don't consider toolbars etc when placing
                maincl = *it;
                if( (*it)->isOnCurrentDesktop())
                    on_current = true;
                }
            if( on_current )
                desk = workspace()->currentDesktop();
            else if( maincl != NULL )
                desk = maincl->desktop();
            }
        if ( info->desktop() )
            desk = info->desktop(); // window had the initial desktop property, force it
        if( desktop() == 0 && asn_valid && asn_data.desktop() != 0 )
            desk = asn_data.desktop();
        }
    if ( desk == 0 ) // assume window wants to be visible on the current desktop
        desk = workspace()->currentDesktop();
    desk = rules()->checkDesktop( desk, !isMapped );
    if( desk != NET::OnAllDesktops ) // do range check
        desk = KMAX( 1, KMIN( workspace()->numberOfDesktops(), desk ));
    info->setDesktop( desk );
    workspace()->updateOnAllDesktopsOfTransients( this ); // SELI
//    onAllDesktopsChange(); decoration doesn't exist here yet

    QRect geom( attr.x, attr.y, attr.width, attr.height );
    bool placementDone = FALSE;

    if ( session )
        geom = session->geometry;

    QRect area;
    bool partial_keep_in_area = isMapped || session;
    if( isMapped || session )
        area = workspace()->clientArea( FullArea, geom.center(), desktop());
    else if( options->xineramaPlacementEnabled )
        area = workspace()->clientArea( PlacementArea, QCursor::pos(), desktop());
    else
        area = workspace()->clientArea( PlacementArea, geom.center(), desktop());

    if( checkFullScreenHack( geom ))
        {
        fullscreen_mode = FullScreenHack;
        geom = workspace()->clientArea( FullScreenArea, geom.center(), desktop());
        placementDone = true;
        }

    if ( isDesktop() ) 
        {
        // desktops are treated slightly special
        geom = workspace()->clientArea( FullArea, geom.center(), desktop());
        placementDone = true;
        }

    bool usePosition = false;
    if ( isMapped || session || placementDone )
        placementDone = true; // use geometry
    else if( isTransient() && !isUtility() && !isDialog() && !isSplash())
        usePosition = true;
    else if( isTransient() && !hasNETSupport())
        usePosition = true;
    else if( isDialog() && hasNETSupport())
    // if the dialog is actually non-NETWM transient window, don't try to apply placement to it,
    // it breaks with too many things (xmms, display)
        {
        if( mainClients().count() >= 1 )
            {
#if 1
            // TODO #78082 - Ok, it seems there are after all some cases when an application has a good
            // reason to specify a position for its dialog. Too bad other WMs have never bothered
            // with placement for dialogs, so apps always specify positions for their dialogs,
            // including such silly positions like always centered on the screen or under mouse.
            // Using ignoring requested position in window-specific settings helps, but at least
            // for Qt apps this should work better.
            usePosition = true;
#else
            ; // force using placement policy
#endif
            }
        else
            usePosition = true;
        }
    else if( isSplash())
        ; // force using placement policy
    else
        usePosition = true;
    if( !rules()->checkIgnorePosition( !usePosition ))
        {
        bool ignorePPosition = ( options->ignorePositionClasses.contains(QString::fromLatin1(resourceClass())));

        if ( ( (xSizeHint.flags & PPosition) && !ignorePPosition ) ||
             (xSizeHint.flags & USPosition) ) 
            {
            placementDone = TRUE;
            // disobey xinerama placement option for now (#70943)
            area = workspace()->clientArea( PlacementArea, geom.center(), desktop());
            }
        }
    if( true ) // size is always obeyed for now, only with constraints applied
        if ( (xSizeHint.flags & USSize) || (xSizeHint.flags & PSize) ) 
            {
            // keep in mind that we now actually have a size :-)
            }

    if (xSizeHint.flags & PMaxSize)
        geom.setSize( geom.size().boundedTo(
            rules()->checkMaxSize( QSize(xSizeHint.max_width, xSizeHint.max_height ) ) ) );
    if (xSizeHint.flags & PMinSize)
        geom.setSize( geom.size().expandedTo(
            rules()->checkMinSize( QSize(xSizeHint.min_width, xSizeHint.min_height ) ) ) );

    if( isMovable())
        {
        if( geom.x() > area.right() || geom.y() > area.bottom())
            placementDone = FALSE; // weird, do not trust.
        }

    if ( placementDone )
        move( geom.x(), geom.y() ); // before gravitating

    updateDecoration( false ); // also gravitates
    // TODO is CentralGravity right here, when resizing is done after gravitating?
    plainResize( rules()->checkSize( sizeForClientSize( geom.size()), !isMapped ));

    QPoint forced_pos = rules()->checkPosition( invalidPoint, !isMapped );
    if( forced_pos != invalidPoint )
        {
        move( forced_pos );
        placementDone = true;
        // don't keep inside workarea if the window has specially configured position
        partial_keep_in_area = true;
        area = workspace()->clientArea( FullArea, geom.center(), desktop());
        }
    if( !placementDone ) 
        { // placement needs to be after setting size
        workspace()->place( this, area );
        placementDone = TRUE;
        }

    if(( !isSpecialWindow() || isToolbar()) && isMovable())
        keepInArea( area, partial_keep_in_area );

    XShapeSelectInput( qt_xdisplay(), window(), ShapeNotifyMask );
    if ( (is_shape = Shape::hasShape( window())) ) 
        {
        updateShape();
        }
//    else
//	setShapable(FALSE);
	
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
        minimize( true ); // no animation

    // SELI this seems to be mainly for kstart and ksystraycmd
    // probably should be replaced by something better
    bool doNotShow = false;
    if ( workspace()->isNotManaged( caption() ) )
        doNotShow = TRUE;

    // other settings from the previous session
    if ( session ) 
        {
        // session restored windows are not considered to be new windows WRT rules,
        // i.e. obey only forcing rules
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
        MaximizeMode maxmode = static_cast< MaximizeMode >
            ((( info->state() & NET::MaxVert ) ? MaximizeVertical : 0 )
            | (( info->state() & NET::MaxHoriz ) ? MaximizeHorizontal : 0 ));
        MaximizeMode forced_maxmode = rules()->checkMaximize( maxmode, !isMapped );
        // either hints were set to maximize, or is forced to maximize,
        // or is forced to non-maximize and hints were set to maximize
        if( forced_maxmode != MaximizeRestore || maxmode != MaximizeRestore )
            maximize( forced_maxmode );

        // read other initial states
        setShade( rules()->checkShade( info->state() & NET::Shaded ? ShadeNormal : ShadeNone, !isMapped ));
        setKeepAbove( rules()->checkKeepAbove( info->state() & NET::KeepAbove, !isMapped ));
        setKeepBelow( rules()->checkKeepBelow( info->state() & NET::KeepBelow, !isMapped ));
        setSkipTaskbar( rules()->checkSkipTaskbar( info->state() & NET::SkipTaskbar, !isMapped ), true );
        setSkipPager( rules()->checkSkipPager( info->state() & NET::SkipPager, !isMapped ));
        if( info->state() & NET::DemandsAttention )
            demandAttention();
        if( info->state() & NET::Modal )
            setModal( true );
        if( fullscreen_mode != FullScreenHack && isFullScreenable())
            setFullScreen( rules()->checkFullScreen( info->state() & NET::FullScreen, !isMapped ), false );
        }

    updateAllowedActions( true );

    // TODO this should avoid flicker, because real restacking is done
    // only after manage() finishes, but the window is shown sooner
    // - keep it?
    XLowerWindow( qt_xdisplay(), frameId());

    user_time = readUserTimeMapTimestamp( asn_valid ? &asn_id : NULL, asn_valid ? &asn_data : NULL, session );

    if( isTopMenu()) // they're shown in Workspace::addClient() if their mainwindow
        hideClient( true ); // is the active one

    if( isShown( true ) && !doNotShow )
        {
        if( isDialog())
            Notify::raise( Notify::TransNew );
        if( isNormalWindow())
            Notify::raise( Notify::New );

        bool allow;
        if( session )
            allow = session->active && !workspace()->wasUserInteraction();
        else
            allow = workspace()->allowClientActivation( this, userTime(), false );

        // if session saving, force showing new windows (i.e. "save file?" dialogs etc.)
        // also force if activation is allowed
        if( !isOnCurrentDesktop() && !isMapped && !session && ( allow || workspace()->sessionSaving()))
            workspace()->setCurrentDesktop( desktop());

        if( workspace()->showingDesktop())
            workspace()->resetShowingDesktop( false );

        if( isOnCurrentDesktop() && !isMapped && !allow )
            workspace()->restackClientUnderActive( this );
        else
            workspace()->raiseClient( this );

        updateVisibility();

        if( !isMapped )
            {
            if( allow && isOnCurrentDesktop())
                {
                if( !isSpecialWindow())
                    if ( options->focusPolicyIsReasonable() && wantsTabFocus() )
                        workspace()->requestFocus( this );
                }
            else
                {
                if( !session && !isSpecialWindow())
                        demandAttention();
                }
            }
        }
    else if( !doNotShow ) // if( !isShown( true ) && !doNotShow )
        {
        updateVisibility();
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

    ungrabXServer();
    
    client_rules.discardTemporary();
    updateWindowRules(); // was blocked while !isManaged()

// TODO there's a small problem here - isManaged() depends on the mapping state,
// but this client is not yet in Workspace's client list at this point, will
// be only done in addClient()
    return true;
    }

// called only from manage()
void Client::embedClient( Window w, const XWindowAttributes &attr )
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

    XSetWindowAttributes swa;
    swa.colormap = attr.colormap;
    swa.background_pixmap = None;
    swa.border_pixel = 0;

    frame = XCreateWindow( qt_xdisplay(), qt_xrootwin(), 0, 0, 1, 1, 0,
		    attr.depth, InputOutput, attr.visual,
		    CWColormap | CWBackPixmap | CWBorderPixel, &swa );
    wrapper = XCreateWindow( qt_xdisplay(), frame, 0, 0, 1, 1, 0,
		    attr.depth, InputOutput, attr.visual,
		    CWColormap | CWBackPixmap | CWBorderPixel, &swa );

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
    updateMouseGrab();
    }

} // namespace
