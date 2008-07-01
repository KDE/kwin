/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2006 Lubos Lunak <l.lunak@kde.org>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*********************************************************************/

/*
 Code related to compositing (redirecting windows to pixmaps and tracking
 window damage).
 
 Docs:
 
 XComposite (the protocol, but the function calls map to it):
 http://gitweb.freedesktop.org/?p=xorg/proto/compositeproto.git;a=blob_plain;hb=HEAD;f=compositeproto.txt
 
 XDamage (again the protocol):
 http://gitweb.freedesktop.org/?p=xorg/proto/damageproto.git;a=blob_plain;hb=HEAD;f=damageproto.txt

 Paper including basics on compositing, XGL vs AIGLX, XRender vs OpenGL, etc.:
 http://www.vis.uni-stuttgart.de/~hopf/pub/LinuxTag2007_compiz_NextGenerationDesktop_Paper.pdf
 
 Composite HOWTO from Fredrik:
 http://ktown.kde.org/~fredrik/composite_howto.html

*/

#include <config-X11.h>

#include "utils.h"
#include <QTextStream>
#include "workspace.h"
#include "client.h"
#include "unmanaged.h"
#include "deleted.h"
#include "effects.h"
#include "scene.h"
#include "scene_basic.h"
#include "scene_xrender.h"
#include "scene_opengl.h"
#include "compositingprefs.h"

#include <stdio.h>

#include <QMenu>
#include <kxerrorhandler.h>

#include <X11/extensions/shape.h>

#ifdef HAVE_XCOMPOSITE
#include <X11/extensions/Xcomposite.h>
#if XCOMPOSITE_MAJOR > 0 || XCOMPOSITE_MINOR >= 3
#define HAVE_XCOMPOSITE_OVERLAY
#endif
#endif
#ifdef HAVE_XRANDR
#include <X11/extensions/Xrandr.h>
#endif

namespace KWin
{

//****************************************
// Workspace
//****************************************

void Workspace::setupCompositing()
    {
#ifdef KWIN_HAVE_COMPOSITING
    if( scene != NULL )
        return;
    if( !options->useCompositing && getenv( "KWIN_COMPOSE") == NULL )
        {
        kDebug( 1212 ) << "Compositing is turned off in options";
        return;
        }
    else if( compositingSuspended )
        {
        kDebug( 1212 ) << "Compositing is suspended";
        return;
        }
    else if( !CompositingPrefs::compositingPossible() )
        {
        kError( 1212 ) << "Compositing is not possible";
        return;
        }
    CompositingType type = options->compositingMode;
    if( getenv( "KWIN_COMPOSE" ))
        {
        char c = getenv( "KWIN_COMPOSE" )[ 0 ];
        switch( c )
            {
            case 'O':
                type = OpenGLCompositing;
                break;
            case 'X':
                type = XRenderCompositing;
                break;
            default:
                kDebug( 1212 ) << "No compositing";
                return;
            }
        }

    char selection_name[ 100 ];
    sprintf( selection_name, "_NET_WM_CM_S%d", DefaultScreen( display()));
    cm_selection = new KSelectionOwner( selection_name );
    connect( cm_selection, SIGNAL( lostOwnership()), SLOT( lostCMSelection()));
    cm_selection->claim( true ); // force claiming

    switch( type )
        {
        /*case 'B':
            kDebug( 1212 ) << "X compositing";
            scene = new SceneBasic( this );
          break; // don't fall through (this is a testing one) */
#ifdef KWIN_HAVE_OPENGL_COMPOSITING
        case OpenGLCompositing:
            kDebug( 1212 ) << "OpenGL compositing";
            scene = new SceneOpenGL( this );
            if( !scene->initFailed())
                break; // -->
            delete scene;
            scene = NULL;
            break; // do not fall back to XRender for now, maybe in the future
#endif
#ifdef KWIN_HAVE_XRENDER_COMPOSITING
        case XRenderCompositing:
            kDebug( 1212 ) << "XRender compositing";
            scene = new SceneXrender( this );
          break;
#endif
        default:
#ifndef KWIN_HAVE_COMPOSITING
            kDebug( 1212 ) << "Compositing was not available at compile time";
#else
            kDebug( 1212 ) << "No compositing";
#endif
            delete cm_selection;
          return;
        }
    if( scene == NULL || scene->initFailed())
        {
        kError( 1212 ) << "Failed to initialize compositing, compositing disabled";
        kError( 1212 ) << "Consult http://techbase.kde.org/Projects/KWin/4.0-release-notes#Setting_up";
        delete scene;
        scene = NULL;
        delete cm_selection;
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
    // however, additional throttling makes prevents very high rates from taking place anyway
    else if( rate > 1000 )
        rate = 1000;
    kDebug( 1212 ) << "Refresh rate " << rate << "Hz";
    compositeRate = 1000 / rate;
    compositeTimer.start( compositeRate );
    lastCompositePaint.start();
    XCompositeRedirectSubwindows( display(), rootWindow(), CompositeRedirectManual );
    new EffectsHandlerImpl( scene->compositingType() ); // sets also the 'effects' pointer
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
#else
    kDebug( 1212 ) << "Compositing was not available at compile time";
#endif
    }

void Workspace::finishCompositing()
    {
#ifdef KWIN_HAVE_COMPOSITING
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
    kDebug( 1212 ) << "Lost compositing manager selection";
    finishCompositing();
    }

// for the shortcut
void Workspace::slotToggleCompositing()
    {
    compositingSuspended = !compositingSuspended;
    finishCompositing();
    setupCompositing(); // will do nothing if suspended
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
    
void Workspace::addRepaint( const QRegion& r )
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
#ifdef KWIN_HAVE_COMPOSITING
    // The event loop apparently tries to fire a QTimer as often as possible, even
    // at the expense of not processing many X events. This means that the composite
    // repaints can seriously impact performance of everything else, therefore throttle
    // them - leave at least 1msec time after one repaint is finished and next one
    // is started.
    if( lastCompositePaint.elapsed() < 1 )
        return;
    checkCursorPos();
    if(( repaints_region.isEmpty() && !windowRepaintsPending()) // no damage
        || !overlay_visible ) // nothing is visible anyway
        {
        scene->idle();
        return;
        }
    // create a list of all windows in the stacking order
    ToplevelList windows = rootStackingOrder();
    foreach( EffectWindow* c, static_cast< EffectsHandlerImpl* >( effects )->elevatedWindows())
        {
        Toplevel* t = static_cast< EffectWindowImpl* >( c )->window();
        windows.removeAll( t );
        windows.append( t );
        }
    // skip windows that are not yet ready for being painted
    ToplevelList tmp = windows;
    windows.clear();
#if 0
    // There is a bug somewhere that prevents this from working properly (#160393), but additionally
    // this cannot be used so carelessly - needs protections against broken clients, the window
    // should not get focus before it's displayed and so on.
    foreach( Toplevel* c, tmp )
        if( c->readyForPainting())
            windows.append( c );
#else
    foreach( Toplevel* c, tmp )
        windows.append( c );
#endif
    foreach( Toplevel* c, windows )
        { // This could be possibly optimized WRT obscuring, but that'd need being already
          // past prePaint() phase - probably not worth it.
          // TODO I think effects->transformWindowDamage() doesn't need to be called here,
          // pre-paint will extend painted window areas as necessary.
        repaints_region |= c->repaints().translated( c->pos());
        c->resetRepaints( c->rect());
        }
    QRegion repaints = repaints_region;
    // clear all repaints, so that post-pass can add repaints for the next repaint
    repaints_region = QRegion();
    scene->paint( repaints, windows );
    if( scene->waitSyncAvailable())
        {
        // if vsync is used, schedule the next repaint slightly in advance of the next sync,
        // so that there is still time for the drawing to take place
        int untilNextSync = compositeRate - ( lastCompositePaint.elapsed() % compositeRate );
        compositeTimer.start( qMax( 1, untilNextSync - 10 )); // 10 ms in advance - TODO maybe less?
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
    if( !Extensions::shapeInputAvailable()) // needed in setupOverlay()
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
    assert( Extensions::shapeInputAvailable());
    XShapeCombineRectangles( display(), overlay, ShapeInput, 0, 0, NULL, 0, ShapeSet, Unsorted );
    if( w != None )
        XShapeCombineRectangles( display(), w, ShapeInput, 0, 0, NULL, 0, ShapeSet, Unsorted );
    XSelectInput( display(), overlay, VisibilityChangeMask );
    }

void Workspace::showOverlay()
    {
    assert( overlay != None );
    if( overlay_shown )
        return;
    XMapSubwindows( display(), overlay );
    XMapWindow( display(), overlay );
    overlay_shown = true;
    }

void Workspace::destroyOverlay()
    {
    if( overlay == None )
        return;
#ifdef HAVE_XCOMPOSITE_OVERLAY
    XCompositeReleaseOverlayWindow( display(), overlay );
#endif
    overlay = None;
    overlay_shown = false;
    }

//****************************************
// Toplevel
//****************************************

void Toplevel::setupCompositing()
    {
#ifdef KWIN_HAVE_COMPOSITING
    if( !compositing())
        return;
    if( damage_handle != None )
        return;
    damage_handle = XDamageCreate( display(), frameId(), XDamageReportRawRectangles );
    damage_region = QRegion( 0, 0, width(), height());
    effect_window = new EffectWindowImpl();
    effect_window->setWindow( this );
#endif
    }

void Toplevel::finishCompositing()
    {
#ifdef KWIN_HAVE_COMPOSITING
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
    if( effectWindow() != NULL && effectWindow()->sceneWindow() != NULL )
        effectWindow()->sceneWindow()->pixmapDiscarded();
    }

Pixmap Toplevel::createWindowPixmap()
    {
#ifdef KWIN_HAVE_COMPOSITING
    assert( compositing());
    grabXServer();
    KXErrorHandler err;
    Pixmap pix = XCompositeNameWindowPixmap( display(), frameId());
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
    if( window_pix == None || pix == None )
        {
        kDebug( 1212 ) << "Creating window pixmap failed: " << this;
        if( pix != None )
            XFreePixmap( display(), pix );
        }
    window_pix = pix;
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

void Client::damageNotifyEvent( XDamageNotifyEvent* e )
    {
    Toplevel::damageNotifyEvent( e );
#ifdef HAVE_XSYNC
    if( sync_counter == None ) // cannot detect complete redraw, consider done now
        ready_for_painting = true;
#else
    ready_for_painting = true; // no sync at all, consider done now
#endif
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
    static_cast<EffectsHandlerImpl*>(effects)->windowDamaged( effectWindow(), r );
    }

void Toplevel::addDamageFull()
    {
    if( !compositing())
        return;
    damage_region = rect();
    repaints_region = rect();
    static_cast<EffectsHandlerImpl*>(effects)->windowDamaged( effectWindow(), rect());
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

void Toplevel::addWorkspaceRepaint( int x, int y, int w, int h )
    {
    addWorkspaceRepaint( QRect( x, y, w, h ));
    }

void Toplevel::addWorkspaceRepaint( const QRect& r2 )
    {
    if( !compositing())
        return;
    if( effectWindow() == NULL ) // TODO - this can happen during window destruction
        return workspace()->addRepaint( r2 );
    QRect r = effects->transformWindowDamage( effectWindow(), r2 );
    workspace()->addRepaint( r );
    }

} // namespace
