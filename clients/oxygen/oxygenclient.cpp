//////////////////////////////////////////////////////////////////////////////
// oxygenclient.cpp
// -------------------
//
// Copyright (c) 2009 Hugo Pereira Da Costa <hugo.pereira@free.fr>
// Copyright (c) 2006, 2007 Casper Boemann <cbr@boemann.dk>
// Copyright (c) 2006, 2007 Riccardo Iaconelli <riccardo@kde.org>
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

#include "oxygenclient.h"
#include "oxygenclient.moc"

#include "oxygen.h"
#include "oxygenbutton.h"
#include "oxygensizegrip.h"
#include "lib/tileset.h"

#include <cassert>
#include <cmath>

#include <kdeversion.h>
#include <KLocale>
#include <KColorUtils>
#include <KDebug>

#include <QtGui/QLabel>
#include <QtGui/QPainter>
#include <QtGui/QApplication>

namespace Oxygen
{

  //___________________________________________
  void renderDot(QPainter *p, const QPointF &point, qreal diameter)
  {
    p->drawEllipse(QRectF(point.x()-diameter/2, point.y()-diameter/2, diameter, diameter));
  }

  //_________________________________________________________
  QColor reduceContrast(const QColor &c0, const QColor &c1, double t)
  {
    double s = KColorUtils::contrastRatio(c0, c1);
    if (s < t)
      return c1;

    double l = 0.0, h = 1.0;
    double x = s, a;
    QColor r = c1;
    for (int maxiter = 16; maxiter; --maxiter)
    {

      a = 0.5 * (l + h);
      r = KColorUtils::mix(c0, c1, a);
      x = KColorUtils::contrastRatio(c0, r);

      if (fabs(x - t) < 0.01) break;
      if (x > t) h = a;
      else l = a;
    }

    return r;
  }

  //___________________________________________
  OxygenClient::OxygenClient(KDecorationBridge *b, OxygenFactory *f):
    KCommonDecorationUnstable(b, f),
    factory_( f ),
    sizeGrip_( 0 ),
    timeLine_( 200, this ),
    titleTimeLine_( 200, this ),
    initialized_( false )
  {}

  //___________________________________________
  OxygenClient::~OxygenClient()
  {

    // delete sizegrip if any
    if( hasSizeGrip() ) deleteSizeGrip();

  }

  //___________________________________________
  QString OxygenClient::visibleName() const
  { return i18n("Oxygen"); }

  //___________________________________________
  void OxygenClient::init()
  {

    KCommonDecoration::init();
    widget()->setAttribute(Qt::WA_NoSystemBackground );
    widget()->setAutoFillBackground( false );

    // initialize timeLine
    timeLine_.setFrameRange( maxAnimationIndex/5, maxAnimationIndex );
    timeLine_.setCurveShape( QTimeLine::EaseOutCurve );
    connect( &timeLine_, SIGNAL( frameChanged( int ) ), widget(), SLOT( update() ) );
    connect( &timeLine_, SIGNAL( finished() ), widget(), SLOT( update() ) );

    // initialize titleTimeLine
    titleTimeLine_.setFrameRange( 0, maxAnimationIndex );
    titleTimeLine_.setCurveShape( QTimeLine::EaseInOutCurve );
    connect( &titleTimeLine_, SIGNAL( frameChanged( int ) ), widget(), SLOT( update() ) );
    connect( &titleTimeLine_, SIGNAL( finished() ), widget(), SLOT( update() ) );
    connect( &titleTimeLine_, SIGNAL( finished() ), this, SLOT( updateOldCaption() ) );

    initialized_ = true;

    // in case of preview, one wants to make the label used
    // for the central widget transparent. This allows one to have
    // the correct background (with gradient) rendered
    // Remark: this is minor (and safe) a hack.
    // This should be moved upstream (into kwin/lib/kdecoration)
    if( isPreview() )
    {

      QList<QLabel*> children( widget()->findChildren<QLabel*>() );
      for( QList<QLabel*>::iterator iter = children.begin(); iter != children.end(); ++iter )
      { (*iter)->setAutoFillBackground( false ); }

    }

    resetConfiguration();

  }

  //___________________________________________
  void OxygenClient::reset( unsigned long changed )
  {
    if( changed & SettingCompositing )
    {
      updateWindowShape();
      widget()->update();
    }

    KCommonDecorationUnstable::reset( changed );
  }

  //___________________________________________
  bool OxygenClient::decorationBehaviour(DecorationBehaviour behaviour) const
  {
    switch (behaviour)
    {

      case DB_MenuClose:
      return true;

      case DB_WindowMask:
      return false;

      default:
      return KCommonDecoration::decorationBehaviour(behaviour);
    }
  }

  //_________________________________________________________
  KCommonDecorationButton *OxygenClient::createButton(::ButtonType type)
  {
    switch (type) {
      case MenuButton:
      return new OxygenButton(*this, i18n("Menu"), ButtonMenu);

      case HelpButton:
      return new OxygenButton(*this, i18n("Help"), ButtonHelp);

      case MinButton:
      return new OxygenButton(*this, i18n("Minimize"), ButtonMin);

      case MaxButton:
      return new OxygenButton(*this, i18n("Maximize"), ButtonMax);

      case CloseButton:
      return new OxygenButton(*this, i18n("Close"), ButtonClose);

      case AboveButton:
      return new OxygenButton(*this, i18n("Keep Above Others"), ButtonAbove);

      case BelowButton:
      return new OxygenButton(*this, i18n("Keep Below Others"), ButtonBelow);

      case OnAllDesktopsButton:
      return new OxygenButton(*this, i18n("On All Desktops"), ButtonSticky);

      case ShadeButton:
      return new OxygenButton(*this, i18n("Shade Button"), ButtonShade);

      default:
      return 0;
    }
  }

  //_________________________________________________________
  QRegion OxygenClient::calcMask( void ) const
  {

    if( isMaximized() )
    { return widget()->rect(); }

    QRect frame( widget()->rect().adjusted(
        layoutMetric( LM_OuterPaddingLeft ), layoutMetric( LM_OuterPaddingTop ),
        -layoutMetric( LM_OuterPaddingRight ), -layoutMetric( LM_OuterPaddingBottom ) ) );

    if( configuration().frameBorder() == OxygenConfiguration::BorderNone && !isShade() ) return helper().roundedMask( frame, 1, 1, 1, 0 );
    else return helper().roundedMask( frame );

  }

    //___________________________________________
  int OxygenClient::layoutMetric(LayoutMetric lm, bool respectWindowState, const KCommonDecorationButton *btn) const
  {

    bool maximized( isMaximized() );
    int frameBorder( configuration().frameBorder() );
    int buttonSize( configuration().buttonSize() );

    switch (lm)
    {
      case LM_BorderLeft:
      case LM_BorderRight:
      case LM_BorderBottom:
      {
        int border( 0 );
        if( respectWindowState && maximized && lm != LM_BorderBottom )
        {

          border = 0;

        }  else if( lm == LM_BorderBottom && frameBorder >= OxygenConfiguration::BorderNoSide ) {

          // for tiny border, the convention is to have a larger bottom area in order to
          // make resizing easier
          border = qMax(frameBorder, 4);

        } else if( frameBorder < OxygenConfiguration::BorderTiny ) {

          border = 0;

        } else if( !compositingActive() && frameBorder == OxygenConfiguration::BorderTiny ) {

          border = qMax( frameBorder, 3 );

        } else {

          border = frameBorder;

        }

        return border;
      }

      case LM_TitleEdgeTop:
      {
        int border = 0;
        if( !( respectWindowState && maximized ))
        {
          border = TFRAMESIZE;
        }

        return border;

      }

      case LM_TitleEdgeBottom:
      {
        return 0;
      }

      case LM_TitleEdgeLeft:
      case LM_TitleEdgeRight:
      {
        int border = 0;
        if( !(respectWindowState && maximized) )
        { border = 6; }

        return border;

      }

      case LM_TitleBorderLeft:
      case LM_TitleBorderRight:
      {
        int border = 5;

        // if title outline is to be drawn, one adds the space needed to
        // separate title and tab border. namely the same value
        if( configuration().drawTitleOutline() ) border += border;

        return border;
      }

      case LM_ButtonWidth:
      case LM_ButtonHeight:
      case LM_TitleHeight:
      {
        return buttonSize;
      }

      case LM_ButtonSpacing:
      return 1;

      case LM_ButtonMarginTop:
      return 0;

      // outer margin for shadow/glow
      case LM_OuterPaddingLeft:
      case LM_OuterPaddingRight:
      case LM_OuterPaddingTop:
      case LM_OuterPaddingBottom:
      return shadowCache().shadowSize();

      default:
      return KCommonDecoration::layoutMetric(lm, respectWindowState, btn);
    }

  }

  //_________________________________________________________
  QRect OxygenClient::titleBoundingRect( QPainter* painter, const QRect& rect, const QString& caption ) const
  {

    // get title bounding rect
    QRect boundingRect = painter->boundingRect( rect, configuration().titleAlignment() | Qt::AlignVCenter, caption );

    // adjust to make sure bounding rect
    // 1/ has same vertical alignment as original titleRect
    // 2/ does not exceeds available horizontal space
    boundingRect.setTop( rect.top() );
    boundingRect.setBottom( rect.bottom() );

    if( rect.left() > boundingRect.left() ) { boundingRect.setLeft( rect.left() ); }
    if( rect.right() < boundingRect.right() ) { boundingRect.setRight( rect.right() ); }

    return boundingRect;

  }

  //_________________________________________________________
  QColor OxygenClient::titlebarTextColor(const QPalette &palette)
  {
    if( timeLineIsRunning() ) return KColorUtils::mix(
      titlebarTextColor( palette, false ),
      titlebarTextColor( palette, true ),
      opacity() );
    else return titlebarTextColor( palette, isActive() );
  }

  //_________________________________________________________
  QColor OxygenClient::titlebarTextColor(const QPalette &palette, bool active)
  {

    if( active ){

      return palette.color(QPalette::Active, QPalette::WindowText);

    } else {

      // todo: reimplement cache
      QColor ab = palette.color(QPalette::Active, QPalette::Window);
      QColor af = palette.color(QPalette::Active, QPalette::WindowText);
      QColor nb = palette.color(QPalette::Inactive, QPalette::Window);
      QColor nf = palette.color(QPalette::Inactive, QPalette::WindowText);
      return reduceContrast(nb, nf, qMax(qreal(2.5), KColorUtils::contrastRatio(ab, KColorUtils::mix(ab, af, 0.4))));

    }

  }

  //_________________________________________________________
  void OxygenClient::renderWindowBackground( QPainter* painter, const QRect& rect, const QWidget* widget, const QPalette& palette ) const
  {

    if( configuration().blendColor() == OxygenConfiguration::NoBlending )
    {

      painter->fillRect( rect, palette.color( widget->window()->backgroundRole() ) );

    } else {

      int offset = layoutMetric( LM_OuterPaddingTop );
      int height = 64 + configuration().buttonSize() - 22;
      const QWidget* window( isPreview() ? OxygenClient::widget() : widget->window() );
      helper().renderWindowBackground(painter, rect, widget, window, palette, offset, height );

    }

  }

  //_________________________________________________________
  void OxygenClient::renderWindowBorder( QPainter* painter, const QRect& clipRect, const QWidget* widget, const QPalette& palette ) const
  {

    // check if outline is needed
    if( !( isActive() && configuration().drawTitleOutline() ) ) return;

    // get coordinates relative to the client area
    // this is annoying. One could use mapTo if this was taking const QWidget* and not
    // const QWidget* as argument.
    const QWidget* window = (isPreview()) ? OxygenClient::widget() : widget->window();
    const QWidget* w = widget;
    QPoint position( 0, 0 );
    while (  w != window && !w->isWindow() && w != w->parentWidget() )
    {
      position += w->geometry().topLeft();
      w = w->parentWidget();
    }

    // save painter
    if( clipRect.isValid() )
    {
      painter->save();
      painter->setClipRegion(clipRect,Qt::IntersectClip);
    }

    QRect r = (isPreview()) ? OxygenClient::widget()->rect():window->rect();
    qreal shadowSize( shadowCache().shadowSize() );
    r.adjust( shadowSize, shadowSize, -shadowSize, -shadowSize );
    r.adjust(0,0, 1, 1);

    // base color
    QColor color( palette.window().color() );

    // title height
    int titleHeight( layoutMetric( LM_TitleEdgeTop ) + layoutMetric( LM_TitleEdgeBottom ) + layoutMetric( LM_TitleHeight ) );

    // horizontal line
    {
      int shadowSize = 7;
      int height = shadowSize-3;

      QPoint topLeft( r.topLeft()+QPoint(0,titleHeight-height)-position );
      QRect rect( topLeft, QSize( r.width(), height ) );

      // adjustements to cope with shadow size and outline border.
      rect.adjust( -shadowSize, 0, shadowSize-1, 0 );
      if( configuration().frameBorder() > OxygenConfiguration::BorderTiny && configuration().drawTitleOutline() && isActive() && !isMaximized() )
      { rect.adjust( HFRAMESIZE-1, 0, -HFRAMESIZE+1, 0 ); }

      helper().slab( color, 0, shadowSize )->render( rect, painter, TileSet::Top );

    }

    if( configuration().drawTitleOutline() )
    {

      // save mask and frame to where
      // grey window background is to be rendered
      QRegion mask;
      QRect frame;

      // bottom line
      int leftOffset = qMin( layoutMetric( LM_BorderLeft ), int(HFRAMESIZE) );
      int rightOffset = qMin( layoutMetric( LM_BorderRight ), int(HFRAMESIZE) );
      if( configuration().frameBorder() > OxygenConfiguration::BorderNone )
      {

        int height = qMax( 0, layoutMetric( LM_BorderBottom ) - HFRAMESIZE );
        int width = r.width() - leftOffset - rightOffset - 1;

        QRect rect( r.bottomLeft()-position + QPoint( leftOffset, -layoutMetric( LM_BorderBottom ) ), QSize( width, height ) );
        if( height > 0 ) { mask += rect; frame |= rect; }

        QColor shadow( helper().calcDarkColor( color ) );
        painter->setPen( shadow );
        painter->drawLine( rect.bottomLeft()+QPoint(0,1), rect.bottomRight()+QPoint(0,1) );

      }

      // left and right
      int topOffset = titleHeight;
      int bottomOffset = qMin( layoutMetric( LM_BorderBottom ), int(HFRAMESIZE) );
      int height = r.height() - topOffset - bottomOffset - 1;

      if( configuration().frameBorder() >= OxygenConfiguration::BorderTiny )
      {

        QColor shadow( helper().calcLightColor( color ) );
        painter->setPen( shadow );

        // left
        int width = qMax( 0, layoutMetric( LM_BorderLeft ) - HFRAMESIZE );
        QRect rect( r.topLeft()-position + QPoint( layoutMetric( LM_BorderLeft ) - width, topOffset ), QSize( width, height ) );
        if( width > 0 ) { mask += rect; frame |= rect; }

        painter->drawLine( rect.topLeft()-QPoint(1,0), rect.bottomLeft()-QPoint(1, 0) );

        // right
        width = qMax( 0, layoutMetric( LM_BorderRight ) - HFRAMESIZE );
        rect = QRect(r.topRight()-position + QPoint( -layoutMetric( LM_BorderRight ), topOffset ), QSize( width, height ));
        if( width > 0 ) { mask += rect; frame |= rect; }

        painter->drawLine( rect.topRight()+QPoint(1,0), rect.bottomRight()+QPoint(1, 0) );
      }

      // in preview mode also adds center square
      if( isPreview() )
      {
        QRect rect( r.topLeft()-position + QPoint( layoutMetric( LM_BorderLeft ), topOffset ), QSize(r.width()-layoutMetric( LM_BorderLeft )-layoutMetric( LM_BorderRight ),height) );
        mask += rect; frame |= rect;
      }

      // paint
      if( !mask.isEmpty() )
      {
        painter->setClipRegion( mask, Qt::IntersectClip);
        renderWindowBackground(painter, frame, widget, palette );
      }

    }

    // restore painter
    if( clipRect.isValid() )
    { painter->restore(); }

  }

  //_________________________________________________________
  void OxygenClient::renderSeparator( QPainter* painter, const QRect& clipRect, const QWidget* widget, const QColor& color ) const
  {

    const QWidget* window = (isPreview()) ? OxygenClient::widget() : widget->window();

    // get coordinates relative to the client area
    // this is annoying. One could use mapTo if this was taking const QWidget* and not
    // const QWidget* as argument.
    QPoint position( 0, 0 );
    {
      const QWidget* w = widget;
      while (  w != window && !w->isWindow() && w != w->parentWidget() ) {
        position += w->geometry().topLeft();
        w = w->parentWidget();
      }
    }

    // setup painter
    if (clipRect.isValid())
    {
      painter->save();
      painter->setClipRegion(clipRect,Qt::IntersectClip);
    }

    QRect r = (isPreview()) ? OxygenClient::widget()->rect():window->rect();
    qreal shadowSize( shadowCache().shadowSize() );
    r.adjust( shadowSize, shadowSize, -shadowSize, -shadowSize );

    // dimensions
    const int titleHeight = layoutMetric(LM_TitleHeight);
    const int titleTop = layoutMetric(LM_TitleEdgeTop) + r.top();

    QColor local( color );
    if( timeLineIsRunning() ) local = helper().alphaColor( color, opacity() );
    helper().drawSeparator( painter, QRect(r.top(), titleTop+titleHeight-1.5, r.width(), 2).translated( -position ), local, Qt::Horizontal);

    if (clipRect.isValid()) { painter->restore(); }

  }

  //_________________________________________________________
  void OxygenClient::renderTitleOutline(  QPainter* painter, const QRect& rect, const QPalette& palette ) const
  {

    // center (for active windows only)
    {
      painter->save();
      int offset = 2;
      int voffset = 1;
      QRect adjustedRect( rect.adjusted( offset, voffset, -offset, 1 ) );

      // prepare painter mask
      QRegion mask( adjustedRect.adjusted( 1, 0, -1, 0 ) );
      mask += adjustedRect.adjusted( 0, 1, 0, 0 );
      painter->setClipRegion( mask, Qt::IntersectClip );

      // draw window background
      renderWindowBackground(painter, adjustedRect, widget(), palette );
      painter->restore();
    }

    // shadow
    int shadowSize = 7;
    int offset = -2;
    int voffset = 5-shadowSize;
    QRect adjustedRect( rect.adjusted(offset, voffset, -offset, shadowSize) );
    helper().slab( palette.color( widget()->backgroundRole() ), 0, shadowSize )->render( adjustedRect, painter, TileSet::Top|TileSet::Left|TileSet::Right );

  }

  //_________________________________________________________
  void OxygenClient::renderTitleText( QPainter* painter, const QRect& rect, QColor color ) const
  {

    if( titleTimeLineIsRunning() )
    {

      if( !oldCaption().isEmpty() ) renderTitleText( painter, rect, oldCaption(), helper().alphaColor( color, 1.0 - titleOpacity() ) );
      if( !caption().isEmpty() ) renderTitleText( painter, rect, caption(), helper().alphaColor( color, titleOpacity() ) );

    } else if( !caption().isEmpty() ) {

      renderTitleText( painter, rect, caption(), color );

    }

  }

  //_______________________________________________________________________
  void OxygenClient::renderTitleText( QPainter* painter, const QRect& rect, const QString& caption, const QColor& color ) const
  {

    Qt::Alignment alignment( configuration().titleAlignment() | Qt::AlignVCenter );
    QString local( QFontMetrics( painter->font() ).elidedText( caption, Qt::ElideRight, rect.width() ) );
    painter->setPen( color );
    painter->drawText( rect, alignment, local );

  }

  //_______________________________________________________________________
  void OxygenClient::renderFloatFrame( QPainter* painter, const QRect& frame, const QPalette& palette ) const
  {

    // shadow and resize handles
    if( !isMaximized() )
    {

      if( configuration().frameBorder() >= OxygenConfiguration::BorderTiny )
      {

        helper().drawFloatFrame(
          painter, frame, backgroundColor( widget(), palette ),
          !compositingActive(), isActive() && configuration().useOxygenShadows(),
          KDecoration::options()->color(ColorTitleBar)
          );

      } else {

        // for small borders, use a frame that matches the titlebar only
        QRect local( frame.topLeft(), QSize( frame.width(), layoutMetric(LM_TitleHeight) + layoutMetric(LM_TitleEdgeTop) ) );
        helper().drawFloatFrame(
          painter, local, backgroundColor( widget(), palette ),
          false, isActive() && configuration().useOxygenShadows(),
          KDecoration::options()->color(ColorTitleBar)
          );
      }

    } else if( isShade() ) {

      // for shaded maximized windows adjust frame and draw the bottom part of it
      helper().drawFloatFrame(
        painter, frame.adjusted( -4, 0, 4, 0 ), backgroundColor( widget(), palette ),
        !( compositingActive() || configuration().frameBorder() == OxygenConfiguration::BorderNone ), isActive(),
        KDecoration::options()->color(ColorTitleBar),
        TileSet::Bottom
        );

    }

  }

  //____________________________________________________________________________
  void OxygenClient::renderDots( QPainter* painter, const QRect& frame, const QColor& color ) const
  {

    if( configuration().frameBorder() >= OxygenConfiguration::BorderTiny )
    {


      // dimensions
      int x,y,w,h;
      frame.getRect(&x, &y, &w, &h);

      if( isResizable() && !isShade() && !isMaximized() )
      {

        // Draw right side 3-dots resize handles
        qreal cenY = h / 2 + y + 0.5;
        qreal posX = w + x - 2.5;

        painter->setPen(Qt::NoPen);
        painter->setBrush( color );
        renderDot( painter, QPointF(posX, cenY - 3), 1.8);
        renderDot( painter, QPointF(posX, cenY), 1.8);
        renderDot( painter, QPointF(posX, cenY + 3), 1.8);

      }

      // Draw bottom-right cornet 3-dots resize handles
      if( isResizable() && !isShade() && !configuration().drawSizeGrip() )
      {

        painter->setPen(Qt::NoPen);
        painter->setBrush( color );

        painter->save();
        painter->translate(x + w-9, y + h-9);
        renderDot( painter, QPointF(2.5, 6.5), 1.8);
        renderDot( painter, QPointF(5.5, 5.5), 1.8);
        renderDot( painter, QPointF(6.5, 2.5), 1.8);
        painter->restore();
      }

    }

  }

  //_________________________________________________________
  void OxygenClient::activeChange( void )
  {

    KCommonDecorationUnstable::activeChange();

    // reset animation
    if( animateActiveChange() )
    {
      timeLine_.setDirection( isActive() ? QTimeLine::Forward : QTimeLine::Backward );
      if(timeLine_.state() == QTimeLine::NotRunning )
      { timeLine_.start(); }
    }

    // update size grip so that it gets the right color
    // also make sure it is remaped to from z stack,
    // unless hidden
    if( hasSizeGrip() && !(isShade() || isMaximized() ))
    {
      sizeGrip().activeChange();
      sizeGrip().update();
    }

  }

  //_________________________________________________________
  void OxygenClient::maximizeChange( void  )
  {
    if( hasSizeGrip() ) sizeGrip().setVisible( !( isShade() || isMaximized() ) );
    KCommonDecorationUnstable::maximizeChange();
  }

  //_________________________________________________________
  void OxygenClient::shadeChange( void  )
  {
    if( hasSizeGrip() ) sizeGrip().setVisible( !( isShade() || isMaximized() ) );
    KCommonDecorationUnstable::shadeChange();
  }

  //_________________________________________________________
  void OxygenClient::captionChange( void  )
  {
    KCommonDecorationUnstable::captionChange();

    if( !animateTitleChange() ) return;
    if( titleTimeLineIsRunning() ) titleTimeLine_.stop();
    titleTimeLine_.start();

  }

  //_________________________________________________________
  QPalette OxygenClient::backgroundPalette( const QWidget* widget, QPalette palette ) const
  {

    if( configuration().drawTitleOutline() )
    {
      if( timeLineIsRunning() )
      {

        QColor inactiveColor( backgroundColor( widget, palette, false ) );
        QColor activeColor( backgroundColor( widget, palette, true ) );
        QColor mixed( KColorUtils::mix( inactiveColor, activeColor, opacity() ) );
        palette.setColor( widget->window()->backgroundRole(), mixed );
        palette.setColor( QPalette::Button, mixed );

      } else if( isActive() ) {

        QColor color =  options()->color( KDecorationDefines::ColorTitleBar, true );
        palette.setColor( widget->window()->backgroundRole(), color );
        palette.setColor( QPalette::Button, color );

      }

    }

    return palette;

  }

  //_________________________________________________________
  QColor OxygenClient::backgroundColor( const QWidget* widget, QPalette palette, bool active ) const
  {

    return ( configuration().drawTitleOutline() && active ) ?
      options()->color( KDecorationDefines::ColorTitleBar, true ):
      palette.color( widget->window()->backgroundRole() );

  }

  //________________________________________________________________
  void OxygenClient::updateWindowShape()
  {

    if(isMaximized() || compositingActive() ) clearMask();
    else setMask( calcMask() );

  }


  //___________________________________________
  void OxygenClient::resetConfiguration( void )
  {

    if( !initialized_ ) return;

    configuration_ = factory_->configuration( *this );

    // animations duration
    timeLine_.setDuration( configuration_.animationsDuration() );
    titleTimeLine_.setDuration( configuration_.animationsDuration() );

    // need to update old caption
    updateOldCaption();

    // handle size grip
    if( configuration_.drawSizeGrip() )
    {

      if( !( hasSizeGrip() || isPreview() ) ) createSizeGrip();

    } else if( hasSizeGrip() ) deleteSizeGrip();

  }

  //_________________________________________________________
  void OxygenClient::paintEvent( QPaintEvent* event )
  {

    // factory
    if(!( initialized_ && factory_->initialized() ) ) return;

    // palette
    QPalette palette = widget()->palette();
    palette.setCurrentColorGroup( (isActive() ) ? QPalette::Active : QPalette::Inactive );

    // painter
    QPainter painter(widget());
    painter.setClipRegion( event->region() );

    // define frame
    QRect frame = widget()->rect();

    // base color
    QColor color = palette.window().color();

    // draw shadows
    if( compositingActive() )
    {

      TileSet *tileSet( 0 );
      if( configuration().useOxygenShadows() && timeLineIsRunning() )
      {

        int frame = timeLine_.currentFrame();
        if( timeLine_.direction() == QTimeLine::Backward ) frame -= timeLine_.startFrame();
        tileSet = shadowCache().tileSet( this, frame );

      } else tileSet = shadowCache().tileSet( this );

      if( !isMaximized() ) tileSet->render( frame.adjusted( 4, 4, -4, -4), &painter, TileSet::Ring);
      else if( isShade() ) tileSet->render( frame.adjusted( 0, 4, 0, -4), &painter, TileSet::Bottom);

    }

    // adjust frame
    qreal shadowSize( shadowCache().shadowSize() );
    frame.adjust( shadowSize, shadowSize, -shadowSize, -shadowSize );

    //  adjust mask
    if( compositingActive() || isPreview() )
    {

      if( isMaximized() ) {

        painter.setClipRect( frame, Qt::IntersectClip );

      } else {

        // multipliers
        int left = 1;
        int right = 1;
        int top = 1;
        int bottom = 1;

        // disable bottom corners when border frame is too small and window is not shaded
        if( configuration().frameBorder() == OxygenConfiguration::BorderNone && !isShade() ) bottom = 0;
        QRegion mask( helper().roundedRegion( frame, left, right, top, bottom ) );

        // in no-border configuration, an extra pixel is added to the mask
        // in order to get the corners color right in case of title highlighting.
        if( configuration().frameBorder() == OxygenConfiguration::BorderNone )
        {
            int x, y, w, h;
            frame.getRect(&x, &y, &w, &h);
            mask += QRegion(x+0*left, y+4*top, w-0*(left+right), h-4*(top+bottom));
        }

        painter.setClipRegion( mask, Qt::IntersectClip );

      }


    }

    // window background
    renderWindowBackground( &painter, frame, widget(), backgroundPalette( widget(), palette ) );
    renderWindowBorder( &painter, frame, widget(), palette );

    // clipping
    if( compositingActive() )
    {
      painter.setClipping(false);
      frame.adjust(-1,-1, 1, 1);
    }

    // adjust frame if there are shadows
    {
      painter.save();
      painter.setRenderHint(QPainter::Antialiasing);


      // float frame
      renderFloatFrame( &painter, frame, palette );

      // clipping
      if( compositingActive() ) painter.setClipping(false);

      // resize handles
      renderDots( &painter, frame, QColor(0, 0, 0, 66) );
      painter.restore();
    }

    // title bounding rect
    painter.setFont( options()->font(isActive(), false) );
    QRect boundingRect( titleBoundingRect( &painter, caption() ) );
    if( isActive() && configuration().drawTitleOutline() )
    {
      renderTitleOutline( &painter, boundingRect.adjusted(
        -layoutMetric( LM_TitleBorderLeft ),
        -layoutMetric( LM_TitleEdgeTop ),
        layoutMetric( LM_TitleBorderRight ), 0 ), palette );
    }

    // title text
    renderTitleText( &painter, boundingRect, titlebarTextColor( backgroundPalette( widget(), palette ) ) );

    // separator
    if( drawSeparator() ) renderSeparator(&painter, frame, widget(), color );

  }

  //_________________________________________________________________
  void OxygenClient::createSizeGrip( void )
  {

    assert( !hasSizeGrip() );
    if( ( isResizable() && windowId() != 0 ) || isPreview() )
    {
      sizeGrip_ = new OxygenSizeGrip( this );
      sizeGrip().setVisible( !( isMaximized() || isShade() ) );
    }

  }

  //_________________________________________________________________
  void OxygenClient::deleteSizeGrip( void )
  {
    assert( hasSizeGrip() );
    sizeGrip_->deleteLater();
    sizeGrip_ = 0;
  }

}
