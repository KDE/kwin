/*****************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2006 Lubos Lunak <l.lunak@kde.org>

You can Freely distribute this program under the GNU General Public
License. See the file "COPYING" for the exact licensing terms.
******************************************************************/

/*
 This is very simple compositing code using only plain X. It doesn't use any effects
 or anything like that, it merely draws everything as it would be visible without
 compositing. It was the first compositing code in KWin and is usable only for testing
 and as the very simple "this is how it works".
*/

#include "scene_basic.h"

#include "utils.h"
#include "client.h"

namespace KWinInternal
{

SceneBasic::SceneBasic( Workspace* ws )
    : Scene( ws )
    {
    }

SceneBasic::~SceneBasic()
    {
    }
    
void SceneBasic::paint( QRegion, ToplevelList windows )
    {
    Pixmap composite_pixmap = XCreatePixmap( display(), rootWindow(), displayWidth(), displayHeight(), QX11Info::appDepth());
    XGCValues val;
    val.foreground = WhitePixel( display(), DefaultScreen( display()));
    val.subwindow_mode = IncludeInferiors;
    GC gc = XCreateGC( display(), composite_pixmap, GCForeground | GCSubwindowMode, &val );
    XFillRectangle( display(), composite_pixmap, gc, 0, 0, displayWidth(), displayHeight());
    for( ToplevelList::ConstIterator it = windows.begin();
         it != windows.end();
         ++it )
        {
        QRect r = (*it)->geometry().intersect( QRect( 0, 0, displayWidth(), displayHeight()));
        if( !r.isEmpty())
            {
            Pixmap pix = (*it)->windowPixmap();
            XCopyArea( display(), pix, composite_pixmap, gc,
                qMax( 0, -(*it)->x()), qMax( 0, -(*it)->y()), r.width(), r.height(), r.x(), r.y());
            }
        }
    XCopyArea( display(), composite_pixmap, rootWindow(), gc, 0, 0, displayWidth(), displayHeight(), 0, 0 );
    XFreeGC( display(), gc );
    XFreePixmap( display(), composite_pixmap );
    XFlush( display());
    }

// These functions are not used at all, SceneBasic
// is not using inherited functionality.

void SceneBasic::paintBackground( QRegion )
    {
    }

void SceneBasic::windowGeometryShapeChanged( Toplevel* )
    {
    }

void SceneBasic::windowOpacityChanged( Toplevel* )
    {
    }

void SceneBasic::windowAdded( Toplevel* )
    {
    }

void SceneBasic::windowClosed( Toplevel*, Deleted* )
    {
    }

void SceneBasic::windowDeleted( Deleted* )
    {
    }

} // namespace
