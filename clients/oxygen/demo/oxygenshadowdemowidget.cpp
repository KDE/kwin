//////////////////////////////////////////////////////////////////////////////
// oxygenshadowdemowidget.h
// shadow demo widget
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

#include "oxygenshadowdemowidget.h"
#include "oxygenshadowdemowidget.moc"

#include <QtGui/QPainter>
namespace Oxygen
{

    //______________________________________
    void ShadowDemoWidget::paintEvent( QPaintEvent* event )
    {

        if( !_tileSet.isValid() ) return;

        QPainter painter( this );
        painter.setClipRegion( event->region() );
        _tileSet.render( rect(), &painter );

        // draw background
        if( _drawBackground )
        {
            updateBackgroundPixmap();
            painter.translate( _shadowSize, _shadowSize  );
            painter.drawPixmap( QPoint(0,0), _backgroundPixmap );
        }

    }

    //______________________________________
    void ShadowDemoWidget::updateBackgroundPixmap( void )
    {

        // check if background pixmap needs update
        QRect backgroundRect( QPoint( 0, 0 ), size() - QSize( 2*_shadowSize, 2*_shadowSize )  );
        if( !_backgroundPixmap.isNull() && _backgroundPixmap.size() == backgroundRect.size() )
        { return; }

        _backgroundPixmap = QPixmap( backgroundRect.size() );
        _backgroundPixmap.fill( Qt::transparent );
        QPainter painter( &_backgroundPixmap );
        painter.setRenderHint( QPainter::Antialiasing );

        _dummy.resize( backgroundRect.size() );
        helper().renderWindowBackground(
            &painter, backgroundRect,
            &_dummy, &_dummy, palette().color( QPalette::Window ), 0, 64 );

        // float frame
        helper().drawFloatFrame( &painter, backgroundRect.adjusted( -1, -1, 1, 1 ), palette().color( QPalette::Window ), false );

        // add rounded mask
        painter.save();
        painter.setCompositionMode( QPainter::CompositionMode_DestinationIn );
        painter.setBrush( Qt::black );
        painter.setPen( Qt::NoPen );
        if( _square )
        {
            QRectF rectF( backgroundRect );
            QPainterPath path;

            // rotate counterclockwise, cause that makes angles easier
            path.moveTo( rectF.topLeft() + QPointF( 3.5, 0 ) );
            path.arcTo( QRectF( rectF.topLeft(), QSize( 7, 7 ) ), 90, 90 );
            path.lineTo( rectF.bottomLeft() );
            path.lineTo( rectF.bottomRight() );
            path.lineTo( rectF.topRight() + QPointF( 0, 3.5 ) );
            path.arcTo( QRectF( rectF.topRight() + QPointF( -7, 0 ), QSize( 7, 7 ) ), 0, 90 );
            path.lineTo( rectF.topLeft() + QPointF( 3.5, 0 ) );
            painter.drawPath( path );

        } else {

            painter.drawRoundedRect( QRectF( backgroundRect ), 3.5, 3.5 );

        }

        painter.restore();
    }

}
