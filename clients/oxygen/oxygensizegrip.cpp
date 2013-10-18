//////////////////////////////////////////////////////////////////////////////
// oxygensizegrip.cpp
// bottom right size grip for borderless windows
// -------------------
//
// Copyright (c) 2009 Hugo Pereira Da Costa <hugo.pereira@free.fr>
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to
// deal in the Software without restriction, including without limitation the
// rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
// sell copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
// FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
// IN THE SOFTWARE.
//////////////////////////////////////////////////////////////////////////////


#include "oxygensizegrip.h"
#include "oxygenbutton.h"
#include "oxygenclient.h"

#include <cassert>
#include <QPainter>
#include <QPolygon>
#include <QTimer>

#include <xcb/xcb.h>

namespace Oxygen
{

    //_____________________________________________
    SizeGrip::SizeGrip( Client* client ):
        QWidget(0),
        _client( client )
    {

        setAttribute(Qt::WA_NoSystemBackground );
        setAutoFillBackground( false );

        // cursor
        setCursor( Qt::SizeFDiagCursor );

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
        updatePosition();

        // event filter
        client->widget()->installEventFilter( this );

        // show
        show();

    }

    //_____________________________________________
    SizeGrip::~SizeGrip( void )
    {}

    //_____________________________________________
    void SizeGrip::activeChange( void )
    {
        static const uint32_t value = XCB_STACK_MODE_ABOVE;
        xcb_configure_window( _client->helper().xcbConnection(), winId(), XCB_CONFIG_WINDOW_STACK_MODE, &value );
        xcb_map_window( _client->helper().xcbConnection(), winId() );
    }

    //_____________________________________________
    void SizeGrip::embed( void )
    {
        xcb_window_t window_id = _client->windowId();
        if( _client->isPreview() ) {

            setParent( _client->widget() );

        } else if( window_id ) {

            xcb_window_t current = window_id;
            xcb_connection_t* connection = _client->helper().xcbConnection();
            while( true )
            {
                xcb_query_tree_cookie_t cookie = xcb_query_tree_unchecked( connection, current );
                QScopedPointer<xcb_query_tree_reply_t, QScopedPointerPodDeleter> tree(xcb_query_tree_reply( connection, cookie, nullptr ) );
                if (tree.isNull()) break;

                if (tree->parent && tree->parent != tree->root && tree->parent != current) current = tree->parent;
                else break;
            }

            // reparent
            xcb_reparent_window( connection, winId(), current, 0, 0 );

        } else {

            hide();

        }
    }

    //_____________________________________________
    bool SizeGrip::eventFilter( QObject*, QEvent* event )
    {

        if ( event->type() == QEvent::Resize) updatePosition();
        return false;

    }

    //_____________________________________________
    void SizeGrip::paintEvent( QPaintEvent* )
    {

        // get relevant colors
        QColor base( _client->backgroundColor( this, palette(), _client->isActive() ) );
        QColor light( _client->helper().calcDarkColor( base ) );
        QColor dark( _client->helper().calcDarkColor( base.darker(150) ) );

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
    void SizeGrip::mousePressEvent( QMouseEvent* event )
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
                if( !_client->windowId() ) break;
                _client->widget()->setFocus();
                if( _client->decoration() )
                { _client->decoration()->performWindowOperation( KDecorationDefines::ResizeOp ); }

            }
            break;

            default: break;

        }

        return;

    }

    //_______________________________________________________________________________
    void SizeGrip::updatePosition( void )
    {

        QPoint position(
            _client->width() - GRIP_SIZE - OFFSET,
            _client->height() - GRIP_SIZE - OFFSET );

        if( _client->isPreview() )
        {

            position -= QPoint(
                _client->layoutMetric( Client::LM_BorderRight )+
                _client->layoutMetric( Client::LM_OuterPaddingRight ),
                _client->layoutMetric( Client::LM_OuterPaddingBottom )+
                _client->layoutMetric( Client::LM_BorderBottom )
                );

        } else {

            position -= QPoint(
                _client->layoutMetric( Client::LM_BorderRight ),
                _client->layoutMetric( Client::LM_BorderBottom ) );
        }

        move( position );

    }

}
