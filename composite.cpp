/*****************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2006 Lubos Lunak <l.lunak@kde.org>

You can Freely distribute this program under the GNU General Public
License. See the file "COPYING" for the exact licensing terms.
******************************************************************/

/*
 Code related to compositing (redirecting windows to pixmaps and tracking
 window damage).
 
 Docs:
 
 XComposite (the protocol, but the function calls map to it):
 http://gitweb.freedesktop.org/?p=xorg/proto/compositeproto.git;a=blob_plain;hb=HEAD;f=compositeproto.txt
 
 XDamage (again the protocol):
 http://gitweb.freedesktop.org/?p=xorg/proto/damageproto.git;a=blob_plain;hb=HEAD;f=damageproto.txt

 Composite HOWTO from Fredrik:
 http://ktown.kde.org/~fredrik/composite_howto.html

*/

#include "utils.h"
#include "workspace.h"
#include "client.h"
#include "unmanaged.h"
#include "deleted.h"
#include "effects.h"
#include "scene.h"
#include "scene_basic.h"
#include "scene_xrender.h"
#include "scene_opengl.h"

#include <stdio.h>

#include <QMenu>
#include <kxerrorhandler.h>

#include <X11/extensions/shape.h>

namespace KWinInternal
{

//****************************************
// Workspace
//****************************************

void Workspace::setupCompositing()
    {
#if defined( HAVE_XCOMPOSITE ) && defined( HAVE_XDAMAGE )
    if( !options->useTranslucency )
        return;
    if( !Extensions::compositeAvailable() || !Extensions::damageAvailable())
        return;
    if( scene != NULL )
        return;
    char selection_name[ 100 ];
    sprintf( selection_name, "_NET_WM_CM_S%d", DefaultScreen( display()));
    cm_selection = new KSelectionOwner( selection_name );
    connect( cm_selection, SIGNAL( lostOwnership()), SLOT( lostCMSelection()));
    cm_selection->claim( true ); // force claiming
    char type = 'O';
    if( getenv( "KWIN_COMPOSE" ))
        type = getenv( "KWIN_COMPOSE" )[ 0 ];
    switch( type )
        {
        case 'B':
            scene = new SceneBasic( this );
            kDebug( 1212 ) << "X compositing" << endl;
          break;
#ifdef HAVE_XRENDER
        case 'X':
            scene = new SceneXrender( this );
            kDebug( 1212 ) << "XRender compositing" << endl;
          break;
#endif
#ifdef HAVE_OPENGL
        case 'O':
            scene = new SceneOpenGL( this );
            kDebug( 1212 ) << "OpenGL compositing" << endl;
          break;
#endif
        default:
          kDebug( 1212 ) << "No compositing" << endl;
          return;
        }
    int rate = 0;
    if( options->refreshRate > 0 )
        { // use manually configured refresh rate
        rate = options->refreshRate;
        }
#ifdef HAVE_XRANDR
    else
        { // autoconfigure refresh rate based on XRandR info
        if( Extensions::randrAvailable() )
            {
            XRRScreenConfiguration *config;

            config = XRRGetScreenInfo( display(), rootWindow() );
            rate = XRRConfigCurrentRate( config );
            XRRFreeScreenConfigInfo( config );
            }
        }
#endif
    // 0Hz or less is invalid, so we fallback to a default rate
    if( rate <= 0 )
        rate = 50;
    // QTimer gives us 1msec (1000Hz) at best, so we ignore anything higher;
    // however, since compositing is limited to no more than once per 5msec,
    // 200Hz to 1000Hz are effectively identical
    else if( rate > 1000 )
        rate = 1000;
    kDebug( 1212 ) << "Refresh rate " << rate << "Hz" << endl;
    compositeRate = 1000 / rate;
    compositeTimer.start( compositeRate );
    lastCompositePaint.start();
    XCompositeRedirectSubwindows( display(), rootWindow(), CompositeRedirectManual );
    new EffectsHandler(); // sets also the 'effects' pointer
    addRepaintFull();
    foreach( Client* c, clients )
        c->setupCompositing();
    foreach( Client* c, desktops )
        c->setupCompositing();
    foreach( Unmanaged* c, unmanaged )
        c->setupCompositing();
    foreach( Client* c, clients )
        scene->windowAdded( c );
    foreach( Client* c, desktops )
        scene->windowAdded( c );
    foreach( Unmanaged* c, unmanaged )
        scene->windowAdded( c );
    delete popup; // force re-creation of the Alt+F3 popup (opacity option)
    popup = NULL;
#endif
    }

void Workspace::finishCompositing()
    {
#if defined( HAVE_XCOMPOSITE ) && defined( HAVE_XDAMAGE )
    if( scene == NULL )
        return;
    delete cm_selection;
    foreach( Client* c, clients )
        scene->windowClosed( c, NULL );
    foreach( Client* c, desktops )
        scene->windowClosed( c, NULL );
    foreach( Unmanaged* c, unmanaged )
        scene->windowClosed( c, NULL );
    foreach( Deleted* c, deleted )
        scene->windowDeleted( c );
    foreach( Client* c, clients )
        c->finishCompositing();
    foreach( Client* c, desktops )
        c->finishCompositing();
    foreach( Unmanaged* c, unmanaged )
        c->finishCompositing();
    foreach( Deleted* c, deleted )
        c->finishCompositing();
    XCompositeUnredirectSubwindows( display(), rootWindow(), CompositeRedirectManual );
    compositeTimer.stop();
    delete effects;
    effects = NULL;
    delete scene;
    scene = NULL;
    repaints_region = QRegion();
    for( ClientList::ConstIterator it = clients.begin();
         it != clients.end();
         ++it )
        { // forward all opacity values to the frame in case there'll be other CM running
        if( (*it)->opacity() != 1.0 )
            {
            NETWinInfo i( display(), (*it)->frameId(), rootWindow(), 0 );
            i.setOpacity( static_cast< unsigned long >((*it)->opacity() * 0xffffffff ));
            }
        }
    delete popup; // force re-creation of the Alt+F3 popup (opacity option)
    popup = NULL;
#endif
    }

void Workspace::lostCMSelection()
    {
    kDebug( 1212 ) << "Lost compositing manager selection" << endl;
    finishCompositing();
    }

void Workspace::addRepaint( int x, int y, int w, int h )
    {
    if( !compositing())
        return;
    repaints_region += QRegion( x, y, w, h );
    }

void Workspace::addRepaint( const QRect& r )
    {
    if( !compositing())
        return;
    repaints_region += r;
    }
    
void Workspace::addRepaintFull()
    {
    if( !compositing())
        return;
    repaints_region = QRegion( 0, 0, displayWidth(), displayHeight());
    }

void Workspace::performCompositing()
    {
#if defined( HAVE_XCOMPOSITE ) && defined( HAVE_XDAMAGE )
    // The event loop apparently tries to fire a QTimer as often as possible, even
    // at the expense of not processing many X events. This means that the composite
    // repaints can seriously impact performance of everything else, therefore throttle
    // them - leave at least 5msec time after one repaint is finished and next one
    // is started.
    if( lastCompositePaint.elapsed() < 5 )
        return;
    if( repaints_region.isEmpty() && !windowRepaintsPending()) // no damage
        {
        scene->idle();
        return;
        }
    // create a list of all windows in the stacking order
    // TODO keep this list like now a stacking order of Client window is kept
    ToplevelList windows;
    Window* children;
    unsigned int children_count;
    Window dummy;
    XQueryTree( display(), rootWindow(), &dummy, &dummy, &children, &children_count );
    for( unsigned int i = 0;
         i < children_count;
         ++i )
        {
        if( Client* c = findClient( FrameIdMatchPredicate( children[ i ] )))
            windows.append( c );
        else if( Unmanaged* c = findUnmanaged( WindowMatchPredicate( children[ i ] )))
            windows.append( c );
        }
    foreach( Deleted* c, deleted ) // TODO remember stacking order somehow
        windows.append( c );
    if( children != NULL )
        XFree( children );
    foreach( Toplevel* c, windows )
        { // this could be possibly optimized WRT obscuring, but that'd need being already
          // past prePaint() phase - probably not worth it
        repaints_region |= c->repaints().translated( c->pos());
        c->resetRepaints( c->rect());
        }
    QRegion repaints = repaints_region;
    // clear all repaints, so that post-pass can add repaints for the next repaint
    repaints_region = QRegion();
    scene->paint( repaints, windows );
    if( scene->waitSyncAvailable() && options->glVSync )
        { // if we're using vsync, then time the next paint pass to
          // before the next available sync
        int paintTime = ( lastCompositePaint.elapsed() % compositeRate ) +
                        ( compositeRate / 2 );
        if( paintTime >= compositeRate )
            compositeTimer.start( paintTime );
        else if( paintTime < compositeRate )
            compositeTimer.start( compositeRate - paintTime );
        }
    lastCompositePaint.start();
#endif
    }

bool Workspace::windowRepaintsPending() const
    {
    foreach( Toplevel* c, clients )
        if( !c->repaints().isEmpty())
            return true;
    foreach( Toplevel* c, desktops )
        if( !c->repaints().isEmpty())
            return true;
    foreach( Toplevel* c, unmanaged )
        if( !c->repaints().isEmpty())
            return true;
    foreach( Toplevel* c, deleted )
        if( !c->repaints().isEmpty())
            return true;
    return false;
    }

bool Workspace::createOverlay()
    {
    assert( overlay == None );
    if( !Extensions::compositeOverlayAvailable())
        return false;
#ifdef HAVE_XCOMPOSITE_OVERLAY
    overlay = XCompositeGetOverlayWindow( display(), rootWindow());
    if( overlay == None )
        return false;
    return true;
#else
    return false;
#endif
    }

void Workspace::setupOverlay( Window w )
    {
    assert( overlay != None );
    XShapeCombineRectangles( display(), overlay, ShapeInput, 0, 0, NULL, 0, ShapeSet, Unsorted );
    if( w != None )
        {
        XShapeCombineRectangles( display(), w, ShapeInput, 0, 0, NULL, 0, ShapeSet, Unsorted );
        XMapWindow( display(), w );
        }
    XMapRaised( display(), overlay );
    }

void Workspace::destroyOverlay()
    {
    if( overlay == None )
        return;
#ifdef HAVE_XCOMPOSITE_OVERLAY
    XCompositeReleaseOverlayWindow( display(), overlay );
#endif
    overlay = None;
    }

//****************************************
// Toplevel
//****************************************

void Toplevel::setupCompositing()
    {
#if defined( HAVE_XCOMPOSITE ) && defined( HAVE_XDAMAGE )
    if( !compositing())
        return;
    if( damage_handle != None )
        return;
    damage_handle = XDamageCreate( display(), frameId(), XDamageReportRawRectangles );
    damage_region = QRegion( 0, 0, width(), height());
    effect_window = new EffectWindow();
    effect_window->setWindow( this );
#endif
    }

void Toplevel::finishCompositing()
    {
#if defined( HAVE_XCOMPOSITE ) && defined( HAVE_XDAMAGE )
    if( damage_handle == None )
        return;
    if( effect_window->window() == this ) // otherwise it's already passed to Deleted, don't free data
        {
        discardWindowPixmap();
        delete effect_window;
        }
    XDamageDestroy( display(), damage_handle );
    damage_handle = None;
    damage_region = QRegion();
    repaints_region = QRegion();
    effect_window = NULL;
#endif
    }

void Toplevel::discardWindowPixmap()
    {
    addDamageFull();
    if( window_pix == None )
        return;
    XFreePixmap( display(), window_pix );
    window_pix = None;
    }

Pixmap Toplevel::createWindowPixmap()
    {
#ifdef HAVE_XCOMPOSITE
    assert( compositing());
    grabXServer();
    KXErrorHandler err;
    window_pix = XCompositeNameWindowPixmap( display(), frameId());
    // check that the received pixmap is valid and actually matches what we
    // know about the window (i.e. size)
    XWindowAttributes attrs;
    if( !XGetWindowAttributes( display(), frameId(), &attrs ))
        window_pix = None;
    if( err.error( false ))
        window_pix = None;
    if( attrs.width != width() || attrs.height != height() || attrs.map_state != IsViewable )
        window_pix = None;
    ungrabXServer();
    if( window_pix == None )
        kDebug( 1212 ) << "Creating window pixmap failed: " << this << endl;
    return window_pix;
#else
    return None;
#endif
    }

#ifdef HAVE_XDAMAGE
void Toplevel::damageNotifyEvent( XDamageNotifyEvent* e )
    {
    addDamage( e->area.x, e->area.y, e->area.width, e->area.height );
    // compress
    while( XPending( display()))
        {
        XEvent e2;
        if( XPeekEvent( display(), &e2 ) && e2.type == Extensions::damageNotifyEvent()
            && e2.xany.window == frameId())
            {
            XNextEvent( display(), &e2 );
            XDamageNotifyEvent* e = reinterpret_cast< XDamageNotifyEvent* >( &e2 );
            addDamage( e->area.x, e->area.y, e->area.width, e->area.height );
            continue;
            }
        break;
        }
    }
#endif

void Toplevel::addDamage( const QRect& r )
    {
    addDamage( r.x(), r.y(), r.width(), r.height());
    }

void Toplevel::addDamage( int x, int y, int w, int h )
    {
    if( !compositing())
        return;
    QRect r( x, y, w, h );
    // resizing the decoration may lag behind a bit and when shrinking there
    // may be a damage event coming with size larger than the current window size
    r &= rect();
    damage_region += r;
    repaints_region += r;
    }

void Toplevel::addDamageFull()
    {
    if( !compositing())
        return;
    damage_region = rect();
    repaints_region = rect();
    }

void Toplevel::resetDamage( const QRect& r )
    {
    damage_region -= r;
    }

void Toplevel::addRepaint( const QRect& r )
    {
    addRepaint( r.x(), r.y(), r.width(), r.height());
    }

void Toplevel::addRepaint( int x, int y, int w, int h )
    {
    if( !compositing())
        return;
    QRect r( x, y, w, h );
    r &= rect();
    repaints_region += r;
    }

void Toplevel::addRepaintFull()
    {
    repaints_region = rect();
    }

void Toplevel::resetRepaints( const QRect& r )
    {
    repaints_region -= r;
    }

} // namespace
