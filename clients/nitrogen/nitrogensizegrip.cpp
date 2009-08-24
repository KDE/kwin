// $Id: nitrogensizegrip.cpp,v 1.20 2009/07/05 19:47:57 hpereira Exp $

/******************************************************************************
 *                        
 * Copyright (C) 2002 Hugo PEREIRA <mailto: hugo.pereira@free.fr>            
 *                        
 * This is free software; you can redistribute it and/or modify it under the     
 * terms of the GNU General Public License as published by the Free Software     
 * Foundation; either version 2 of the License, or (at your option) any later   
 * version.                            
 *                         
 * This software is distributed in the hope that it will be useful, but WITHOUT 
 * ANY WARRANTY;  without even the implied warranty of MERCHANTABILITY or         
 * FITNESS FOR A PARTICULAR PURPOSE.   See the GNU General Public License         
 * for more details.                    
 *                         
 * You should have received a copy of the GNU General Public License along with 
 * software; if not, write to the Free Software Foundation, Inc., 59 Temple     
 * Place, Suite 330, Boston, MA   02111-1307 USA                          
 *                        
 *                        
 *******************************************************************************/

#include <cassert>
#include <QApplication>
#include <QPainter>
#include <QPolygon>
#include <QTextStream>
#include <QTimer>

#include <kdeversion.h>

#include "nitrogenbutton.h"
#include "nitrogenclient.h"
#include "nitrogensizegrip.h"
#include "x11util.h"

#include <QX11Info>
#include <X11/Xlib.h>

namespace Nitrogen
{
  
  //_____________________________________________
  NitrogenSizeGrip::NitrogenSizeGrip( NitrogenClient* client ):
    QWidget( client->widget() ),
    client_( client ),
    decoration_offset_( false )
  { 

    // cursor
    setCursor( Qt::SizeFDiagCursor );
    setAutoFillBackground( true );

    // size
    setFixedSize( QSize( GRIP_SIZE, GRIP_SIZE ) );
    
    // mask
    QPolygon p;
    p << QPoint( 0, GRIP_SIZE )
      << QPoint( GRIP_SIZE, 0 )
      << QPoint( GRIP_SIZE, GRIP_SIZE )
      << QPoint( 0, GRIP_SIZE );
    
    setMask( QRegion( p ) );
    
    // embed
    embed();
    updateBackgroundColor();
    updatePosition();
    
    // event filter
    client->widget()->installEventFilter( this );

    // show
    show();

  }
  //_____________________________________________
  NitrogenSizeGrip::~NitrogenSizeGrip( void )
  {}
  
  //_____________________________________________
  void NitrogenSizeGrip::updateBackgroundColor( void )
  {
    
    QPalette palette = client().widget()->palette();
    
    // active color
    palette.setCurrentColorGroup( client().isActive() ? QPalette::Active:QPalette::Inactive );
      
    // get relevant colors
    QColor base( client().configuration().overwriteColors() ? 
      palette.button().color().darker(250) : 
      client().options()->color( NitrogenClient::ColorTitleBar, client().isActive() ) );
    
    palette.setColor( backgroundRole(), base );
    setPalette( palette );
       
    XMapRaised( QX11Info::display(), winId() ); 
    
  }
  
  //_____________________________________________
  void NitrogenSizeGrip::embed( void )
  {
    
    WId window_id = client().windowId();
    assert( window_id );
    
    WId current = window_id;
    while( true )
    {
      WId root, parent = 0;
      WId *children = 0L;
      uint child_count = 0;
      XQueryTree(QX11Info::display(), current, &root, &parent, &children, &child_count);
      if( parent && parent != root && parent != current ) current = parent;
      else break;
    }
    
    // if the current window is the window_id 
    // (i.e. if the client is top-level) 
    // the decoration_offset_ flag is set to true, meaning that decoration borders
    // are taken into account when placing the widget.
    #if KDE_IS_VERSION(4,2,92)
    decoration_offset_ = ( current == window_id );
    #else
    decoration_offset_ = NitrogenConfiguration::useCompiz();
    #endif
    
    // reparent
    XReparentWindow( QX11Info::display(), winId(), current, 0, 0 );
    
  }
  
  //_____________________________________________
  bool NitrogenSizeGrip::eventFilter( QObject* object, QEvent* event )
  {
    
    if( object != client().widget() ) return false;
    if ( event->type() == QEvent::Resize) updatePosition();
    return false;
    
  }
  
  //_____________________________________________
  void NitrogenSizeGrip::paintEvent( QPaintEvent* )
  {
    
    QPalette palette = client().widget()->palette();
    palette.setCurrentColorGroup( (client().isActive() ) ? QPalette::Active : QPalette::Inactive );
    
    // get relevant colors
    QColor base( client().configuration().overwriteColors() ? 
      palette.button().color() : 
      client().options()->color( NitrogenClient::ColorTitleBar, client().isActive() ) );
    
    QColor dark( base.darker(250) );
    QColor light( base.darker(150) );

    // create and configure painter
    QPainter painter(this);
    painter.setRenderHints(QPainter::Antialiasing );
        
    painter.setPen( Qt::NoPen );
    painter.setBrush( base );

    // polygon
    QPolygon p;
    p << QPoint( 0, GRIP_SIZE )
      << QPoint( GRIP_SIZE, 0 )
      << QPoint( GRIP_SIZE, GRIP_SIZE )
      << QPoint( 0, GRIP_SIZE );
    painter.drawPolygon( p );
    
    // diagonal border
    painter.setBrush( Qt::NoBrush );
    painter.setPen( QPen( dark, 3 ) );
    painter.drawLine( QPoint( 0, GRIP_SIZE ), QPoint( GRIP_SIZE, 0 ) );
    
    // side borders
    painter.setPen( QPen( light, 1.5 ) );
    painter.drawLine( QPoint( 1, GRIP_SIZE ), QPoint( GRIP_SIZE, GRIP_SIZE ) );
    painter.drawLine( QPoint( GRIP_SIZE, 1 ), QPoint( GRIP_SIZE, GRIP_SIZE ) );
    painter.end();
    
  }
  
  //_____________________________________________
  void NitrogenSizeGrip::mousePressEvent( QMouseEvent* event )
  {

    switch (event->button())
    {
      
      case Qt::RightButton:
      {
        hide(); 
        QTimer::singleShot(5000, this, SLOT(show()));
        break;
      }
      
      case Qt::MidButton:
      {
        hide();
        break;
      }
      
      case Qt::LeftButton:
      if( rect().contains( event->pos() ) )
      {
        
        // check client window id
        if( !client().windowId() ) break;
        
        // get matching screen
        int screen( client().widget()->x11Info().screen() );
        
        client().widget()->setFocus();
        
        // post event
        X11Util::get().moveResizeWidget( client().windowId(), screen, event->globalPos(), X11Util::_NET_WM_MOVERESIZE_SIZE_BOTTOMRIGHT );
        
      }
      break;
      
      default: break;
      
    }
    
    return;
    
  }
  
  //_______________________________________________________________________________
  void NitrogenSizeGrip::updatePosition( void )
  {
    
    QPoint position( 
      client().width() - GRIP_SIZE - OFFSET,
      client().height() - GRIP_SIZE - OFFSET );
    
    if( decoration_offset_ ) 
    { position-= QPoint( client().borderWidth(), client().borderHeight() ); }
    
    #if KDE_IS_VERSION(4,2,92)
    else { 
        position -= QPoint( 
            client().layoutMetric( NitrogenClient::LM_BorderRight ),
            client().layoutMetric( NitrogenClient::LM_BorderBottom ) );
    }
    #endif
    
    move( position );

  }
  
}
