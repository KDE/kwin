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
#include "oxygenclient.h"
#include "oxygen.h"

#include <cmath>

#include <QPainter>
#include <QPen>

#include <KColorUtils>
#include <KColorScheme>
#include <kcommondecoration.h>
#include <KDebug>

namespace Oxygen
{
  //_______________________________________________
  OxygenButton::OxygenButton(OxygenClient &parent,
    const QString& tip, ButtonType type):
    KCommonDecorationButton((::ButtonType)type, &parent),
    client_(parent),
    helper_( parent.helper() ),
    type_(type),
    colorCacheInvalid_(true),
    timeLine_( 200, this )
  {
    setAutoFillBackground(false);
    setAttribute(Qt::WA_NoSystemBackground);

    {
      unsigned int size( client_.configuration().buttonSize() );
      setFixedSize( size, size );
    }

    setCursor(Qt::ArrowCursor);
    setToolTip(tip);

    timeLine_.setFrameRange( 0, 1000 );
    timeLine_.setCurveShape( QTimeLine::EaseInOutCurve );
    connect( &timeLine_, SIGNAL( frameChanged( int ) ), SLOT( update() ) );
    connect( &timeLine_, SIGNAL( finished() ), SLOT( update() ) );

  }

  //_______________________________________________
  OxygenButton::~OxygenButton()
  {}

  //declare function from Oxygenclient.cpp
  QColor reduceContrast(const QColor &c0, const QColor &c1, double t);

  //_______________________________________________
  QColor OxygenButton::buttonDetailColor(const QPalette &palette)
  {

    if (client_.isActive()) return palette.color(QPalette::Active, QPalette::ButtonText);
    else {

      if (colorCacheInvalid_)
      {
        QColor ab = palette.color(QPalette::Active, QPalette::Button);
        QColor af = palette.color(QPalette::Active, QPalette::ButtonText);
        QColor nb = palette.color(QPalette::Inactive, QPalette::Button);
        QColor nf = palette.color(QPalette::Inactive, QPalette::ButtonText);

        colorCacheInvalid_ = false;
        cachedButtonDetailColor_ = reduceContrast(nb, nf, qMax(qreal(2.5), KColorUtils::contrastRatio(ab, KColorUtils::mix(ab, af, 0.4))));
      }

      return cachedButtonDetailColor_;
    }
  }

  //___________________________________________________
  QSize OxygenButton::sizeHint() const
  {
    unsigned int size( client_.configuration().buttonSize() );
    return QSize( size, size );
  }

  //___________________________________________________
  void OxygenButton::enterEvent(QEvent *e)
  {
    KCommonDecorationButton::enterEvent(e);
    if (status_ != Oxygen::Pressed) status_ = Oxygen::Hovered;
    timeLine_.setDirection( QTimeLine::Forward );
    if( timeLine_.state() == QTimeLine::NotRunning ) timeLine_.start();
    update();
  }

  //___________________________________________________
  void OxygenButton::leaveEvent(QEvent *e)
  {
    KCommonDecorationButton::leaveEvent(e);

    if( status_ == Oxygen::Hovered )
    {
      timeLine_.setDirection( QTimeLine::Backward );
      if( timeLine_.state() == QTimeLine::NotRunning ) timeLine_.start();
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
    status_ = Oxygen::Normal;
    update();

    KCommonDecorationButton::mouseReleaseEvent(e);
  }

  //___________________________________________________
  void OxygenButton::paintEvent(QPaintEvent *event)
  {

    QPainter painter(this);
    painter.setClipRect(this->rect().intersected( event->rect() ) );
    painter.setRenderHints(QPainter::Antialiasing);

    QPalette palette = OxygenButton::palette();

    if( client_.isActive() ) palette.setCurrentColorGroup(QPalette::Active);
    else palette.setCurrentColorGroup(QPalette::Inactive);

    // window background
    client_.renderWindowBackground( &painter, rect(), this, palette );

    // window border
    if( client_.drawTitleOutline() && !client_.isMaximized() )
    { client_.renderWindowBorder( &painter, rect(), this, client_.backgroundPalette( this, palette ) ); }

    // colors
    QColor color = palette.window().color();
    QColor light = helper_.calcLightColor( color );
    QColor dark = helper_.calcDarkColor( color );

    dark.setAlpha(120);

    if( client_.drawSeparator() )
    { client_.renderSeparator( &painter, rect(), this, color ); }

    // for menu button the application icon is used
    if (type_ == ButtonMenu)
    {

      const QPixmap& pixmap( client_.icon().pixmap( client_.configuration().iconScale() ) );
      double offset = 0.5*(width()-pixmap.width() );
      painter.drawPixmap(offset, offset-1, pixmap );
      return;
    }

    // calculate color depending on timeline state
    color = buttonDetailColor(palette);
    QColor glow = (type_ == ButtonClose) ?
      KColorScheme(palette.currentColorGroup()).foreground(KColorScheme::NegativeText).color():
      KColorScheme(palette.currentColorGroup()).decoration(KColorScheme::HoverColor).color();

    if( timeLine_.state() == QTimeLine::Running )
    {
      qreal ratio( qreal( timeLine_.currentFrame() )/qreal( timeLine_.endFrame() ) );
      color = KColorUtils::mix( color, glow, ratio );
    } else if( status_ == Oxygen::Hovered ) color = glow;

    // translate buttons up if window maximized
    if(client_.isMaximized())
    { painter.translate( 0, -1 ); }

    // button shape color
    QColor bt = palette.window().color();

    // draw button shape
    painter.drawPixmap(0, 0, helper_.windecoButton(bt, status_ == Oxygen::Pressed, (21.0*client_.configuration().buttonSize())/22 ) );

    // draw glow on hover
    if( timeLine_.state() == QTimeLine::Running )
    {

      qreal ratio( qreal( timeLine_.currentFrame() )/qreal( timeLine_.endFrame() ) );
      painter.save();
      painter.setOpacity( ratio );
      painter.drawPixmap(0, 0, helper_.windecoButtonGlow(glow, (21.0*client_.configuration().buttonSize())/22));
      painter.restore();

    } else if( status_ == Oxygen::Hovered ) {

      painter.drawPixmap(0, 0, helper_.windecoButtonGlow(glow, (21.0*client_.configuration().buttonSize())/22));

    }

    // draw button icon
    if (client_.isActive())
    {

      QLinearGradient lg = helper_.decoGradient( QRect( 4, 4, 13, 13 ), color);
      painter.setRenderHints(QPainter::Antialiasing);
      qreal width( 1.4 );

      painter.setBrush(Qt::NoBrush);
      painter.setPen(QPen(color, width, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
      drawIcon(&painter, palette, type_);

    } else {

      // outlined mode
      QPixmap pixmap(size());
      pixmap.fill(Qt::transparent);
      QPainter pp(&pixmap);
      pp.setRenderHints(QPainter::Antialiasing);
      pp.setBrush(Qt::NoBrush);
      pp.setPen(QPen(color, 3.0, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
      drawIcon(&pp, palette, type_);

      pp.setCompositionMode(QPainter::CompositionMode_DestinationOut);
      pp.setPen(QPen(color, 1.0, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
      drawIcon(&pp, palette, type_);

      painter.drawPixmap(QPoint(0,0), pixmap);

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
      if(isChecked()) {
        newPen.setColor(KColorScheme(palette.currentColorGroup()).decoration(KColorScheme::HoverColor).color());
        painter->setPen(newPen);
      }
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
