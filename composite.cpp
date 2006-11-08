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
 http://gitweb.freedesktop.org/?p=xorg/proto/compositeproto.git;a=blob;hb=HEAD;f=protocol
 
 XDamage (again the protocol):
 http://gitweb.freedesktop.org/?p=xorg/proto/damageproto.git;a=blob;hb=HEAD;f=protocol

 Composite HOWTO from Fredrik:
 http://ktown.kde.org/~fredrik/composite_howto.html

*/

#include "utils.h"
#include "workspace.h"
#include "client.h"
#include "unmanaged.h"
#include "effects.h"
#include "scene.h"
#include "scene_basic.h"
#include "scene_xrender.h"
#include "scene_opengl.h"

#include <X11/extensions/shape.h>

namespace KWinInternal
{

//****************************************
// Workspace
//****************************************

#if defined( HAVE_XCOMPOSITE ) && defined( HAVE_XDAMAGE )
void Workspace::setupCompositing()
    {
    if( !options->useTranslucency )
        return;
    if( !Extensions::compositeAvailable() || !Extensions::damageAvailable())
        return;
    if( scene != NULL )
        return;
    char type = 'O';
    if( getenv( "KWIN_COMPOSE" ))
        type = getenv( "KWIN_COMPOSE" )[ 0 ];
    switch( type )
        {
        case 'B':
            scene = new SceneBasic( this );
          break;
        case 'X':
            scene = new SceneXrender( this );
          break;
        case 'O':
            scene = new SceneOpenGL( this );
          break;
        default:
          kDebug() << "No compositing" << endl;
          return;
        }
    compositeTimer.start( 20 );
    lastCompositePaint.start();
    XCompositeRedirectSubwindows( display(), rootWindow(), CompositeRedirectManual );
    if( dynamic_cast< SceneOpenGL* >( scene ))
        kDebug() << "OpenGL compositing" << endl;
    else if( dynamic_cast< SceneXrender* >( scene ))
        kDebug() << "XRender compositing" << endl;
    else if( dynamic_cast< SceneBasic* >( scene ))
        kDebug() << "X compositing" << endl;
    effects = new EffectsHandler( this );
    addDamageFull();
    foreach( Client* c, clients )
        c->setupCompositing();
    foreach( Unmanaged* c, unmanaged )
        c->setupCompositing();
    foreach( Client* c, clients )
        scene->windowAdded( c );
    foreach( Unmanaged* c, unmanaged )
        scene->windowAdded( c );
    delete popup; // force re-creation of the Alt+F3 popup (opacity option)
    popup = NULL;
    }

void Workspace::finishCompositing()
    {
    if( scene == NULL )
        return;
    foreach( Client* c, clients )
        scene->windowDeleted( c );
    foreach( Unmanaged* c, unmanaged )
        scene->windowDeleted( c );
    foreach( Client* c, clients )
        c->finishCompositing();
    foreach( Unmanaged* c, unmanaged )
        c->finishCompositing();
    XCompositeUnredirectSubwindows( display(), rootWindow(), CompositeRedirectManual );
    compositeTimer.stop();
    delete effects;
    effects = NULL;
    delete scene;
    scene = NULL;
    damage_region = QRegion();
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
    }

void Workspace::addDamage( int x, int y, int w, int h )
    {
    if( !compositing())
        return;
    damage_region += QRegion( x, y, w, h );
    }

void Workspace::addDamage( const QRect& r )
    {
    if( !compositing())
        return;
    damage_region += r;
    }
    
void Workspace::addDamageFull()
    {
    if( !compositing())
        return;
    damage_region = QRegion( 0, 0, displayWidth(), displayHeight());
    }

void Workspace::performCompositing()
    {
    // The event loop apparently tries to fire a QTimer as often as possible, even
    // at the expense of not processing many X events. This means that the composite
    // repaints can seriously impact performance of everything else, therefore throttle
    // them - leave at least 5msec time after one repaint is finished and next one
    // is started.
    if( lastCompositePaint.elapsed() < 5 )
        return;
    if( damage_region.isEmpty()) // no damage
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
            {
            if( c->isShown( true ) && c->isOnCurrentDesktop())
                windows.append( c );
            }
        else if( Unmanaged* c = findUnmanaged( HandleMatchPredicate( children[ i ] )))
            windows.append( c );
        }
    scene->initPaint();
    scene->paint( damage_region, windows );
    // clear all damage
    damage_region = QRegion();
    foreach( Toplevel* c, windows )
        c->resetDamage();
    // run post-pass only after clearing the damage
    scene->postPaint();
    lastCompositePaint.start();
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
    XShapeCombineRectangles( display(), w, ShapeInput, 0, 0, NULL, 0, ShapeSet, Unsorted );
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
    if( !compositing())
        return;
    if( damage_handle != None )
        return;
    damage_handle = XDamageCreate( display(), handle(), XDamageReportRawRectangles );
    damage_region = QRegion( 0, 0, width(), height());
    }

void Toplevel::finishCompositing()
    {
    if( damage_handle == None )
        return;
    XDamageDestroy( display(), damage_handle );
    damage_handle = None;
    damage_region = QRegion();
    }

Pixmap Toplevel::createWindowPixmap() const
    {
    assert( compositing());
    return XCompositeNameWindowPixmap( display(), handle());
    }

void Toplevel::damageNotifyEvent( XDamageNotifyEvent* e )
    {
    addDamage( e->area.x, e->area.y, e->area.width, e->area.height );
    }

void Toplevel::addDamage( const QRect& r )
    {
    addDamage( r.x(), r.y(), r.width(), r.height());
    }

void Toplevel::addDamage( int x, int y, int w, int h )
    {
    if( !compositing())
        return;
    QRect r( x, y, w, h );
    damage_region += r;
    r.translate( this->x(), this->y());
    // this could be possibly optimized to damage Workspace only if the toplevel
    // is actually visible there and not obscured by something, but I guess
    // that's not really worth it
    workspace()->addDamage( r );
    }

void Toplevel::addDamageFull()
    {
    addDamage( rect());
    }

void Toplevel::resetDamage()
    {
    damage_region = QRegion();
    }

#endif

} // namespace
