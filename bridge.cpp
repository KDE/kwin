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
BRIDGE_HELPER( void, setKeepAbove, bool set, set, )
BRIDGE_HELPER( void, setKeepBelow, bool set, set, )

NET::WindowType Bridge::windowType( unsigned long supported_types ) const
    {
    return c->windowType( false, supported_types );
    }

QIconSet Bridge::icon() const
    {
    return QIconSet( c->miniIcon(), c->icon());
    }

void Bridge::showWindowMenu( QPoint p )
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
    NETRect r = c->netWinInfo()->iconGeometry();
    return QRect( r.pos.x, r.pos.y, r.size.width, r.size.height );
    }

QWidget* Bridge::workspaceWidget() const
    {
    return c->workspace()->desktopWidget();
    }

void Bridge::titlebarDblClickOperation()
    {
    c->workspace()->performWindowOperation( c, options->operationTitlebarDblClick());
    }

void Bridge::setShade( bool set )
    {
    c->setShade( set ? Client::ShadeNormal : Client::ShadeNone );
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
    return Qt::WX11BypassWM;
    }

void Bridge::helperShowHide( bool show )
    {
    if( show )
        c->rawShow();
    else
        c->rawHide();
    }

} // namespace
