/*****************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2006 Lubos Lunak <l.lunak@kde.org>

You can Freely distribute this program under the GNU General Public
License. See the file "COPYING" for the exact licensing terms.
******************************************************************/

#include "scene_basic.h"

#include "utils.h"
#include "client.h"

namespace KWinInternal
{

//****************************************
// SceneBasic
//****************************************

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
            XCopyArea( display(), (*it)->windowPixmap(), composite_pixmap, gc,
                qMax( 0, -(*it)->x()), qMax( 0, -(*it)->y()), r.width(), r.height(), r.x(), r.y());
            }
        }
    XCopyArea( display(), composite_pixmap, rootWindow(), gc, 0, 0, displayWidth(), displayHeight(), 0, 0 );
    XFreeGC( display(), gc );
    XFreePixmap( display(), composite_pixmap );
    XFlush( display());
    }

} // namespace
