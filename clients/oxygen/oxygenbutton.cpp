//////////////////////////////////////////////////////////////////////////////
// Button.cpp
// -------------------
//
// Copyright (c) 2009 Hugo Pereira Da Costa <hugo.pereira@free.fr>
// Copyright (c) 2006, 2007 Riccardo Iaconelli <riccardo@kde.org>
// Copyright (c) 2006, 2007 Casper Boemann <cbr@boemann.dk>
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

#include "oxygenbutton.h"
#include "oxygenbutton.moc"
#include "oxygenclient.h"

#include <cmath>

#include <QtGui/QPainter>
#include <QtGui/QPen>

#include <KColorUtils>
#include <KColorScheme>
#include <kcommondecoration.h>

namespace Oxygen
{
    //_______________________________________________
    Button::Button(
        Client &parent,
        const QString& tip,
        ButtonType type):
        KCommonDecorationButton((::ButtonType)type, &parent),
        _client(parent),
        _helper( parent.helper() ),
        _type(type),
        _status( 0 ),
        _forceInactive( false ),
        _glowAnimation( new Animation( 150, this ) ),
        _glowIntensity(0)
    {
        setAutoFillBackground(false);
        setAttribute(Qt::WA_NoSystemBackground);

        unsigned int size( _client.configuration().buttonSize() );
        setFixedSize( size, size );

        setCursor(Qt::ArrowCursor);
        setToolTip(tip);

        // setup animation
        _glowAnimation->setStartValue( 0 );
        _glowAnimation->setEndValue( 1.0 );
        _glowAnimation->setTargetObject( this );
        _glowAnimation->setPropertyName( "glowIntensity" );
        _glowAnimation->setEasingCurve( QEasingCurve::InOutQuad );

        // setup connections
        reset(0);


    }

    //_______________________________________________
    Button::~Button()
    {}

    //_______________________________________________
    QColor Button::buttonDetailColor(const QPalette &palette) const
    {
        if( _client.glowIsAnimated() && !_forceInactive && !_client.isForcedActive()) return KColorUtils::mix(
            buttonDetailColor( palette, false ),
            buttonDetailColor( palette, true ),
            _client.glowIntensity() );
        else return buttonDetailColor( palette, isActive() || _client.isForcedActive() );
    }

    //___________________________________________________
    bool Button::isActive( void ) const
    { return (!_forceInactive) && _client.isActive(); }

    //___________________________________________________
    bool Button::buttonAnimationsEnabled( void ) const
    { return _client.animationsEnabled() && _client.configuration().buttonAnimationsEnabled(); }

    //___________________________________________________
    QSize Button::sizeHint() const
    {
        unsigned int size( _client.configuration().buttonSize() );
        return QSize( size, size );
    }

    //___________________________________________________
    void Button::reset( unsigned long )
    { _glowAnimation->setDuration( _client.configuration().buttonAnimationsDuration() ); }

    //___________________________________________________
    void Button::paint( QPainter& painter )
    {

        QPalette palette( this->palette() );
        palette.setCurrentColorGroup( isActive() ? QPalette::Active : QPalette::Inactive);

        if(
            _client.compositingActive() &&
            !( _client.isMaximized() || _type == ButtonItemClose || _type == ButtonItemMenu ) )
        { painter.translate( 0, -1 ); }

        // translate buttons down if window maximized
        if( _client.isMaximized() ) painter.translate( 0, 1 );

        // base button color
        QColor base;
        if( _type == ButtonItemClose && _forceInactive ) base = _client.backgroundPalette( this, palette ).window().color();
        else if( _type == ButtonItemClose ) base = palette.window().color();
        else base = palette.button().color();

        // text color
        QColor color = (_type == ButtonItemClose && _forceInactive ) ?
            buttonDetailColor( _client.backgroundPalette( this, palette ) ):
            buttonDetailColor( palette );

        // decide decoration color
        QColor glow;
        if( isAnimated() || (_status&Hovered) )
        {
            glow = isCloseButton() ?
                _helper.viewNegativeTextBrush().brush(palette).color():
                _helper.viewHoverBrush().brush(palette).color();

            if( isAnimated() )
            {

                color = KColorUtils::mix( color, glow, glowIntensity() );
                glow = _helper.alphaColor( glow, glowIntensity() );

            } else if( _status&Hovered  ) color = glow;

        }

        if( hasDecoration() )
        {
            // scale
            qreal scale( (21.0*_client.configuration().buttonSize())/22.0 );

            // pressed state
            const bool pressed(
                (_status&Pressed) ||
                ( _type == ButtonSticky && _client.isOnAllDesktops()  ) ||
                ( _type == ButtonAbove && _client.keepAbove() ) ||
                ( _type == ButtonBelow && _client.keepBelow() ) );

            // draw button shape
            painter.drawPixmap(0, 0, _helper.windecoButton( base, glow, pressed, scale ) );

        }

        // Icon
        // for menu button the application icon is used
        if( isMenuButton() )
        {

            const QPixmap& pixmap( _client.icon().pixmap( _client.configuration().iconScale() ) );
            const double offset = 0.5*(width()-pixmap.width() );
            painter.drawPixmap(offset, offset-1, pixmap );

        } else {

            painter.setRenderHints(QPainter::Antialiasing);
            qreal width( 1.2 );

            // contrast
            painter.setBrush(Qt::NoBrush);
            painter.setPen(QPen( _helper.calcLightColor( base ), width, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
            drawIcon(&painter);

            // main
            painter.translate(0,-1.5);
            painter.setBrush(Qt::NoBrush);
            painter.setPen(QPen(color, width, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
            drawIcon(&painter);

        }

    }

    //___________________________________________________
    void Button::mousePressEvent( QMouseEvent *event )
    {

        if( _type == ButtonMax || event->button() == Qt::LeftButton )
        {
            _status |= Pressed;
            parentUpdate();
        }

        KCommonDecorationButton::mousePressEvent( event );
    }

    //___________________________________________________
    void Button::mouseReleaseEvent( QMouseEvent* event )
    {

        _status &= ~Pressed;
        parentUpdate();

        KCommonDecorationButton::mouseReleaseEvent( event );
    }

    //___________________________________________________
    void Button::enterEvent( QEvent *event )
    {

        KCommonDecorationButton::enterEvent( event );
        _status |= Hovered;

        if( buttonAnimationsEnabled() )
        {

            _glowAnimation->setDirection( Animation::Forward );
            if( !isAnimated() ) _glowAnimation->start();

        } else parentUpdate();

    }

    //___________________________________________________
    void Button::leaveEvent( QEvent *event )
    {

        KCommonDecorationButton::leaveEvent( event );

        if( _status&Hovered && buttonAnimationsEnabled() )
        {
            _glowAnimation->setDirection( Animation::Backward );
            if( !isAnimated() ) _glowAnimation->start();
        }

        _status &= ~Hovered;
        parentUpdate();

    }

    //___________________________________________________
    void Button::resizeEvent( QResizeEvent *event )
    {

        // resize backing store pixmap
        if( !_client.compositingActive() )
        { _pixmap = QPixmap( event->size() ); }

        // base class implementation
        KCommonDecorationButton::resizeEvent( event );

    }

    //___________________________________________________
    void Button::paintEvent(QPaintEvent *event)
    {

        if( _client.hideTitleBar() ) return;

        /*
        do nothing in compositing mode.
        painting is performed by the parent widget
        */
        if( !_client.compositingActive() )
        {

            {

                // create painter
                QPainter painter( &_pixmap );
                painter.setRenderHints(QPainter::Antialiasing);
                painter.setClipRect( this->rect().intersected( event->rect() ) );

                // render parent background
                painter.translate( -geometry().topLeft() );
                _client.paintBackground( painter );

                // render buttons
                painter.translate( geometry().topLeft() );
                paint( painter );

            }

            QPainter painter(this);
            painter.setClipRegion( event->region() );
            painter.drawPixmap( QPoint(), _pixmap );

        }

    }

    //___________________________________________________
    void Button::parentUpdate( void )
    {

        if( _client.compositingActive() && parentWidget() ) parentWidget()->update( geometry().adjusted( -1, -1, 1, 1 ) );
        else this->update();

    }

    //___________________________________________________
    void Button::drawIcon( QPainter* painter )
    {

        painter->save();
        painter->setWindow( 0, 0, 22, 22 );

        switch(_type)
        {

            case ButtonSticky:
            painter->drawPoint(QPointF(10.5,10.5));
            break;

            case ButtonHelp:
            painter->translate(1.5, 1.5);
            painter->drawArc(7,5,4,4,135*16, -180*16);
            painter->drawArc(9,8,4,4,135*16,45*16);
            painter->drawPoint(9,12);
            painter->translate(-1.5, -1.5);
            break;

            case ButtonMin:
            painter->drawLine(QPointF( 7.5, 9.5), QPointF(10.5,12.5));
            painter->drawLine(QPointF(10.5,12.5), QPointF(13.5, 9.5));
            break;

            case ButtonMax:
            switch(_client.maximizeMode())
            {
                case Client::MaximizeRestore:
                case Client::MaximizeVertical:
                case Client::MaximizeHorizontal:
                painter->drawLine(QPointF( 7.5,11.5), QPointF(10.5, 8.5));
                painter->drawLine(QPointF(10.5, 8.5), QPointF(13.5,11.5));
                break;

                case Client::MaximizeFull:
                {
                    painter->translate(1.5, 1.5);
                    QPoint points[4] = {QPoint(9, 6), QPoint(12, 9), QPoint(9, 12), QPoint(6, 9)};
                    painter->drawPolygon(points, 4);
                    painter->translate(-1.5, -1.5);
                    break;
                }
            }
            break;

            case ButtonItemClose:
            case ButtonClose:
            painter->drawLine(QPointF( 7.5,7.5), QPointF(13.5,13.5));
            painter->drawLine(QPointF(13.5,7.5), QPointF( 7.5,13.5));
            break;

            case ButtonAbove:
            painter->drawLine(QPointF( 7.5,14), QPointF(10.5,11));
            painter->drawLine(QPointF(10.5,11), QPointF(13.5,14));
            painter->drawLine(QPointF( 7.5,10), QPointF(10.5, 7));
            painter->drawLine(QPointF(10.5, 7), QPointF(13.5,10));
            break;

            case ButtonBelow:
            painter->drawLine(QPointF( 7.5,11), QPointF(10.5,14));
            painter->drawLine(QPointF(10.5,14), QPointF(13.5,11));
            painter->drawLine(QPointF( 7.5, 7), QPointF(10.5,10));
            painter->drawLine(QPointF(10.5,10), QPointF(13.5, 7));
            break;

            case ButtonShade:
            if (!isChecked())
            {

                // shade button
                painter->drawLine(QPointF( 7.5, 7.5), QPointF(10.5,10.5));
                painter->drawLine(QPointF(10.5,10.5), QPointF(13.5, 7.5));
                painter->drawLine(QPointF( 7.5,13.0), QPointF(13.5,13.0));

            } else {

                // unshade button
                painter->drawLine(QPointF( 7.5,10.5), QPointF(10.5, 7.5));
                painter->drawLine(QPointF(10.5, 7.5), QPointF(13.5,10.5));
                painter->drawLine(QPointF( 7.5,13.0), QPointF(13.5,13.0));

            }
            break;

            default:
            break;
        }
        painter->restore();
        return;
    }

}
