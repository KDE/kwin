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
    if( composite_pixmap != None )
        return;
    compositeTimer.start( 20 );
    XCompositeRedirectSubwindows( display(), rootWindow(), CompositeRedirectManual );
    composite_pixmap = XCreatePixmap( display(), rootWindow(), displayWidth(), displayHeight(), QX11Info::appDepth());
    setDamaged();
    }

void Workspace::finishCompositing()
    {
    if( composite_pixmap == None )
        return;
    XCompositeUnredirectSubwindows( display(), rootWindow(), CompositeRedirectManual );
    XFreePixmap( display(), composite_pixmap );
    compositeTimer.stop();
    composite_pixmap = None;
    }

void Workspace::setDamaged()
    {
    if( !compositing())
        return;
    damaged = true;
    }
    
void Workspace::compositeTimeout()
    {
#define EFF
#ifdef EFF
    const int SPD = 10;
    static int cnt = 0;
    int olds = cnt / SPD;
    ++cnt;
    if( cnt == 8 * SPD )
        cnt = 0;
    int s = cnt / SPD;
    if( !damaged && s == olds )
        return;
#else
    if( !damaged )
        return;
#endif
    XGCValues val;
    val.foreground = WhitePixel( display(), DefaultScreen( display()));
    val.subwindow_mode = IncludeInferiors;
    GC gc = XCreateGC( display(), composite_pixmap, GCForeground | GCSubwindowMode, &val );
    XFillRectangle( display(), composite_pixmap, gc, 0, 0, displayWidth(), displayHeight());
#ifdef EFF
    val.fill_style = FillStippled;
    char data[] = { 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x00 };
    data[ s ] = 1 << s;
    val.stipple = XCreateBitmapFromData( display(), rootWindow(), data, 8, 8 );
    val.foreground = BlackPixel( display(), DefaultScreen( display()));
    GC gc2 = XCreateGC( display(), composite_pixmap, GCForeground | GCSubwindowMode | GCFillStyle | GCStipple, &val );
#endif
    for( ClientList::ConstIterator it = stackingOrder().begin();
         it != stackingOrder().end();
         ++it )
        {
        QRect r = (*it)->geometry().intersect( QRect( 0, 0, displayWidth(), displayHeight()));
        if( !r.isEmpty())
            {
            XCopyArea( display(), (*it)->windowPixmap(), composite_pixmap, gc,
                qMax( 0, -(*it)->x()), qMax( 0, -(*it)->y()), r.width(), r.height(), r.x(), r.y());
#ifdef EFF
            XFillRectangle( display(), composite_pixmap, gc2, r.x(), r.y(), r.width(), r.height());
#endif
            }
        }
    for( UnmanagedList::ConstIterator it = unmanaged.begin();
         it != unmanaged.end();
         ++it )
        {
        QRect r = (*it)->geometry().intersect( QRect( 0, 0, displayWidth(), displayHeight()));
        if( !r.isEmpty())
            XCopyArea( display(), (*it)->windowPixmap(), composite_pixmap, gc,
                qMax( 0, -(*it)->x()), qMax( 0, -(*it)->y()), r.width(), r.height(), r.x(), r.y());
        }
    XCopyArea( display(), composite_pixmap, rootWindow(), gc, 0, 0, displayWidth(), displayHeight(), 0, 0 );
#ifdef EFF
    XFreePixmap( display(), val.stipple );
    XFreeGC( display(), gc2 );
#endif
    XFreeGC( display(), gc );
    XFlush( display());
    damaged = false;
    }

//****************************************
// Toplevel
//****************************************

void Toplevel::setupCompositing()
    {
    if( !workspace()->compositing())
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
    if( !workspace()->compositing())
        return;
    workspace()->setDamaged();
    }

void Toplevel::resetWindowPixmap()
    {
    if( !workspace()->compositing())
        return;
    if( window_pixmap != None )
        XFreePixmap( display(), window_pixmap );
    window_pixmap = None;
    }

Pixmap Toplevel::windowPixmap() const
    {
    if( window_pixmap == None && workspace()->compositing())
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
