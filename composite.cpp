/*****************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2006 Lubos Lunak <l.lunak@kde.org>

You can Freely distribute this program under the GNU General Public
License. See the file "COPYING" for the exact licensing terms.
******************************************************************/

#include "utils.h"
#include "workspace.h"
#include "client.h"
#include "unmanaged.h"
#include "effects.h"
#include "scene.h"
#include "scene_basic.h"
#include "scene_xrender.h"
#include "scene_opengl.h"

namespace KWinInternal
{

//****************************************
// Workspace
//****************************************

#if defined( HAVE_XCOMPOSITE ) && defined( HAVE_XDAMAGE ) && defined( HAVE_XFIXES )
void Workspace::setupCompositing()
    {
    if( !options->useTranslucency )
        return;
    if( !Extensions::compositeAvailable() || !Extensions::damageAvailable())
        return;
    if( scene != NULL )
        return;
    compositeTimer.start( 20 );
    XCompositeRedirectSubwindows( display(), rootWindow(), CompositeRedirectManual );
//    scene = new SceneBasic( this );
//    scene = new SceneXrender( this );
    scene = new SceneOpenGL( this );
    effects = new EffectsHandler( this );
    addDamage( 0, 0, displayWidth(), displayHeight());
    foreach( Client* c, clients )
        c->setupCompositing();
    foreach( Unmanaged* c, unmanaged )
        c->setupCompositing();
    }

void Workspace::finishCompositing()
    {
    if( scene == NULL )
        return;
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
    if( damage_region != None )
        XFixesDestroyRegion( display(), damage_region );
    damage_region = None;
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
    }

void Workspace::addDamage( const QRect& r )
    {
    addDamage( r.x(), r.y(), r.width(), r.height());
    }

void Workspace::addDamage( int x, int y, int w, int h )
    {
    if( !compositing())
        return;
    XRectangle r;
    r.x = x;
    r.y = y;
    r.width = w;
    r.height = h;
    addDamage( XFixesCreateRegion( display(), &r, 1 ), true );
    }

void Workspace::addDamage( XserverRegion r, bool destroy )
    {
    if( !compositing())
        return;
    if( damage_region != None )
        {
        XFixesUnionRegion( display(), damage_region, damage_region, r );
        if( destroy )
            XFixesDestroyRegion( display(), r );
        }
    else
        {
        if( destroy )
            damage_region = r;
        else
            {
            damage_region = XFixesCreateRegion( display(), NULL, 0 );
            XFixesCopyRegion( display(), damage_region, r );
            }
        }
    }

void Workspace::addDamage( Toplevel* c, const QRect& r )
    {
    addDamage( c, r.x(), r.y(), r.width(), r.height());
    }

void Workspace::addDamage( Toplevel* c, int x, int y, int w, int h )
    {
    if( !compositing())
        return;
    XRectangle r;
    r.x = x;
    r.y = y;
    r.width = w;
    r.height = h;
    addDamage( c, XFixesCreateRegion( display(), &r, 1 ), true );
    }

void Workspace::addDamage( Toplevel* c, XserverRegion r, bool destroy )
    {
    if( !compositing())
        return;
    if( !destroy )
        {
        XserverRegion r2 = XFixesCreateRegion( display(), NULL, 0 );
        XFixesCopyRegion( display(), r2, r );
        r = r2;
        destroy = true;
        }
    scene->transformWindowDamage( c, r );
    if( damage_region != None )
        {
        XFixesUnionRegion( display(), damage_region, damage_region, r );
        XFixesDestroyRegion( display(), r );
        }
    else
        damage_region = r;
    }

void Workspace::compositeTimeout()
    {
    ToplevelList windows;
    foreach( Toplevel* c, clients )
        windows.append( c );
    foreach( Toplevel* c, unmanaged )
        windows.append( c );
    foreach( Toplevel* c, windows )
        {
        if( c->damage() != None )
            {
            if( damage_region == None )
                damage_region = XFixesCreateRegion( display(), NULL, 0 );
            XserverRegion r = XFixesCreateRegion( display(), NULL, 0 );
            XFixesCopyRegion( display(), r, c->damage());
            scene->transformWindowDamage( c, r );
            XFixesUnionRegion( display(), damage_region, damage_region, r );
            XFixesDestroyRegion( display(), r );
            c->resetDamage();
            }
        }
    if( damage_region == None ) // no damage
        return;
    windows.clear();
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
    scene->paint( damage_region, windows );
    XFixesDestroyRegion( display(), damage_region );
    damage_region = None;
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
    }

void Toplevel::finishCompositing()
    {
    if( damage_handle == None )
        return;
    XDamageDestroy( display(), damage_handle );
    damage_handle = None;
    if( window_pixmap != None )
        XFreePixmap( display(), window_pixmap );
    window_pixmap = None;
    if( damage_region != None )
        XFixesDestroyRegion( display(), damage_region );
    damage_region = None;
    }

void Toplevel::resetWindowPixmap()
    {
    if( !compositing())
        return;
    if( window_pixmap != None )
        XFreePixmap( display(), window_pixmap );
    window_pixmap = None;
    }

Pixmap Toplevel::windowPixmap() const
    {
    if( window_pixmap == None && compositing())
        window_pixmap = XCompositeNameWindowPixmap( display(), handle());
    return window_pixmap;
    }

void Toplevel::damageNotifyEvent( XDamageNotifyEvent* e )
    {
    XserverRegion r = XFixesCreateRegion( display(), &e->area, 1 );
    addDamage( r, true );
    }

void Toplevel::addDamage( const QRect& r )
    {
    addDamage( r.x(), r.y(), r.width(), r.height());
    }

void Toplevel::addDamage( int x, int y, int w, int h )
    {
    if( !compositing())
        return;
    XRectangle r;
    r.x = x;
    r.y = y;
    r.width = w;
    r.height = h;
    addDamage( XFixesCreateRegion( display(), &r, 1 ), true );
    }

void Toplevel::addDamage( XserverRegion r, bool destroy )
    {
    if( !compositing())
        return;
    if( damage_region != None )
        {
        XFixesUnionRegion( display(), damage_region, damage_region, r );
        if( destroy )
            XFixesDestroyRegion( display(), r );
        }
    else
        {
        if( destroy )
            damage_region = r;
        else
            {
            damage_region = XFixesCreateRegion( display(), NULL, 0 );
            XFixesCopyRegion( display(), damage_region, r );
            }
        }
    }

void Toplevel::resetDamage()
    {
    if( damage_region != None )
        XFixesDestroyRegion( display(), damage_region );
    damage_region = None;
    }

#endif

} // namespace
