/*
 *
 * Copyright (c) 2003 Lubos Lunak <l.lunak@kde.org>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include "preview.h"

#include <kapplication.h>
#include <klocale.h>
#include <kconfig.h>
#include <kglobal.h>
#include <qlabel.h>
#include <qstyle.h>
#include <kiconloader.h>

#include <X11/Xlib.h>
#include <X11/extensions/shape.h>

#include <kdecorationfactory.h>
#include <kdecoration_plugins_p.h>

// FRAME the preview doesn't update to reflect the changes done in the kcm

KDecorationPreview::KDecorationPreview( QWidget* parent, const char* name )
    :   QWidget( parent, name )
{
    options = new KDecorationPreviewOptions;

    bridge = new KDecorationPreviewBridge( this, true );

    deco = 0;

    setFixedSize( 600, 500 );

    positionPreviews();
}

KDecorationPreview::~KDecorationPreview()
{
    delete deco;
    delete bridge;
    delete options;
}

void KDecorationPreview::performRepaintTest(int n)
{
    for (int i = 0; i < n; ++i) {
        deco->widget()->repaint();
        kapp->processEvents();
    }
}

bool KDecorationPreview::recreateDecoration( KDecorationPlugins* plugins )
{
    delete deco;
    deco = plugins->createDecoration(bridge);
    deco->init();

    if (!deco)
        return false;

    positionPreviews();
    deco->widget()->show();

    return true;
}

void KDecorationPreview::positionPreviews()
{
//     int titleBarHeight, leftBorder, rightBorder, xoffset,
//         dummy1, dummy2, dummy3;

    if ( !deco )
        return;

    QSize size = QSize(width()-2*10, height()-2*10)/*.expandedTo(deco->minimumSize()*/;

    QRect geometry(QPoint(10, 10), size);
    deco->widget()->setGeometry(geometry);

//     // don't have more than one reference to the same dummy variable in one borders() call.
//     deco[Active]->borders( dummy1, dummy2, titleBarHeight, dummy3 );
//     deco[Inactive]->borders( leftBorder, rightBorder, dummy1, dummy2 );
//
//     titleBarHeight = kMin( int( titleBarHeight * .9 ), 30 );
//     xoffset = kMin( kMax( 10, QApplication::reverseLayout()
// 			    ? leftBorder : rightBorder ), 30 );
//
//     // Resize the active window
//     size = QSize( width() - xoffset, height() - titleBarHeight )
//                 .expandedTo( deco[Active]->minimumSize() );
//     geometry = QRect( QPoint( 0, titleBarHeight ), size );
//     deco[Active]->widget()->setGeometry( QStyle::visualRect( geometry, this ) );
//
//     // Resize the inactive window
//     size = QSize( width() - xoffset, height() - titleBarHeight )
//                 .expandedTo( deco[Inactive]->minimumSize() );
//     geometry = QRect( QPoint( xoffset, 0 ), size );
//     deco[Inactive]->widget()->setGeometry( QStyle::visualRect( geometry, this ) );
}

void KDecorationPreview::setPreviewMask( const QRegion& reg, int mode )
{
    QWidget *widget = deco->widget();

    // FRAME duped from client.cpp
    if( mode == Unsorted )
        {
        XShapeCombineRegion( qt_xdisplay(), widget->winId(), ShapeBounding, 0, 0,
            reg.handle(), ShapeSet );
        }
    else
        {
        QMemArray< QRect > rects = reg.rects();
        XRectangle* xrects = new XRectangle[ rects.count() ];
        for( unsigned int i = 0;
             i < rects.count();
             ++i )
            {
            xrects[ i ].x = rects[ i ].x();
            xrects[ i ].y = rects[ i ].y();
            xrects[ i ].width = rects[ i ].width();
            xrects[ i ].height = rects[ i ].height();
            }
        XShapeCombineRectangles( qt_xdisplay(), widget->winId(), ShapeBounding, 0, 0,
	    xrects, rects.count(), ShapeSet, mode );
        delete[] xrects;
        }
}

QRect KDecorationPreview::windowGeometry( bool active ) const
{
    QWidget *widget = deco->widget();
    return widget->geometry();
}

QRegion KDecorationPreview::unobscuredRegion( bool active, const QRegion& r ) const
{
        return r;
}

KDecorationPreviewBridge::KDecorationPreviewBridge( KDecorationPreview* p, bool a )
    :   preview( p ), active( a )
    {
    }

QWidget* KDecorationPreviewBridge::initialParentWidget() const
    {
    return preview;
    }

Qt::WFlags KDecorationPreviewBridge::initialWFlags() const
    {
    return 0;
    }

bool KDecorationPreviewBridge::isActive() const
    {
    return active;
    }

bool KDecorationPreviewBridge::isCloseable() const
    {
    return true;
    }

bool KDecorationPreviewBridge::isMaximizable() const
    {
    return true;
    }

KDecoration::MaximizeMode KDecorationPreviewBridge::maximizeMode() const
    {
    return KDecoration::MaximizeRestore;
    }

bool KDecorationPreviewBridge::isMinimizable() const
    {
    return true;
    }

bool KDecorationPreviewBridge::providesContextHelp() const
    {
    return true;
    }

int KDecorationPreviewBridge::desktop() const
    {
    return 1;
    }

bool KDecorationPreviewBridge::isModal() const
    {
    return false;
    }

bool KDecorationPreviewBridge::isShadeable() const
    {
    return true;
    }

bool KDecorationPreviewBridge::isShade() const
    {
    return false;
    }

bool KDecorationPreviewBridge::isSetShade() const
    {
    return false;
    }

bool KDecorationPreviewBridge::keepAbove() const
    {
    return false;
    }

bool KDecorationPreviewBridge::keepBelow() const
    {
    return false;
    }

bool KDecorationPreviewBridge::isMovable() const
    {
    return true;
    }

bool KDecorationPreviewBridge::isResizable() const
    {
    return true;
    }

NET::WindowType KDecorationPreviewBridge::windowType( unsigned long ) const
    {
    return NET::Normal;
    }

QIconSet KDecorationPreviewBridge::icon() const
    {
    return SmallIconSet( "xapp" );
    }

QString KDecorationPreviewBridge::caption() const
    {
    return active ? i18n( "Active Window" ) : i18n( "Inactive Window" );
    }

void KDecorationPreviewBridge::processMousePressEvent( QMouseEvent* )
    {
    }

void KDecorationPreviewBridge::showWindowMenu( const QRect &)
    {
    }

void KDecorationPreviewBridge::showWindowMenu( QPoint )
    {
    }

void KDecorationPreviewBridge::performWindowOperation( WindowOperation )
    {
    }

void KDecorationPreviewBridge::setMask( const QRegion& reg, int mode )
    {
    preview->setPreviewMask( reg, mode );
    }

bool KDecorationPreviewBridge::isPreview() const
    {
    return true;
    }

QRect KDecorationPreviewBridge::geometry() const
    {
    return preview->windowGeometry( active );
    }

QRect KDecorationPreviewBridge::iconGeometry() const
    {
    return QRect();
    }

QRegion KDecorationPreviewBridge::unobscuredRegion( const QRegion& r ) const
    {
    return preview->unobscuredRegion( active, r );
    }

QWidget* KDecorationPreviewBridge::workspaceWidget() const
    {
    return preview;
    }

WId KDecorationPreviewBridge::windowId() const
    {
    return 0; // no decorated window
    }

void KDecorationPreviewBridge::closeWindow()
    {
    }

void KDecorationPreviewBridge::maximize( MaximizeMode )
    {
    }

void KDecorationPreviewBridge::minimize()
    {
    }

void KDecorationPreviewBridge::showContextHelp()
    {
    }

void KDecorationPreviewBridge::setDesktop( int )
    {
    }

void KDecorationPreviewBridge::titlebarDblClickOperation()
    {
    }

void KDecorationPreviewBridge::setShade( bool )
    {
    }

void KDecorationPreviewBridge::setKeepAbove( bool )
    {
    }

void KDecorationPreviewBridge::setKeepBelow( bool )
    {
    }

int KDecorationPreviewBridge::currentDesktop() const
    {
    return 1;
    }

void KDecorationPreviewBridge::helperShowHide( bool )
    {
    }

void KDecorationPreviewBridge::grabXServer( bool )
    {
    }

KDecorationPreviewOptions::KDecorationPreviewOptions()
    {
    d = new KDecorationOptionsPrivate;
    d->defaultKWinSettings();
    updateSettings();
    }

KDecorationPreviewOptions::~KDecorationPreviewOptions()
    {
    delete d;
    }

unsigned long KDecorationPreviewOptions::updateSettings()
{
    KConfig cfg( "kwinrc", true );
    unsigned long changed = 0;
    changed |= d->updateKWinSettings( &cfg );

    return changed;
}

bool KDecorationPreviewPlugins::provides( Requirement )
    {
    return false;
    }

// #include "preview.moc"
