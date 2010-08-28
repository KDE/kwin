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
        client_(parent),
        helper_( parent.helper() ),
        type_(type),
        forceInactive_( false ),
        glowAnimation_( new Animation( 150, this ) ),
        glowIntensity_(0)
    {
        setAutoFillBackground(false);
        setAttribute(Qt::WA_NoSystemBackground);

        {
            unsigned int size( client_.configuration().buttonSize() );
            setFixedSize( size, size );
        }

        setCursor(Qt::ArrowCursor);
        setToolTip(tip);

        // setup animation
        glowAnimation().data()->setStartValue( 0 );
        glowAnimation().data()->setEndValue( 1.0 );
        glowAnimation().data()->setTargetObject( this );
        glowAnimation().data()->setPropertyName( "glowIntensity" );
        glowAnimation().data()->setEasingCurve( QEasingCurve::InOutQuad );

        // setup connections
        reset(0);

    }

    //_______________________________________________
    Button::~Button()
    {}

    //_______________________________________________
    QColor Button::buttonDetailColor(const QPalette &palette) const
    {
        if( client_.glowIsAnimated() && !forceInactive_ && !client_.isForcedActive()) return KColorUtils::mix(
            buttonDetailColor( palette, false ),
            buttonDetailColor( palette, true ),
            client_.glowIntensity() );
        else return buttonDetailColor( palette, isActive() || client_.isForcedActive() );
    }

    //___________________________________________________
    bool Button::isActive( void ) const
    { return (!forceInactive_) && client_.isActive(); }

    //___________________________________________________
    bool Button::animateButtonHover( void ) const
    { return client_.useAnimations(); }

    //___________________________________________________
    QSize Button::sizeHint() const
    {
        unsigned int size( client_.configuration().buttonSize() );
        return QSize( size, size );
    }

    //___________________________________________________
    void Button::reset( unsigned long )
    { glowAnimation().data()->setDuration( client_.configuration().animationsDuration() ); }


    //___________________________________________________
    void Button::enterEvent(QEvent *e)
    {

        KCommonDecorationButton::enterEvent(e);
        if (status_ != Oxygen::Pressed) status_ = Oxygen::Hovered;
        if( animateButtonHover() )
        {

            glowAnimation().data()->setDirection( Animation::Forward );
            if( !isAnimated() ) glowAnimation().data()->start();

        } else update();

    }

    //___________________________________________________
    void Button::leaveEvent(QEvent *e)
    {

        KCommonDecorationButton::leaveEvent(e);

        if( status_ == Oxygen::Hovered && animateButtonHover() )
        {
            glowAnimation().data()->setDirection( Animation::Backward );
            if( !isAnimated() ) glowAnimation().data()->start();
        }

        status_ = Oxygen::Normal;
        update();

    }

    //___________________________________________________
    void Button::mousePressEvent(QMouseEvent *e)
    {

        if( type_ == ButtonMax || e->button() == Qt::LeftButton )
        {
            status_ = Oxygen::Pressed;
            update();
        }

        KCommonDecorationButton::mousePressEvent(e);
    }

    //___________________________________________________
    void Button::mouseReleaseEvent(QMouseEvent *e)
    {

        status_ = ( rect().contains( e->pos() ) ) ? Oxygen::Hovered:Oxygen::Normal;
        update();

        KCommonDecorationButton::mouseReleaseEvent(e);
    }

    //___________________________________________________
    void Button::paintEvent(QPaintEvent *event)
    {

        if( client_.configuration().hideTitleBar() ) return;

        QPainter painter(this);
        painter.setClipRect(this->rect().intersected( event->rect() ) );
        painter.setRenderHints(QPainter::Antialiasing);

        QPalette palette( this->palette() );
        palette.setCurrentColorGroup( isActive() ? QPalette::Active : QPalette::Inactive);

        if(
            client_.compositingActive() &&
            !( client_.isMaximized() || type_ == ButtonItemClose || type_ == ButtonItemMenu ) )
        { painter.translate( 0, -1 ); }

        // translate buttons down if window maximized
        if( client_.isMaximized() ) painter.translate( 0, 1 );

        // base button color
        const QColor bt = ((type_ == ButtonItemClose && forceInactive_ ) ? client_.backgroundPalette( this, palette ):palette).window().color();

        // button icon and glow color depending on glow intensity
        QColor color = (type_ == ButtonItemClose && forceInactive_ ) ?
            buttonDetailColor( client_.backgroundPalette( this, palette ) ):
            buttonDetailColor( palette );

        QColor glow = isCloseButton() ?
            helper_.viewNegativeTextBrush().brush(palette).color():
            helper_.viewHoverBrush().brush(palette).color();

        glow = helper_.calcDarkColor( glow );

        // decide decoration color
        if( isAnimated() ) color = KColorUtils::mix( color, glow, glowIntensity() );
        else if( status_ == Oxygen::Hovered  ) color = glow;

        if( hasDecoration() )
        {

            // decide shadow color
            QColor shadow;
            if( isAnimated() ) shadow = KColorUtils::mix( Qt::black, glow, glowIntensity() );
            else if( status_ == Oxygen::Hovered ) shadow = glow;
            else shadow = Qt::black;

            qreal scale( (21.0*client_.configuration().buttonSize())/22.0 );

            // draw shadow
            painter.drawPixmap( 0, 0, helper_.windecoButtonGlow( shadow, scale ) );

            // draw button shape
            const bool pressed( (status_ == Oxygen::Pressed) ||
                ( isChecked() && isToggleButton() ) );

            painter.drawPixmap(0, 0, helper_.windecoButton(bt, pressed, scale ) );

        }

        // Icon
        // for menu button the application icon is used
        if( isMenuButton() )
        {

            const QPixmap& pixmap( client_.icon().pixmap( client_.configuration().iconScale() ) );
            const double offset = 0.5*(width()-pixmap.width() );
            painter.drawPixmap(offset, offset-1, pixmap );

        } else {

            painter.setRenderHints(QPainter::Antialiasing);
            qreal width( 1.2 );

            painter.setBrush(Qt::NoBrush);
            painter.setPen(QPen( helper_.calcLightColor( bt ), width, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
            drawIcon(&painter);

            painter.translate(0,-1.5);
            painter.setBrush(Qt::NoBrush);
            painter.setPen(QPen(color, width, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
            drawIcon(&painter);

        }

    }

    //___________________________________________________
    void Button::drawIcon(QPainter *painter)
    {

        painter->save();
        painter->setWindow( 0, 0, 22, 22 );

        switch(type_)
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
            switch(client_.maximizeMode())
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
