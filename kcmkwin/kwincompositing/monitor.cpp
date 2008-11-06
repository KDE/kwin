/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2008 Lubos Lunak <l.lunak@suse.cz>

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

#include "monitor.h"

#include <kdebug.h>
#include <kstandarddirs.h>
#include <qgraphicsitem.h>
#include <qgraphicsview.h>
#include <qgraphicsscene.h>
#include <qgraphicssceneevent.h>
#include <qmenu.h>

namespace KWin
{

Monitor::Monitor( QWidget* parent )
    : QLabel( parent )
    {
    setPixmap( QPixmap( KStandardDirs::locate( "data", "kcontrol/pics/monitor.png" )));
    for( int i = 0;
         i < 8;
         ++i )
        popups[ i ] = new QMenu( this );
    setAlignment( Qt::AlignCenter );
    scene = new QGraphicsScene( this );
    view = new QGraphicsView( scene, this );
    view->setBackgroundBrush( Qt::black );
    view->setHorizontalScrollBarPolicy( Qt::ScrollBarAlwaysOff );
    view->setVerticalScrollBarPolicy( Qt::ScrollBarAlwaysOff );
    view->setFocusPolicy( Qt::NoFocus );
    view->setFrameShape( QFrame::NoFrame );
    for( int i = 0;
         i < 8;
         ++i )
        {
        items[ i ] = new Corner( this );
        scene->addItem( items[ i ] );
        items[ i ]->setBrush( Qt::red ); // TODO color-scheme dependent?
        grp[ i ] = new QActionGroup( this );
        }
    checkSize();
    }

void Monitor::clear()
    {
    for( int i = 0;
         i < 8;
         ++i )
        {
        popups[ i ]->clear();
        setEdge( i, false );
        delete grp[ i ];
        grp[ i ] = new QActionGroup( this );
        }
    }

void Monitor::resizeEvent( QResizeEvent* e )
    {
    QLabel::resizeEvent( e );
    checkSize();
    }
    
void Monitor::checkSize()
    {
    int w = 151;
    int h = 115;
    view->setGeometry((width()-200)/2+23, (height()-186)/2+14, w, h );
    scene->setSceneRect( 0, 0, w, h );
    int x2 = ( w - 20 ) / 2;
    int x3 = w - 20;
    int y2 = ( h - 20 ) / 2;
    int y3 = h - 20;
    items[ 0 ]->setRect( 0, y2, 20, 20 );
    items[ 1 ]->setRect( x3, y2, 20, 20 );
    items[ 2 ]->setRect( x2, 0, 20, 20 );
    items[ 3 ]->setRect( x2, y3, 20, 20 );
    items[ 4 ]->setRect( 0, 0, 20, 20 );
    items[ 5 ]->setRect( x3, 0, 20, 20 );
    items[ 6 ]->setRect( 0, y3, 20, 20 );
    items[ 7 ]->setRect( x3, y3, 20, 20 );
    }

void Monitor::setEdge( int edge, bool set )
    {
    items[ edge ]->setBrush( set ? Qt::green : Qt::red );
    }

bool Monitor::edge( int edge ) const
    {
    return items[ edge ]->brush() == Qt::green;
    }

void Monitor::addEdgeItem( int edge, const QString& item )
    {
    QAction* act = popups[ edge ]->addAction( item );
    act->setCheckable( true );
    popup_actions[ edge ].append( act );
    grp[ edge ]->addAction( act );
    if( popup_actions[ edge ].count() == 1 )
        act->setChecked( true );
    setEdge( edge, !popup_actions[ edge ][ 0 ]->isChecked());
    }

void Monitor::selectEdgeItem( int edge, int index )
    {
    popup_actions[ edge ][ index ]->setChecked( true );
    setEdge( edge, !popup_actions[ edge ][ 0 ]->isChecked());
    }

int Monitor::selectedEdgeItem( int edge ) const
    {
    foreach( QAction* act, popup_actions[ edge ] )
        if( act->isChecked())
            return popup_actions[ edge ].indexOf( act );
    abort();
    }

void Monitor::popup( Corner* c, QPoint pos )
    {
    for( int i = 0;
         i < 8;
         ++i )
        {
        if( items[ i ] == c )
            {
            if( popup_actions[ i ].count() == 0 )
                return;
            if( QAction* a = popups[ i ]->exec( pos ))
                {
                selectEdgeItem( i, popup_actions[ i ].indexOf( a ));
                emit changed();
                emit edgeSelectionChanged( i, popup_actions[ i ].indexOf( a ));
                }
            return;
            }
        }
    abort();
    }

void Monitor::flip( Corner* c, QPoint pos )
    {
    for( int i = 0;
         i < 8;
         ++i )
        {
        if( items[ i ] == c )
            {
            if( popup_actions[ i ].count() == 0 )
                setEdge( i, !edge( i ));
            else
                popup( c, pos );
            return;
            }
        }
    abort();
    }

Monitor::Corner::Corner( Monitor* m )
    : monitor( m )
    {
    }

void Monitor::Corner::contextMenuEvent( QGraphicsSceneContextMenuEvent* e )
    {
    monitor->popup( this, e->screenPos());
    }

void Monitor::Corner::mousePressEvent( QGraphicsSceneMouseEvent* e )
    {
    monitor->flip( this, e->screenPos());
    }

} // namespace

#include "monitor.moc"
