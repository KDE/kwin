//////////////////////////////////////////////////////////////////////////////
// oxygentitleanimationdata.h
// handles transition when window title is changed
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

#include "oxygentitleanimationdata.h"
#include "oxygentitleanimationdata.moc"

#include <QPainter>

namespace Oxygen
{

    // use 300 milliseconds for animation lock
    const int TitleAnimationData::_lockTime = 300;

    //_________________________________________________________
    TitleAnimationData::TitleAnimationData( QObject* parent ):
        QObject( parent ),
        _dirty( false ),
        _animation( new Animation( 200, this ) ),
        _opacity(0)
    {}

    //_________________________________________________________
    void TitleAnimationData::initialize( void )
    {

        // setup title animation
        animation().data()->setStartValue( 0 );
        animation().data()->setEndValue( 1 );
        animation().data()->setTargetObject( this );
        animation().data()->setPropertyName( "opacity" );
        animation().data()->setEasingCurve( QEasingCurve::InOutQuad );

    }


    //_________________________________________________________
    void TitleAnimationData::setPixmaps( QRect rect, QPixmap pixmap, QPixmap contrast )
    {

        // stop animation
        if( isAnimated() ) animation().data()->stop();

        // update pixmaps
        _contrastPixmap.initialize( rect, contrast );
        _pixmap.initialize( rect, pixmap );

        setOpacity(0);
        updatePixmaps();

    }

    //_________________________________________________________
    void TitleAnimationData::updatePixmaps( void )
    {
        _contrastPixmap.blend( opacity() );
        _pixmap.blend( opacity() );
        emit pixmapsChanged();
    }

    //_________________________________________________________
    void TitleAnimationData::timerEvent( QTimerEvent* e )
    {

        if( e->timerId() != _animationLockTimer.timerId() )
        { return QObject::timerEvent( e ); }

        // unlock
        unlockAnimations();

        if( !isAnimated() )
        {
            // triggers pixmap updates
            reset();
            emit pixmapsChanged();
        }

    }

    //_________________________________________________________
    void TitleAnimationData::BlendedPixmap::blend( qreal opacity )
    {

        _currentPixmap = QPixmap( _endRect.size() );
        _currentPixmap.fill( Qt::transparent );

        QPainter painter( &_currentPixmap );
        if( opacity < 1 && !_startPixmap.isNull() )
        { painter.drawPixmap( _startRect.topLeft() - _endRect.topLeft(), fade( _startPixmap, 1.0 - opacity ) ); }

        if( opacity > 0 && !_endPixmap.isNull() )
        { painter.drawPixmap( QPoint(0,0), fade( _endPixmap, opacity ) ); }

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
