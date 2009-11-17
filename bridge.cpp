/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2003 Lubos Lunak <l.lunak@kde.org>

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

#include "bridge.h"

#include "client.h"
#include "options.h"
#include "effects.h"

#include <kconfiggroup.h>

namespace KWin
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

QIcon Bridge::icon() const
    {
    QIcon ret( c->icon());
    ret.addPixmap( c->miniIcon());
    return ret;
    }

bool Bridge::isSetShade() const
    {
    return c->shadeMode() != ShadeNone;
    }

void Bridge::showWindowMenu( const QPoint &p )
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
    c->performMouseCommand( options->operationTitlebarMouseWheel( delta ), cursorPos());
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

QRegion Bridge::unobscuredRegion( const QRegion& r ) const
    {
    QRegion reg( r );
    const ClientList stacking_order = c->workspace()->stackingOrder();
    int pos = stacking_order.indexOf( c );
    ++pos;
    for(; pos < stacking_order.count(); ++pos )
        {
        if( !stacking_order[pos]->isShown( true ))
            continue; // these don't obscure the window
        if( c->isOnAllDesktops())
            {
            if( !stacking_order[ pos ]->isOnCurrentDesktop())
                continue;
            }
        else
            {
            if( !stacking_order[ pos ]->isOnDesktop( c->desktop()))
                continue;
            }
        /* the clients all have their mask-regions in local coords
           so we have to translate them to a shared coord system
           we choose ours */
        int dx = stacking_order[ pos ]->x() - c->x();
        int dy = stacking_order[ pos ]->y() - c->y();
        QRegion creg = stacking_order[ pos ]->mask();
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
        KWin::grabXServer();
    else
        KWin::ungrabXServer();
    }

bool Bridge::compositingActive() const
    {
    return c->workspace()->compositingActive();
    }

bool Bridge::isClientGroupActive()
    {
    return c->clientGroup()->containsActiveClient();
    }

QList< ClientGroupItem > Bridge::clientGroupItems() const
    {
    return c->clientGroup()->items();
    }

long Bridge::itemId( int index )
    {
    const ClientList list = c->clientGroup()->clients();
    return reinterpret_cast<long>( list.at( index ));
    }

int Bridge::visibleClientGroupItem()
    {
    return c->clientGroup()->indexOfVisibleClient();
    }

void Bridge::setVisibleClientGroupItem( int index )
    {
    c->clientGroup()->setVisible( index );
    }

void Bridge::moveItemInClientGroup( int index, int before )
    {
    c->clientGroup()->move( index, before );
    }

void Bridge::moveItemToClientGroup( long itemId, int before )
    {
    Client* item = reinterpret_cast<Client*>( itemId );
    c->workspace()->moveItemToClientGroup( item->clientGroup(), item->clientGroup()->indexOfClient( item ),
        c->clientGroup(), before );
    }

void Bridge::removeFromClientGroup( int index, const QRect& newGeom )
    {
    c->clientGroup()->remove( index, newGeom );
    }

void Bridge::closeClientGroupItem( int index )
    {
    const ClientList list = c->clientGroup()->clients();
    if( index >= 0 || index <= list.count() )
        list.at( index )->closeWindow();
    }

void Bridge::closeAllInClientGroup()
    {
    c->clientGroup()->closeAll();
    }

void Bridge::displayClientMenu( int index, const QPoint& pos )
    {
    c->clientGroup()->displayClientMenu( index, pos );
    }

KDecoration::WindowOperation Bridge::buttonToWindowOperation( Qt::MouseButtons button )
    {
    return c->mouseButtonToWindowOperation( button );
    }

} // namespace
