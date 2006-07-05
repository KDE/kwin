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
#include "scene.h"
#include "scene_basic.h"
#include "scene_xrender.h"

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
    // TODO start tracking unmanaged windows
    compositeTimer.start( 20 );
    XCompositeRedirectSubwindows( display(), rootWindow(), CompositeRedirectManual );
//    scene = new SceneBasic( this );
    scene = new SceneXrender( this );
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
    // TODO stop tracking unmanaged windows
    delete scene;
    scene = NULL;
    for( ClientList::ConstIterator it = clients.begin();
         it != clients.end();
         ++it )
        { // forward all opacity values to the frame in case there'll be other CM running
        if( (*it)->opacity() != 1.0 )
            {
            NETWinInfo i( display(), (*it)->frameId(), rootWindow(), 0 );
            i.setOpacity( long((*it)->opacity() * 0xffffffff ));
            }
        }
    }

void Workspace::addDamage( const QRect& r )
    {
    addDamage( r.x(), r.y(), r.width(), r.height());
    }

void Workspace::addDamage( int x, int y, int w, int h )
    {
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
    if( damage != None )
        {
        XFixesUnionRegion( display(), damage, damage, r );
        if( destroy )
            XFixesDestroyRegion( display(), r );
        }
    else
        {
        if( destroy )
            damage = r;
        else
            {
            damage = XFixesCreateRegion( display(), NULL, 0 );
            XFixesCopyRegion( display(), damage, r );
            }
        }
    }
    
void Workspace::compositeTimeout()
    {
    if( damage == None )
        return;
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
    scene->setWindows( windows );
    scene->paint( damage );
    XFixesDestroyRegion( display(), damage );
    damage = None;
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
        {
        XFreePixmap( display(), window_pixmap );
        window_pixmap = None;
        }
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
    XFixesTranslateRegion( display(), r, x(), y());
    workspace()->addDamage( r, true );
    }
    
#endif

} // namespace
