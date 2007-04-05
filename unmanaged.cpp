/*****************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2006 Lubos Lunak <l.lunak@kde.org>

You can Freely distribute this program under the GNU General Public
License. See the file "COPYING" for the exact licensing terms.
******************************************************************/

#include "unmanaged.h"

#include "workspace.h"
#include "effects.h"
#include "deleted.h"

#include <X11/extensions/shape.h>

namespace KWin
{

Unmanaged::Unmanaged( Workspace* ws )
    : Toplevel( ws )
    {
    }
    
Unmanaged::~Unmanaged()
    {
    }

bool Unmanaged::track( Window w )
    {
    XWindowAttributes attr;
    grabXServer();
    if( !XGetWindowAttributes(display(), w, &attr) || attr.map_state != IsViewable )
        {
        ungrabXServer();
        return false;
        }
    if( attr.c_class == InputOnly )
        {
        ungrabXServer();
        return false;
        }
    setWindowHandles( w, w ); // the window is also the frame
    XSelectInput( display(), w, StructureNotifyMask );
    geom = QRect( attr.x, attr.y, attr.width, attr.height );
    vis = attr.visual;
    bit_depth = attr.depth;
    unsigned long properties[ 2 ];
    properties[ NETWinInfo::PROTOCOLS ] =
        NET::WMWindowType |
        NET::WMPid |
        0;
    properties[ NETWinInfo::PROTOCOLS2 ] =
        NET::WM2Opacity |
        0;
    info = new NETWinInfo( display(), w, rootWindow(), properties, 2 );

    if( Extensions::shapeAvailable())
        XShapeSelectInput( display(), w, ShapeNotifyMask );
    detectShape( w );
    setupCompositing();
    ungrabXServer();
    if( effects )
        effects->checkInputWindowStacking();
    return true;
    }

void Unmanaged::release()
    {
    Deleted* del = Deleted::create( this );
    if( effects )
        {
        effects->windowClosed( effectWindow());
        scene->windowClosed( this, del );
        }
    finishCompositing();
    workspace()->removeUnmanaged( this, Allowed );
    if( Extensions::shapeAvailable())
        XShapeSelectInput( display(), window(), NoEventMask );
    XSelectInput( display(), window(), NoEventMask );
    addWorkspaceRepaint( geometry());
    disownDataPassedToDeleted();
    del->unrefWindow();
    deleteUnmanaged( this, Allowed );
    }

void Unmanaged::deleteUnmanaged( Unmanaged* c, allowed_t )
    {
    delete c;
    }

int Unmanaged::desktop() const
    {
    return NET::OnAllDesktops; // TODO for some window types should be the current desktop?
    }

void Unmanaged::debug( kdbgstream& stream ) const
    {
    stream << "\'ID:" << window() << "\'";
    }

} // namespace

#include "unmanaged.moc"
