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
 This is very simple compositing code using only plain X. It doesn't use any effects
 or anything like that, it merely draws everything as it would be visible without
 compositing. It was the first compositing code in KWin and is usable only for testing
 and as the very simple "this is how it works".
*/

#include "scene_basic.h"

#include "utils.h"
#include "client.h"

namespace KWin
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
    Pixmap composite_pixmap = XCreatePixmap( display(), rootWindow(), displayWidth(), displayHeight(), DefaultDepth( display(), DefaultScreen( display())));
    XGCValues val;
    val.foreground = WhitePixel( display(), DefaultScreen( display()));
    val.subwindow_mode = IncludeInferiors;
    GC gc = XCreateGC( display(), composite_pixmap, GCForeground | GCSubwindowMode, &val );
    XFillRectangle( display(), composite_pixmap, gc, 0, 0, displayWidth(), displayHeight());
    for( ToplevelList::ConstIterator it = windows.begin();
         it != windows.end();
         ++it )
        {
        QRect r = (*it)->geometry().intersected( QRect( 0, 0, displayWidth(), displayHeight()));
        if( !r.isEmpty())
            {
            Pixmap pix = (*it)->windowPixmap();
            if( pix == None )
                continue;
            XCopyArea( display(), pix, composite_pixmap, gc,
                qMax( 0, -(*it)->x()), qMax( 0, -(*it)->y()), r.width(), r.height(), r.x(), r.y());
            }
        }
    XCopyArea( display(), composite_pixmap, rootWindow(), gc, 0, 0, displayWidth(), displayHeight(), 0, 0 );
    XFreeGC( display(), gc );
    XFreePixmap( display(), composite_pixmap );
    XFlush( display());
    }

bool SceneBasic::initFailed() const
    {
    return false;
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
