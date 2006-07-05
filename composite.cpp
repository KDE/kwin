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

namespace KWinInternal
{

//****************************************
// Workspace
//****************************************

#if defined( HAVE_XCOMPOSITE ) && defined( HAVE_XDAMAGE )
void Workspace::setupCompositing()
    {
    if( !Extensions::compositeAvailable() || !Extensions::damageAvailable())
        return;
    if( scene != NULL )
        return;
    // TODO start tracking unmanaged windows
    compositeTimer.start( 20 );
    XCompositeRedirectSubwindows( display(), rootWindow(), CompositeRedirectManual );
    setDamaged();
    scene = new SceneBasic( this );
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

void Workspace::setDamaged()
    {
    if( !compositing())
        return;
    damaged = true;
    }
    
void Workspace::compositeTimeout()
    {
    if( !damaged )
        return;
    damaged = false;
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
    scene->paint();
    }

//****************************************
// Toplevel
//****************************************

void Toplevel::setupCompositing()
    {
    if( !compositing())
        return;
    if( damage != None )
        return;
    damage = XDamageCreate( display(), handle(), XDamageReportRawRectangles );
    setDamaged();
    }

void Toplevel::finishCompositing()
    {
    if( damage == None )
        return;
    XDamageDestroy( display(), damage );
    damage = None;
    if( window_pixmap != None )
        {
        XFreePixmap( display(), window_pixmap );
        window_pixmap = None;
        }
    }

void Toplevel::setDamaged()
    {
    if( !compositing())
        return;
    workspace()->setDamaged();
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

//****************************************
// Client
//****************************************

void Client::damageNotifyEvent( XDamageNotifyEvent* )
    {
    setDamaged();
    }
    
//****************************************
// Unmanaged
//****************************************

void Unmanaged::damageNotifyEvent( XDamageNotifyEvent* )
    {
    setDamaged();
    }
    
#endif

} // namespace
