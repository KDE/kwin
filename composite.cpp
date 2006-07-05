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
    }

void Workspace::finishCompositing()
    {
    if( scene == NULL )
        return;
    XCompositeUnredirectSubwindows( display(), rootWindow(), CompositeRedirectManual );
    compositeTimer.stop();
    // TODO stop tracking unmanaged windows
    delete scene;
    scene = NULL;
    }

void Workspace::addDamage( const QRect& r )
    {
    addDamage( r.x(), r.y(), r.height(), r.width());
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

struct XXX
    {
    XXX( XserverRegion r ) : rr( r ) {}
    XserverRegion rr;
    };
    
kdbgstream& operator<<( kdbgstream& stream, XXX r )
    {
    if( r.rr == None )
        return stream << "NONE";
    int num;
    XRectangle* rects = XFixesFetchRegion( display(), r.rr, &num );
    if( rects == NULL || num == 0 )
        return stream << "NONE";
    for( int i = 0;
         i < num;
         ++i )
        stream << "[" << rects[ i ].x << "+" << rects[ i ].y << " " << rects[ i ].width << "x" << rects[ i ].height << "]";
    return stream;
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
