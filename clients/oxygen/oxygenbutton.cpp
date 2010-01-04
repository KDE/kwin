//////////////////////////////////////////////////////////////////////////////
// oxygenbutton.cpp
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
#include "oxygen.h"

#include <cmath>

#include <QtGui/QPainter>
#include <QtGui/QPen>

#include <KColorUtils>
#include <KColorScheme>
#include <kcommondecoration.h>

namespace Oxygen
{
  //_______________________________________________
  OxygenButton::OxygenButton(
    OxygenClient &parent,
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

    // set curve shape. Warning: this is not portable to Qt Kinetic
    glowAnimation().data()->setEasingCurve( QEasingCurve::InOutQuad );

    // setup connections
    connect( glowAnimation().data(), SIGNAL( valueChanged( const QVariant& ) ), SLOT( update( void ) ) );
    connect( glowAnimation().data(), SIGNAL( finished( void ) ), SLOT( update( void ) ) );
    reset(0);

  }

  //_______________________________________________
  OxygenButton::~OxygenButton()
  {}

  //_______________________________________________
  QColor OxygenButton::buttonDetailColor(const QPalette &palette)
  {
    if( client_.glowIsAnimated() && !forceInactive_ && !client_.isForcedActive()) return KColorUtils::mix(
      buttonDetailColor( palette, false ),
      buttonDetailColor( palette, true ),
      client_.glowIntensity() );
    else return buttonDetailColor( palette, isActive() || client_.isForcedActive() );
  }

  //_______________________________________________
  QColor OxygenButton::buttonDetailColor(const QPalette &palette, bool active)
  {

    if( active ) return palette.color(QPalette::Active, QPalette::WindowText);
    else {

      // todo: re-implement caching
      QColor ab = palette.color(QPalette::Active, QPalette::Window);
      QColor af = palette.color(QPalette::Active, QPalette::WindowText);
      QColor nb = palette.color(QPalette::Inactive, QPalette::Window);
      QColor nf = palette.color(QPalette::Inactive, QPalette::WindowText);
      return reduceContrast(nb, nf, qMax(qreal(2.5), KColorUtils::contrastRatio(ab, KColorUtils::mix(ab, af, 0.4))));

    }

  }

  //___________________________________________________
  bool OxygenButton::isActive( void ) const
  { return (!forceInactive_) && client_.isActive(); }

  //___________________________________________________
  QSize OxygenButton::sizeHint() const
  {
    unsigned int size( client_.configuration().buttonSize() );
    return QSize( size, size );
  }

  //___________________________________________________
  void OxygenButton::reset( unsigned long )
  { glowAnimation().data()->setDuration( client_.configuration().animationsDuration() ); }


  //___________________________________________________
  void OxygenButton::enterEvent(QEvent *e)
  {

    KCommonDecorationButton::enterEvent(e);
    if (status_ != Oxygen::Pressed) status_ = Oxygen::Hovered;
    glowAnimation().data()->setDirection( Animation::Forward );
    if( !isAnimated() ) glowAnimation().data()->start();
  }

  //___________________________________________________
  void OxygenButton::leaveEvent(QEvent *e)
  {

    KCommonDecorationButton::leaveEvent(e);

    if( status_ == Oxygen::Hovered )
    {
      glowAnimation().data()->setDirection( Animation::Backward );
      if( !isAnimated() ) glowAnimation().data()->start();
    }

    status_ = Oxygen::Normal;
    update();

  }

  //___________________________________________________
  void OxygenButton::mousePressEvent(QMouseEvent *e)
  {

    status_ = Oxygen::Pressed;
    update();

    KCommonDecorationButton::mousePressEvent(e);
  }

  //___________________________________________________
  void OxygenButton::mouseReleaseEvent(QMouseEvent *e)
  {

    status_ = ( rect().contains( e->pos() ) ) ? Oxygen::Hovered:Oxygen::Normal;
    update();

    KCommonDecorationButton::mouseReleaseEvent(e);
  }

  //___________________________________________________
  void OxygenButton::paintEvent(QPaintEvent *event)
  {

    if( client_.configuration().hideTitleBar() ) return;

    QPainter painter(this);
    painter.setClipRect(this->rect().intersected( event->rect() ) );
    painter.setRenderHints(QPainter::Antialiasing);

    QPalette palette = OxygenButton::palette();
    if( isActive() ) palette.setCurrentColorGroup(QPalette::Active);
    else palette.setCurrentColorGroup(QPalette::Inactive);
    QColor color = palette.window().color();

    if(
      client_.compositingActive() &&
      !( client_.isMaximized() || type_ == ButtonItemClose || type_ == ButtonItemMenu ) )
    { painter.translate( 0, -1 ); }

    // translate buttons down if window maximized
    if( client_.isMaximized() ) painter.translate( 0, 1 );

    // base button color
    QColor bt = ((type_ == ButtonItemClose && forceInactive_ ) ? client_.backgroundPalette( this, palette ):palette).window().color();

    // button icon and glow color depending on glow intensity
    color = (type_ == ButtonItemClose && forceInactive_ ) ?
      buttonDetailColor( client_.backgroundPalette( this, palette ) ):
      buttonDetailColor( palette );

    QColor glow = (type_ == ButtonClose || type_ == ButtonItemClose ) ?
      KColorScheme(palette.currentColorGroup()).foreground(KColorScheme::NegativeText).color():
      KColorScheme(palette.currentColorGroup()).decoration(KColorScheme::HoverColor).color();
    glow = helper_.calcDarkColor( glow );

    if( isAnimated() ) color = KColorUtils::mix( color, glow, glowIntensity() );
    else if( status_ == Oxygen::Hovered ) color = glow;

    // button decoration
    if( type_ != ButtonMenu && type_ != ButtonItemClose && type_ != ButtonItemMenu )
    {

      // draw glow on hover
      if( isAnimated() )
      {

        // mixed shadow and glow for smooth transition
        painter.drawPixmap(0, 0, helper_.windecoButtonGlow( KColorUtils::mix( Qt::black, glow, glowIntensity() ), (21.0*client_.configuration().buttonSize())/22));

      } else if( status_ == Oxygen::Hovered ) {

        // glow only
        painter.drawPixmap(0, 0, helper_.windecoButtonGlow(glow, (21.0*client_.configuration().buttonSize())/22));

      } else {

        // shadow only
        painter.drawPixmap(0, 0, helper_.windecoButtonGlow(Qt::black, (21.0*client_.configuration().buttonSize())/22));

      }

      // draw button shape
      bool pressed( (status_ == Oxygen::Pressed) || ( type_ == ButtonSticky && isChecked() ) );
      painter.drawPixmap(0, 0, helper_.windecoButton(bt, pressed, (21.0*client_.configuration().buttonSize())/22.0 ) );

    }

    // Icon
    // for menu button the application icon is used
    if( type_ == ButtonMenu || type_ == ButtonItemMenu )
    {

      const QPixmap& pixmap( client_.icon().pixmap( client_.configuration().iconScale() ) );
      double offset = 0.5*(width()-pixmap.width() );
      painter.drawPixmap(offset, offset-1, pixmap );

    } else {

      painter.setRenderHints(QPainter::Antialiasing);
      qreal width( 1.2 );

      painter.setBrush(Qt::NoBrush);
      painter.setPen(QPen( helper_.calcLightColor( bt ), width, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
      drawIcon(&painter, palette, type_);

      painter.translate(0,-1.5);
      painter.setBrush(Qt::NoBrush);
      painter.setPen(QPen(color, width, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
      drawIcon(&painter, palette, type_);

    }

  }

  //___________________________________________________
  void OxygenButton::drawIcon(QPainter *painter, QPalette &palette, ButtonType &type)
  {

    painter->save();
    painter->setWindow( 0, 0, 22, 22 );

    QPen newPen = painter->pen();
    switch(type)
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
        case OxygenClient::MaximizeRestore:
        case OxygenClient::MaximizeVertical:
        case OxygenClient::MaximizeHorizontal:
        painter->drawLine(QPointF( 7.5,11.5), QPointF(10.5, 8.5));
        painter->drawLine(QPointF(10.5, 8.5), QPointF(13.5,11.5));
        break;

        case OxygenClient::MaximizeFull:
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
      if(isChecked()) {
        QPen newPen = painter->pen();
        newPen.setColor(KColorScheme(palette.currentColorGroup()).decoration(KColorScheme::HoverColor).color());
        painter->setPen(newPen);
      }

      painter->drawLine(QPointF( 7.5,14), QPointF(10.5,11));
      painter->drawLine(QPointF(10.5,11), QPointF(13.5,14));
      painter->drawLine(QPointF( 7.5,10), QPointF(10.5, 7));
      painter->drawLine(QPointF(10.5, 7), QPointF(13.5,10));
      break;

      case ButtonBelow:
      if(isChecked()) {
        QPen newPen = painter->pen();
        newPen.setColor(KColorScheme(palette.currentColorGroup()).decoration(KColorScheme::HoverColor).color());
        painter->setPen(newPen);
      }

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
