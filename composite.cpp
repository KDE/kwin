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
#include "notifications.h"

#include <stdio.h>

#include <QMenu>
#include <kaction.h>
#include <kactioncollection.h>
#include <klocale.h>
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
        kDebug( 1212 ) << "Compositing is turned off in options or disabled";
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
                kDebug( 1212 ) << "Compositing forced to OpenGL mode by environment variable";
                type = OpenGLCompositing;
                break;
            case 'X':
                kDebug( 1212 ) << "Compositing forced to XRender mode by environment variable";
                type = XRenderCompositing;
                break;
            case 'N':
                if( getenv( "KDE_FAILSAFE" ))
                    kDebug( 1212 ) << "Compositing disabled forcefully by KDE failsafe mode";
                else
                    kDebug( 1212 ) << "Compositing disabled forcefully by environment variable";
                return; // Return not break
            default:
                kDebug( 1212 ) << "Unknown KWIN_COMPOSE mode set, ignoring";
                break;
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
            XRRScreenConfiguration *config = XRRGetScreenInfo( display(), rootWindow() );
            rate = xrrRefreshRate = XRRConfigCurrentRate( config );
            XRRFreeScreenConfigInfo( config );
            }
        }
#endif
    // 0Hz or less is invalid, so we fallback to a default rate
    if( rate <= 0 )
        rate = 50;
    // QTimer gives us 1msec (1000Hz) at best, so we ignore anything higher;
    // however, additional throttling prevents very high rates from taking place anyway
    else if( rate > 1000 )
        rate = 1000;
    kDebug( 1212 ) << "Refresh rate " << rate << "Hz";
    compositeRate = 1000 / rate;
    lastCompositePaint.start();
    // fake a previous paint, so that the next starts right now
    nextPaintReference = QTime::currentTime().addMSecs( -compositeRate );
    compositeTimer.setSingleShot( true );
    checkCompositeTimer();
    composite_paint_times.clear();
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
    discardPopup(); // force re-creation of the Alt+F3 popup (opacity option)
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
    delete effects;
    effects = NULL;
    delete scene;
    scene = NULL;
    compositeTimer.stop();
    mousePollingTimer.stop();
    repaints_region = QRegion();
    for( ClientList::ConstIterator it = clients.constBegin();
         it != clients.constEnd();
         ++it )
        { // forward all opacity values to the frame in case there'll be other CM running
        if( (*it)->opacity() != 1.0 )
            {
            NETWinInfo2 i( display(), (*it)->frameId(), rootWindow(), 0 );
            i.setOpacity( static_cast< unsigned long >((*it)->opacity() * 0xffffffff ));
            }
        }
    discardPopup(); // force re-creation of the Alt+F3 popup (opacity option)
    // discard all Deleted windows (#152914)
    while( !deleted.isEmpty())
        deleted.first()->discard( Allowed );
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
    suspendCompositing( !compositingSuspended );
    }

// for the dbus call
void Workspace::toggleCompositing()
    {
    slotToggleCompositing();
    if( compositingSuspended )
        {
        // when disabled show a shortcut how the user can get back compositing
        QString shortcut, message;
        if( KAction* action = qobject_cast<KAction*>( keys->action("Suspend Compositing")))
            shortcut = action->globalShortcut().primary().toString(QKeySequence::NativeText);
        if( !shortcut.isEmpty() && options->useCompositing )
            {
            // display notification only if there is the shortcut
            message = i18n( "Compositing has been suspended by another application.<br/>"
                "You can resume using the '%1' shortcut.", shortcut );
            Notify::raise( Notify::CompositingSuspendedDbus, message );
            }
        }
    }

void Workspace::suspendCompositing()
    {
    suspendCompositing( true );
    }

void Workspace::suspendCompositing( bool suspend )
    {
    compositingSuspended = suspend;
    finishCompositing();
    setupCompositing(); // will do nothing if suspended
    emit compositingToggled( !compositingSuspended );
    }

void Workspace::resetCompositing()
    {
    if( compositing())
        {
        finishCompositing();
        QTimer::singleShot( 0, this, SLOT( setupCompositing()));
        }
    }

void Workspace::addRepaint( int x, int y, int w, int h )
    {
    if( !compositing())
        return;
    repaints_region += QRegion( x, y, w, h );
    checkCompositeTimer();
    }

void Workspace::addRepaint( const QRect& r )
    {
    if( !compositing())
        return;
    repaints_region += r;
    checkCompositeTimer();
    }
    
void Workspace::addRepaint( const QRegion& r )
    {
    if( !compositing())
        return;
    repaints_region += r;
    checkCompositeTimer();
    }
    
void Workspace::addRepaintFull()
    {
    if( !compositing())
        return;
    repaints_region = QRegion( 0, 0, displayWidth(), displayHeight());
    checkCompositeTimer();
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
        {
        compositeTimer.start( 1 );
        return;
        }
    if( !scene->waitSyncAvailable())
        nextPaintReference = QTime::currentTime();
    if((( repaints_region.isEmpty() && !windowRepaintsPending()) // no damage
        || !overlay_visible )) // nothing is visible anyway
        {
        scene->idle();
        // Note: It would seem here we should undo suspended unredirect, but when scenes need
        // it for some reason, e.g. transformations or translucency, the next pass that does not
        // need this anymore and paints normally will also reset the suspended unredirect.
        // Otherwise the window would not be painted normally anyway.
        return;
        }
    // create a list of all windows in the stacking order
    ToplevelList windows = xStackingOrder();
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
    // should not get focus before it's displayed, handle unredirected windows properly and so on.
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
    QTime t = QTime::currentTime();
    scene->paint( repaints, windows );
    if( scene->waitSyncAvailable())
        {
        // If vsync is used, schedule the next repaint slightly in advance of the next sync,
        // so that there is still time for the drawing to take place. We have just synced, and
        // nextPaintReference is time from which multiples of compositeRate should be added,
        // so set it 10ms back (meaning next paint will be in 'compositeRate - 10').
        // However, make sure the reserve is smaller than the composite rate.
        int reserve = compositeRate <= 10 ? compositeRate - 1 : 10;
        nextPaintReference = QTime::currentTime().addMSecs( -reserve );
        }
    // Trigger at least one more pass even if there would be nothing to paint, so that scene->idle()
    // is called the next time. If there would be nothing pending, it will not restart the timer and
    // checkCompositeTime() would restart it again somewhen later, called from functions that
    // would again add something pending.
    checkCompositeTimer();
    checkCompositePaintTime( t.elapsed());
    lastCompositePaint.start();
#endif
    }

void Workspace::performMousePoll()
    {
    checkCursorPos();
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

void Workspace::setCompositeTimer()
    {
    if( !compositing()) // should not really happen, but there may be e.g. some damage events still pending
        return;
    // The last paint set nextPaintReference as a reference time to which multiples of compositeRate
    // should be added for the next paint. qBound() for protection; system time can change without notice.
    compositeTimer.start( qBound( 0, nextPaintReference.msecsTo( QTime::currentTime() ), 250 ) % compositeRate );
    }

void Workspace::startMousePolling()
    {
    mousePollingTimer.start( 20 ); // 50Hz. TODO: How often do we really need to poll?
    }

void Workspace::stopMousePolling()
    {
    mousePollingTimer.stop();
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
    XResizeWindow(display(), overlay, displayWidth(), displayHeight());
    return true;
#else
    return false;
#endif
    }

void Workspace::checkCompositePaintTime( int msec )
    {
    if( options->disableCompositingChecks )
        return;
    // Sanity check. QTime uses the system clock so if the user changes the time or
    // timezone our timer will return undefined results. Ideally we would use a system
    // clock independent timer but I am uncertain if Qt provides a nice wrapper for
    // one or not. As it's unlikely for a single paint to take 15 seconds it seems
    // like a good upper bound.
    if( msec < 0 || msec > 15000 )
        return;
    composite_paint_times.prepend( msec );
    bool tooslow = false;
    // If last 3 paints were way too slow, disable and warn.
    // 1 second seems reasonable, it's not that difficult to get relatively high times
    // with high system load.
    const int MAX_LONG_PAINT = 1000;
    if( composite_paint_times.count() >= 3 && composite_paint_times[ 0 ] > MAX_LONG_PAINT
        && composite_paint_times[ 1 ] > MAX_LONG_PAINT && composite_paint_times[ 2 ] > MAX_LONG_PAINT )
        {
        kDebug( 1212 ) << "Too long paint times, suspending";
        tooslow = true;
        }
    // If last 15 seconds all paints (all of them) were quite slow, disable and warn too. Quite slow being 0,1s
    // should be reasonable, that's 10fps and having constant 10fps is bad.
    // This may possibly trigger also when activating an expensive effect, so this may need tweaking.
    const int MAX_SHORT_PAINT = 100;
    const int SHORT_TIME = 15000; // 15 sec
    int time = 0;
    foreach( int t, composite_paint_times )
        {
        if( t < MAX_SHORT_PAINT )
            break;
        time += t;
        if( time > SHORT_TIME ) // all paints in the given time were long
            {
            kDebug( 1212 ) << "Long paint times for long time, suspending";
            tooslow = true;
            break;
            }
        }
    if( composite_paint_times.count() > 1000 )
        composite_paint_times.removeLast();
    if( tooslow )
        {
        QTimer::singleShot( 0, this, SLOT( suspendCompositing()));
        QString shortcut, message;
        if( KAction* action = qobject_cast<KAction*>( keys->action("Suspend Compositing")))
            shortcut = action->globalShortcut().primary().toString(QKeySequence::NativeText);
        if (shortcut.isEmpty())
            message = i18n( "Compositing was too slow and has been suspended.\n"
                "You can disable functionality checks in advanced compositing settings." );
        else
            message = i18n( "Compositing was too slow and has been suspended.\n"
                "If this was only a temporary problem, you can resume using the '%1' shortcut.\n"
                "You can also disable functionality checks in advanced compositing settings.", shortcut );
        Notify::raise( Notify::CompositingSlow, message );
        compositeTimer.start( 1000 ); // so that it doesn't trigger sooner than suspendCompositing()
        }
    }

void Workspace::setupOverlay( Window w )
    {
    assert( overlay != None );
    assert( Extensions::shapeInputAvailable());
    XSetWindowBackgroundPixmap( display(), overlay, None );
    overlay_shape = QRegion();
    setOverlayShape( QRect( 0, 0, displayWidth(), displayHeight()));
    if( w != None )
        {
        XSetWindowBackgroundPixmap( display(), w, None );
        XShapeCombineRectangles( display(), w, ShapeInput, 0, 0, NULL, 0, ShapeSet, Unsorted );
        }
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

void Workspace::hideOverlay()
    {
    assert( overlay != None );
    XUnmapWindow( display(), overlay );
    overlay_shown = false;
    setOverlayShape( QRect( 0, 0, displayWidth(), displayHeight()));
    }

void Workspace::setOverlayShape( const QRegion& reg )
    {
    // Avoid setting the same shape again, it causes flicker (apparently it is not a no-op
    // and triggers something).
    if( reg == overlay_shape )
        return;
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
    XShapeCombineRectangles( display(), overlay, ShapeBounding, 0, 0,
        xrects, rects.count(), ShapeSet, Unsorted );
    delete[] xrects;
    XShapeCombineRectangles( display(), overlay, ShapeInput, 0, 0, NULL, 0, ShapeSet, Unsorted );
    overlay_shape = reg;
    }

void Workspace::destroyOverlay()
    {
    if( overlay == None )
        return;
    // reset the overlay shape
    XRectangle rec = { 0, 0, displayWidth(), displayHeight() };
    XShapeCombineRectangles( display(), overlay, ShapeBounding, 0, 0, &rec, 1, ShapeSet, Unsorted );
    XShapeCombineRectangles( display(), overlay, ShapeInput, 0, 0, &rec, 1, ShapeSet, Unsorted );
#ifdef HAVE_XCOMPOSITE_OVERLAY
    XCompositeReleaseOverlayWindow( display(), overlay );
#endif
    overlay = None;
    overlay_shown = false;
    }

bool Workspace::compositingActive()
    {
    return compositing();
    }

// force is needed when the list of windows changes (e.g. a window goes away)
void Workspace::checkUnredirect( bool force )
    {
    if( !compositing() || overlay == None || !options->unredirectFullscreen )
        return;
    if( force )
        forceUnredirectCheck = true;
    if( !unredirectTimer.isActive())
        unredirectTimer.start( 0 );
    }

void Workspace::delayedCheckUnredirect()
    {
    if( !compositing() || overlay == None || !options->unredirectFullscreen )
        return;
    ToplevelList list;
    bool changed = forceUnredirectCheck;
    foreach( Client* c, clients )
        list.append( c );
    foreach( Unmanaged* c, unmanaged )
        list.append( c );
    foreach( Toplevel* c, list )
        {
        if( c->updateUnredirectedState())
            changed = true;
        }
    // no desktops, no Deleted ones
    if( !changed )
        return;
    forceUnredirectCheck = false;
    // Cut out parts from the overlay window where unredirected windows are,
    // so that they are actually visible.
    QRegion reg( 0, 0, displayWidth(), displayHeight());
    foreach( Toplevel* c, list )
        {
        if( c->unredirected())
            reg -= c->geometry();
        }
    setOverlayShape( reg );
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
    unredirect = false;
    workspace()->checkUnredirect( true );
#endif
    }

void Toplevel::finishCompositing()
    {
#ifdef KWIN_HAVE_COMPOSITING
    if( damage_handle == None )
        return;
    workspace()->checkUnredirect( true );
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
    if( unredirected())
        return None;
    grabXServer();
    KXErrorHandler err;
    Pixmap pix = XCompositeNameWindowPixmap( display(), frameId());
    // check that the received pixmap is valid and actually matches what we
    // know about the window (i.e. size)
    XWindowAttributes attrs;
    if( !XGetWindowAttributes( display(), frameId(), &attrs )
        || err.error( false )
        || attrs.width != width() || attrs.height != height() || attrs.map_state != IsViewable )
        {
        kDebug( 1212 ) << "Creating window pixmap failed: " << this;
        XFreePixmap( display(), pix );
        pix = None;
        }
    ungrabXServer();
    return pix;
#else
    return None;
#endif
    }

#ifdef HAVE_XDAMAGE
void Toplevel::damageNotifyEvent( XDamageNotifyEvent* e )
    {
    QRegion damage( e->area.x, e->area.y, e->area.width, e->area.height );
    // compress
    int cnt = 1;
    while( XPending( display()))
        {
        XEvent e2;
        if( XPeekEvent( display(), &e2 ) && e2.type == Extensions::damageNotifyEvent()
            && e2.xany.window == frameId())
            {
            XNextEvent( display(), &e2 );
            if( cnt > 200 )
                {
                // If there are way too many damage events in the queue, just discard them
                // and damage the whole window. Otherwise the X server can just overload
                // us with a flood of damage events. Should be probably optimized
                // in the X server, as this is rather lame.
                damage = rect();
                continue;
                }
            XDamageNotifyEvent* e = reinterpret_cast< XDamageNotifyEvent* >( &e2 );
            QRect r( e->area.x, e->area.y, e->area.width, e->area.height );
            ++cnt;
            // If there are too many damaged rectangles, increase them
            // to be multiples of 100x100 px grid, since QRegion get quite
            // slow with many rectangles, and there is little to gain by using
            // many small rectangles (rather the opposite, several large should
            // be often faster).
            if( cnt > 50 )
                {
                r.setLeft( r.left() / 100 * 100 );
                r.setRight(( r.right() + 99 ) / 100 * 100 );
                r.setTop( r.top() / 100 * 100 );
                r.setBottom(( r.bottom() + 99 ) / 100 * 100 );
                }
            damage += r;
            continue;
            }
        break;
        }
    foreach( const QRect& r, damage.rects())
        addDamage( r ); 
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
    workspace()->checkCompositeTimer();
    }

void Toplevel::addDamageFull()
    {
    if( !compositing())
        return;
    damage_region = rect();
    repaints_region = rect();
    static_cast<EffectsHandlerImpl*>(effects)->windowDamaged( effectWindow(), rect());
    workspace()->checkCompositeTimer();
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
    workspace()->checkCompositeTimer();
    }

void Toplevel::addRepaintFull()
    {
    repaints_region = rect();
    workspace()->checkCompositeTimer();
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

bool Toplevel::updateUnredirectedState()
    {
    assert( compositing());
    bool should = shouldUnredirect() && !unredirectSuspend && !shape() && !hasAlpha() && opacity() == 1.0 &&
        !static_cast<EffectsHandlerImpl*>( effects )->activeFullScreenEffect();
    if( should && !unredirect )
        {
        unredirect = true;
        kDebug( 1212 ) << "Unredirecting:" << this;
#ifdef HAVE_XCOMPOSITE
        XCompositeUnredirectWindow( display(), frameId(), CompositeRedirectManual );
#endif
        return true;
        }
    else if( !should && unredirect )
        {
        unredirect = false;
        kDebug( 1212 ) << "Redirecting:" << this;
#ifdef HAVE_XCOMPOSITE
        XCompositeRedirectWindow( display(), frameId(), CompositeRedirectManual );
#endif
        discardWindowPixmap();
        return true;
        }
    return false;
    }

void Toplevel::suspendUnredirect( bool suspend )
    {
    if( unredirectSuspend == suspend )
        return;
    unredirectSuspend = suspend;
    workspace()->checkUnredirect();
    }

//****************************************
// Client
//****************************************

void Client::setupCompositing()
    {
    Toplevel::setupCompositing();
    updateVisibility(); // for internalKeep()
    }

void Client::finishCompositing()
    {
    Toplevel::finishCompositing();
    updateVisibility();
    triggerDecorationRepaint();
    }

bool Client::shouldUnredirect() const
    {
    if( isActiveFullScreen())
        {
        ToplevelList stacking = workspace()->xStackingOrder();
        for( int pos = stacking.count() - 1;
             pos >= 0;
             --pos )
            {
            Toplevel* c = stacking.at( pos );
            if( c == this ) // is not covered by any other window, ok to unredirect
                return true;
            if( c->geometry().intersects( geometry()))
                return false;
            }
        abort();
        }
    return false;
    }

//****************************************
// Unmanaged
//****************************************

bool Unmanaged::shouldUnredirect() const
    {
    // the pixmap is needed for the login effect, a nicer solution would be the login effect increasing
    // refcount for the window pixmap (which would prevent unredirect), avoiding this hack
    if( resourceClass() == "ksplashx" || resourceClass() == "ksplashsimple" )
        return false;
// it must cover whole display or one xinerama screen, and be the topmost there
    if( geometry() == workspace()->clientArea( FullArea, geometry().center(), workspace()->currentDesktop())
        || geometry() == workspace()->clientArea( ScreenArea, geometry().center(), workspace()->currentDesktop()))
        {
        ToplevelList stacking = workspace()->xStackingOrder();
        for( int pos = stacking.count() - 1;
             pos >= 0;
             --pos )
            {
            Toplevel* c = stacking.at( pos );
            if( c == this ) // is not covered by any other window, ok to unredirect
                return true;
            if( c->geometry().intersects( geometry()))
                return false;
            }
        abort();
        }
    return false;
    }

//****************************************
// Deleted
//****************************************

bool Deleted::shouldUnredirect() const
    {
    return false;
    }

void Deleted::addRepaintFull()
    {
    repaints_region = decorationRect();
    workspace()->checkCompositeTimer();
    }

} // namespace
