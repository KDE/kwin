/*****************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2003 Lubos Lunak <l.lunak@kde.org>

You can Freely distribute this program under the GNU General Public
License. See the file "COPYING" for the exact licensing terms.
******************************************************************/

#include "bridge.h"

#include "client.h"
#include "options.h"

namespace KWinInternal
{

Bridge::Bridge( Client* cl )
    : c( cl )
    {
    }

#define BRIDGE_HELPER( rettype, prototype, args1, args2, cst ) \
rettype Bridge::prototype ( args1 ) cst \
    { \
    return c->prototype( args2 ); \
    }

BRIDGE_HELPER( bool, isActive,,, const )
BRIDGE_HELPER( bool, isCloseable,,, const )
BRIDGE_HELPER( bool, isMaximizable,,, const )
BRIDGE_HELPER( Bridge::MaximizeMode, maximizeMode,,, const )
BRIDGE_HELPER( bool, isMinimizable,,, const )
BRIDGE_HELPER( bool, providesContextHelp,,, const )
BRIDGE_HELPER( int, desktop,,, const )
BRIDGE_HELPER( bool, isModal,,, const )
BRIDGE_HELPER( bool, isShadeable,,, const )
BRIDGE_HELPER( bool, isShade,,, const )
BRIDGE_HELPER( bool, keepAbove,,, const )
BRIDGE_HELPER( bool, keepBelow,,, const )
BRIDGE_HELPER( bool, isMovable,,, const )
BRIDGE_HELPER( bool, isResizable,,, const )
BRIDGE_HELPER( QString, caption,,, const )
BRIDGE_HELPER( void, processMousePressEvent, QMouseEvent* e, e, )
BRIDGE_HELPER( QRect, geometry,,, const )
BRIDGE_HELPER( void, closeWindow,,, )
BRIDGE_HELPER( void, maximize, MaximizeMode m, m, )
BRIDGE_HELPER( void, minimize,,, )
BRIDGE_HELPER( void, showContextHelp,,, )
BRIDGE_HELPER( void, setDesktop, int desktop, desktop, )

void Bridge::setKeepAbove( bool set )
    {
    if( c->keepAbove() != set )
        c->workspace()->performWindowOperation( c, KeepAboveOp );
    }

void Bridge::setKeepBelow( bool set )
    {
    if( c->keepBelow() != set )
        c->workspace()->performWindowOperation( c, KeepBelowOp );
    }

NET::WindowType Bridge::windowType( unsigned long supported_types ) const
    {
    return c->windowType( false, supported_types );
    }

QIconSet Bridge::icon() const
    {
    return QIconSet( c->miniIcon(), c->icon());
    }

bool Bridge::isSetShade() const
    {
    return c->shadeMode() != ShadeNone;
    }

void Bridge::showWindowMenu( QPoint p )
    {
    c->workspace()->showWindowMenu( p, c );
    }

void Bridge::showWindowMenu( const QRect &p )
    {
    c->workspace()->showWindowMenu( p, c );
    }

void Bridge::performWindowOperation( WindowOperation op )
    {
    c->workspace()->performWindowOperation( c, op );
    }

void Bridge::setMask( const QRegion& r, int mode )
    {
    c->setMask( r, mode );
    }

bool Bridge::isPreview() const
    {
    return false;
    }

QRect Bridge::iconGeometry() const
    {
    NETRect r = c->info->iconGeometry();
    return QRect( r.pos.x, r.pos.y, r.size.width, r.size.height );
    }

QWidget* Bridge::workspaceWidget() const
    {
    return c->workspace()->desktopWidget();
    }

WId Bridge::windowId() const
    {
    return c->window();
    }

void Bridge::titlebarDblClickOperation()
    {
    c->workspace()->performWindowOperation( c, options->operationTitlebarDblClick());
    }

void Bridge::titlebarMouseWheelOperation( int delta )
    {
    c->performMouseCommand( options->operationTitlebarMouseWheel( delta ), QCursor::pos());
    }

void Bridge::setShade( bool set )
    {
    c->setShade( set ? ShadeNormal : ShadeNone );
    }

int Bridge::currentDesktop() const
    {
    return c->workspace()->currentDesktop();
    }

QWidget* Bridge::initialParentWidget() const
    {
    return NULL;
    }

Qt::WFlags Bridge::initialWFlags() const
    {
    return 0;
    }

void Bridge::helperShowHide( bool show )
    {
    if( show )
        c->rawShow();
    else
        c->rawHide();
    }

QRegion Bridge::unobscuredRegion( const QRegion& r ) const
    {
    QRegion reg( r );
    const ClientList stacking_order = c->workspace()->stackingOrder();
    ClientList::ConstIterator it = stacking_order.find( c );
    ++it;
    for(;
         it != stacking_order.end();
         ++it )
        {
        if( !(*it)->isShown( true ))
            continue; // these don't obscure the window
        if( c->isOnAllDesktops())
            {
            if( !(*it)->isOnCurrentDesktop())
                continue;
            }
        else
            {
            if( !(*it)->isOnDesktop( c->desktop()))
                continue;
            }
        /* the clients all have their mask-regions in local coords
	   so we have to translate them to a shared coord system
	   we choose ours */
	int dx = (*it)->x() - c->x();
	int dy = (*it)->y() - c->y();
	QRegion creg = (*it)->mask();
	creg.translate(dx, dy);
	reg -= creg;
	if (reg.isEmpty())
            {
	    // early out, we are completely obscured
	    break;
	    }
        }
    return reg;
    }

void Bridge::grabXServer( bool grab )
    {
    if( grab )
        KWinInternal::grabXServer();
    else
        KWinInternal::ungrabXServer();
    }

} // namespace
