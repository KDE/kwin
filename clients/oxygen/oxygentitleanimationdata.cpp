//////////////////////////////////////////////////////////////////////////////
// oxygentitleanimationdata.h
// -------------------
//
// Copyright (c) 2009 Hugo Pereira Da Costa <hugo.pereira@free.fr>
// Copyright (c) 2003, 2004 David Johnson <david@usermode.org>
// Copyright (c) 2006, 2007 Riccardo Iaconelli <ruphy@fsfe.org>
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

#include "oxygentitleanimationdata.h"
#include "oxygentitleanimationdata.moc"

#include <QtGui/QPainter>

namespace Oxygen
{

    //_________________________________________________________
    TitleAnimationData::TitleAnimationData( QObject* parent ):
        QObject( parent ),
        dirty_( false ),
        animation_( new Animation( 200, this ) ),
        opacity_(0)
    {}

    //_________________________________________________________
    void TitleAnimationData::initialize( void )
    {

        // setup title animation
        animation().data()->setStartValue( 0 );
        animation().data()->setEndValue( 1 );
        animation().data()->setTargetObject( this );
        animation().data()->setPropertyName( "opacity" );
        animation().data()->setCurveShape( Animation::EaseInOutCurve );
        connect( animation().data(), SIGNAL( valueChanged( const QVariant& ) ), SLOT( updatePixmaps( void ) ) );
        connect( animation().data(), SIGNAL( finished( void ) ), SLOT( updatePixmaps( void ) ) );

    }


    //_________________________________________________________
    void TitleAnimationData::setPixmaps( QRect rect, QPixmap pixmap, QPixmap contrast )
    {

        // stop animation
        if( isAnimated() ) animation().data()->stop();

        // update pixmaps
        contrastPixmap_.initialize( rect, contrast );
        pixmap_.initialize( rect, pixmap );

        setOpacity(0);
        updatePixmaps();

    }

    //_________________________________________________________
    void TitleAnimationData::updatePixmaps( void )
    {
        contrastPixmap_.blend( opacity() );
        pixmap_.blend( opacity() );
        emit pixmapsChanged();
    }

    //_________________________________________________________
    void TitleAnimationData::BlendedPixmap::blend( qreal opacity )
    {

        currentPixmap_ = QPixmap( endRect_.size() );
        currentPixmap_.fill( Qt::transparent );

        QPainter painter( &currentPixmap_ );
        if( opacity < 1 && !startPixmap_.isNull() )
        { painter.drawPixmap( startRect_.topLeft() - endRect_.topLeft(), fade( startPixmap_, 1.0 - opacity ) ); }

        if( opacity > 0 && !endPixmap_.isNull() )
        { painter.drawPixmap( QPoint(0,0), fade( endPixmap_, opacity ) ); }

        painter.end();
        return;

    }

    //_________________________________________________________
    QPixmap TitleAnimationData::BlendedPixmap::fade( QPixmap source, qreal opacity ) const
    {

        if( source.isNull() ) return QPixmap();

        QPixmap out( source.size() );
        out.fill( Qt::transparent );

        // do nothing if opacity is too small
        if( opacity*255 < 1 ) return out;

        // draw pixmap
        QPainter p( &out );
        p.drawPixmap( QPoint(0,0), source );

        // opacity mask
        if( opacity*255 <= 254 )
        {
            p.setCompositionMode(QPainter::CompositionMode_DestinationIn);
            QColor color( Qt::black );
            color.setAlphaF( opacity );
            p.fillRect(out.rect(), color );
        }

        p.end();
        return out;

    }

}
