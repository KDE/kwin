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
#include "oxygenshadowcache.h"
#include "oxygensizegrip.h"

#include <cassert>
#include <cmath>

#include <kdeversion.h>
#include <KGlobal>
#include <KLocale>
#include <KColorUtils>
#include <KDebug>

#include <QLabel>
#include <QPainter>
#include <QApplication>

using namespace std;
namespace Oxygen
{

  const int maxAnimationIndex( 256 );
  K_GLOBAL_STATIC_WITH_ARGS(OxygenHelper, globalHelper, ("oxygenDeco"))
  K_GLOBAL_STATIC_WITH_ARGS(OxygenShadowCache, globalShadowCache, (maxAnimationIndex))

  //___________________________________________
  OxygenHelper *oxygenHelper()
  { return globalHelper; }

  //___________________________________________
  OxygenShadowCache *oxygenShadowCache()
  { return globalShadowCache; }

  //___________________________________________
  static void oxkwincleanupBefore()
  {

    globalHelper->invalidateCaches();
    globalShadowCache->invalidateCaches();

  }

  //___________________________________________
  void renderDot(QPainter *p, const QPointF &point, qreal diameter)
  {
    p->drawEllipse(QRectF(point.x()-diameter/2, point.y()-diameter/2, diameter, diameter));
  }

  //___________________________________________
  OxygenClient::OxygenClient(KDecorationBridge *b, KDecorationFactory *f):
    KCommonDecorationUnstable(b, f),
    colorCacheInvalid_(true),
    sizeGrip_( 0 ),
    timeLine_( 200, this ),
    helper_(*globalHelper),
    initialized_( false )
  { qAddPostRoutine(oxkwincleanupBefore); }

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
    timeLine_.setFrameRange( 0, maxAnimationIndex );
    timeLine_.setCurveShape( QTimeLine::EaseInOutCurve );
    connect( &timeLine_, SIGNAL( frameChanged( int ) ), widget(), SLOT( update() ) );
    connect( &timeLine_, SIGNAL( finished() ), widget(), SLOT( update() ) );

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

  //___________________________________________
  int OxygenClient::layoutMetric(LayoutMetric lm, bool respectWindowState, const KCommonDecorationButton *btn) const
  {

    bool maximized( isMaximized() );
    int frameBorder( configuration().frameBorder() );

    // used to increase hit area on the sides of the widget
    int extraBorder = (maximized && compositingActive()) ? 0 : EXTENDED_HITAREA;
    int buttonSize( configuration().buttonSize() );

    switch (lm)
    {
      case LM_BorderLeft:
      case LM_BorderRight:
      case LM_BorderBottom:
      {
        int border( 0 );
        if (respectWindowState && maximized) {
          border = 0;
        }  else if( lm == LM_BorderBottom && frameBorder >= OxygenConfiguration::BorderNoSide ) {

          // for tiny border, the convention is to have a larger bottom area in order to
          // make resizing easier
          border = qMax(frameBorder, 4);

        } else if( frameBorder < OxygenConfiguration::BorderTiny ) {

          border = 0;

        } else {

          border = frameBorder;

        }

        return border + extraBorder;
      }

      case LM_TitleEdgeTop:
      {
        int border = 0;
        if( !( respectWindowState && maximized ))
        {
          border = TFRAMESIZE;
          if( configuration().drawTitleOutline() ) border += HFRAMESIZE/2;
        }

        return border + extraBorder;

      }

      case LM_TitleEdgeBottom:
      {
        if( configuration().drawTitleOutline() ) return HFRAMESIZE/2;
        else return 0;
      }

      case LM_TitleEdgeLeft:
      case LM_TitleEdgeRight:
      {
        int border = 0;
        if( !(respectWindowState && maximized) )
        { border = 6; }

        return border + extraBorder;

      }

      case LM_TitleBorderLeft:
      case LM_TitleBorderRight:
      {
        int border = 5;
        if( configuration().drawTitleOutline() ) border += 2*HFRAMESIZE;
        return border;
      }

      case LM_ButtonWidth:
      case LM_ButtonHeight:
      case LM_TitleHeight:
      {
        if (respectWindowState && isToolWindow()) {

          return buttonSize;

        } else {

          return buttonSize;

        }
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
      return oxygenShadowCache()->shadowSize() - extraBorder;

      default:
      return KCommonDecoration::layoutMetric(lm, respectWindowState, btn);
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

  //_________________________________________________________
  QRegion OxygenClient::calcMask( void ) const
  {

    if( isMaximized() )
    { return widget()->rect(); }

    QRect frame( widget()->rect() );

    // dimensions
    int w=frame.width();
    int h=frame.height();

    // multipliers
    int left = 1;
    int right = 1;
    int top = 1;
    int bottom = 1;

    // disable bottom corners when border frame is too small and window is not shaded
    if( configuration().frameBorder() == OxygenConfiguration::BorderNone && !isShade() ) bottom = 0;

    int sw = layoutMetric( LM_OuterPaddingLeft );
    int sh = layoutMetric( LM_OuterPaddingTop );

    w -= sw + layoutMetric( LM_OuterPaddingRight );
    h -= sh + layoutMetric( LM_OuterPaddingBottom );


    QRegion mask(sw + 4*left, sh + 0*top, w-4*(left+right), h-0*(top+bottom));
    mask += QRegion(sw + 0*left, sh + 4*top, w-0*(left+right), h-4*(top+bottom));
    mask += QRegion(sw + 2*left, sh + 1*top, w-2*(left+right), h-1*(top+bottom));
    mask += QRegion(sw + 1*left, sh + 2*top, w-1*(left+right), h-2*(top+bottom));

    return mask;

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

    if( configuration().drawTitleOutline() )
    {

      return options()->color(ColorFont, active);

    } else if( active ){

      return palette.color(QPalette::Active, QPalette::WindowText);

    } else {

      if(colorCacheInvalid_)
      {

        QColor ab = palette.color(QPalette::Active, QPalette::Window);
        QColor af = palette.color(QPalette::Active, QPalette::WindowText);
        QColor nb = palette.color(QPalette::Inactive, QPalette::Window);
        QColor nf = palette.color(QPalette::Inactive, QPalette::WindowText);

        colorCacheInvalid_ = false;
        cachedTitlebarTextColor_ = reduceContrast(nb, nf, qMax(qreal(2.5), KColorUtils::contrastRatio(ab, KColorUtils::mix(ab, af, 0.4))));
      }

      return cachedTitlebarTextColor_;

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

    const QWidget* window = (isPreview()) ? OxygenClient::widget() : widget->window();

    // get coordinates relative to the client area
    // this is annoying. One could use mapTo if this was taking const QWidget* and not
    // const QWidget* as argument.
    const QWidget* w = widget;
    QPoint position( 0, 0 );
    while (  w != window && !w->isWindow() && w != w->parentWidget() ) {
      position += w->geometry().topLeft();
      w = w->parentWidget();
    }

    // save painter
    painter->save();
    if( timeLineIsRunning() ) painter->setOpacity( opacity() );
    if( clipRect.isValid() ) painter->setClipRegion(clipRect,Qt::IntersectClip);

    QRect r = (isPreview()) ? OxygenClient::widget()->rect():window->rect();
    qreal shadowSize( oxygenShadowCache()->shadowSize() );
    r.adjust( shadowSize, shadowSize, -shadowSize, -shadowSize );
    r.adjust(0,0, 1, 1);

    // mask and painting frame
    QRegion mask;
    QRect frame;

    // top line
    {
      int shadowSize = 5;
      int height = HFRAMESIZE;
      QRect rect( r.topLeft()-position, QSize( r.width(), height ) );
      helper().slab( palette.color( widget->backgroundRole() ), 0, shadowSize )->render( rect.adjusted(-shadowSize-1, 0, shadowSize+1, 2 ), painter, TileSet::Bottom );
      mask += rect;
      frame |= rect;
    }

    // bottom line
    if( configuration().frameBorder() > OxygenConfiguration::BorderNone )
    {
      int height = qMin( HFRAMESIZE, layoutMetric( LM_BorderBottom ) );
      QRect rect( r.bottomLeft()-position-QPoint(0,height), QSize( r.width(), height ) );
      mask += rect;
      frame |= rect;
    }

    // left and right
    if( configuration().frameBorder() >= OxygenConfiguration::BorderTiny )
    {

      // left
      {
        int width = qMin( HFRAMESIZE, layoutMetric( LM_BorderLeft ) );
        QRect rect( r.topLeft()-position, QSize( width, r.height() ) );
        mask += rect;
        frame |= rect;
      }

      // right
      {
        int width = qMin( HFRAMESIZE, layoutMetric( LM_BorderRight ) );
        QRect rect( r.topRight()-position-QPoint(width,0), QSize( width, r.height() ) );
        mask += rect;
        frame |= rect;
      }

    }

    // paint
    painter->setClipRegion( mask, Qt::IntersectClip);
    renderWindowBackground(painter, frame, widget, palette );

    // restore painter
    painter->restore();

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
    painter->save();
    if( timeLineIsRunning() ) painter->setOpacity( opacity() );
    if (clipRect.isValid()) painter->setClipRegion(clipRect,Qt::IntersectClip);

    QRect r = (isPreview()) ? OxygenClient::widget()->rect():window->rect();
    qreal shadowSize( oxygenShadowCache()->shadowSize() );
    r.adjust( shadowSize, shadowSize, -shadowSize, -shadowSize );
    r.adjust(0,0, 1, 1);

    int extraBorder = ( isMaximized() && compositingActive() ) ? 0 : EXTENDED_HITAREA;

    // dimensions
    const int titleHeight = layoutMetric(LM_TitleHeight);
    const int titleTop = layoutMetric(LM_TitleEdgeTop) + r.top() - extraBorder;

    // dimensions
    int x,y,w,h;
    r.getRect(&x, &y, &w, &h);
    helper().drawSeparator( painter, QRect(x, titleTop+titleHeight-1.5, w, 2).translated( -position ), color, Qt::Horizontal);

    painter->restore();

  }

  //_________________________________________________________
  void OxygenClient::renderTitleOutline(  QPainter* painter, const QRect& rect, const QPalette& palette ) const
  {

    if( timeLineIsRunning() )
    {
      painter->save();
      painter->setOpacity( opacity() );
    }

    // shadow
    {
      int shadowSize = 7;
      int voffset = -shadowSize;
      if( !isMaximized() ) voffset += HFRAMESIZE;
      helper().slab( palette.color( widget()->backgroundRole() ), 0, shadowSize )->render( rect.adjusted(0, voffset, 0, 0 ), painter, TileSet::Bottom|TileSet::Left|TileSet::Right );
    }

    // center
    {
      int voffset = isMaximized() ? 0:HFRAMESIZE;
      renderWindowBackground(painter, rect.adjusted( 4, voffset, -4, -4 ), widget(), palette );
    }

    if( timeLineIsRunning() ) painter->restore();

  }

  //_________________________________________________________
  void OxygenClient::activeChange( void )
  {

    // update size grip so that it gets the right color
    if( hasSizeGrip() )
    {
      sizeGrip().activeChange();
      sizeGrip().update();
    }

    // reset animation
    if( animateActiveChange() )
    {
      timeLine_.setDirection( isActive() ? QTimeLine::Forward : QTimeLine::Backward );
      if(timeLine_.state() == QTimeLine::NotRunning )
      { timeLine_.start(); }
    }

    KCommonDecorationUnstable::activeChange();

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
  QPalette OxygenClient::backgroundPalette( const QWidget* widget, QPalette palette ) const
  {

    if( configuration().drawTitleOutline() && isActive() )
    { palette.setColor( widget->window()->backgroundRole(), options()->color( KDecorationDefines::ColorTitleBar, true ) ); }

    return palette;

  }

  //________________________________________________________________
  void OxygenClient::updateWindowShape()
  {

    if(isMaximized() || compositingActive() )
    {

      clearMask();

    } else {

      setMask( calcMask() );

    }

  }


  //___________________________________________
  void OxygenClient::resetConfiguration( void )
  {

    if( !initialized_ ) return;

    configuration_ = OxygenFactory::configuration( *this );

    // handle size grip
    if( configuration_.drawSizeGrip() )
    {

      if( !hasSizeGrip() ) createSizeGrip();

    } else if( hasSizeGrip() ) deleteSizeGrip();

  }

  //_________________________________________________________
  void OxygenClient::paintEvent( QPaintEvent* event )
  {

    // factory
    if(!( initialized_ && OxygenFactory::initialized() ) ) return;

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
    if( compositingActive() && !isMaximized() )
    {

      if( configuration().useOxygenShadows() && timeLineIsRunning() )
      {

        oxygenShadowCache()->tileSet( this, timeLine_.currentFrame() )->render(
          frame.adjusted( 4, 4, -4, -4),
          &painter, TileSet::Ring);

      } else {

        oxygenShadowCache()->tileSet( this )->render(
          frame.adjusted( 4, 4, -4, -4),
          &painter, TileSet::Ring);

      }

    }

    // adjust frame
    qreal shadowSize( oxygenShadowCache()->shadowSize() );
    frame.adjust( shadowSize, shadowSize, -shadowSize, -shadowSize );

    //  adjust mask
    if( compositingActive() || isPreview() )
    {

      if( isMaximized() ) {

        painter.setClipRect( frame, Qt::IntersectClip );

      } else {

        int x, y, w, h;
        frame.getRect(&x, &y, &w, &h);

        // multipliers
        int left = 1;
        int right = 1;
        int top = 1;
        int bottom = 1;

        // disable bottom corners when border frame is too small and window is not shaded
        if( configuration().frameBorder() == OxygenConfiguration::BorderNone && !isShade() ) bottom = 0;
        QRegion mask( x+5*left,   y+0*top, w-5*(left+right), h-0*(top+bottom));
        mask += QRegion(x+0*left, y+5*top, w-0*(left+right), h-5*(top+bottom));
        mask += QRegion(x+2*left, y+2*top, w-2*(left+right), h-2*(top+bottom));
        mask += QRegion(x+3*left, y+1*top, w-3*(left+right), h-1*(top+bottom));
        mask += QRegion(x+1*left, y+3*top, w-1*(left+right), h-3*(top+bottom));

        // in no-border configuration, an extra pixel is added to the mask
        // in order to get the corners color right in case of title highlighting.
        if( configuration().frameBorder() == OxygenConfiguration::BorderNone )
        { mask += QRegion(x+0*left, y+4*top, w-0*(left+right), h-4*(top+bottom)); }

        painter.setClipRegion( mask, Qt::IntersectClip );

      }


    }

    // window background
    renderWindowBackground( &painter, frame, widget(), palette );
    if( drawTitleOutline()  && !isMaximized() ) renderWindowBorder( &painter, frame, widget(), backgroundPalette( widget(), palette ) );

    // clipping
    if( compositingActive() ) painter.setClipping(false);

    int extraBorder = ( isMaximized() && compositingActive() ) ? 0 : EXTENDED_HITAREA;

    // dimensions
    const int titleHeight = layoutMetric(LM_TitleHeight);
    const int titleTop = layoutMetric(LM_TitleEdgeTop) + frame.top() - extraBorder;
    const int titleEdgeLeft = layoutMetric(LM_TitleEdgeLeft);
    const int marginLeft = layoutMetric(LM_TitleBorderLeft);
    const int marginRight = layoutMetric(LM_TitleBorderRight);

    const int titleLeft = frame.left() + titleEdgeLeft + buttonsLeftWidth() + marginLeft;
    const int titleWidth = frame.width() -
      titleEdgeLeft - layoutMetric(LM_TitleEdgeRight) -
      buttonsLeftWidth() - buttonsRightWidth() -
      marginLeft - marginRight;

    QRect titleRect( titleLeft, titleTop-1, titleWidth, titleHeight );
    painter.setFont( options()->font(isActive(), false) );

    if( drawTitleOutline() )
    {

      // get title bounding rect
      QRect boundingRect = painter.boundingRect( titleRect, configuration().titleAlignment() | Qt::AlignVCenter, caption() );

      // adjust
      boundingRect.setTop( frame.top() );
      boundingRect.setBottom( titleTop+titleHeight );
      boundingRect.setLeft( qMax( boundingRect.left(), titleLeft ) - 2*HFRAMESIZE );
      boundingRect.setRight( qMin( boundingRect.right(), titleLeft + titleWidth ) + 2*HFRAMESIZE );
      renderTitleOutline( &painter, boundingRect, backgroundPalette( widget(), palette ) );

    }

    // separator
    if( drawSeparator() ) renderSeparator(&painter, frame, widget(), color );

    // draw title text
    painter.setPen( titlebarTextColor( backgroundPalette( widget(), palette ) ) );

    painter.drawText( titleRect, configuration().titleAlignment() | Qt::AlignVCenter, caption() );
    painter.setRenderHint(QPainter::Antialiasing);

    // adjust if there are shadows
    if( compositingActive() ) frame.adjust(-1,-1, 1, 1);

    // dimensions
    int x,y,w,h;
    frame.getRect(&x, &y, &w, &h);

    // shadow and resize handles
    if( configuration().frameBorder() >= OxygenConfiguration::BorderTiny && !isMaximized() )
    {

      helper().drawFloatFrame(
        &painter, frame, backgroundPalette( widget(), palette ).color( widget()->backgroundRole() ),
        !compositingActive(), isActive(),
        KDecoration::options()->color(ColorTitleBar)
        );

      if( isResizable() && !isShade() )
      {

        // Draw the 3-dots resize handles
        qreal cenY = h / 2 + x + 0.5;
        qreal posX = w + y - 2.5;
        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor(0, 0, 0, 66));
        renderDot(&painter, QPointF(posX, cenY - 3), 1.8);
        renderDot(&painter, QPointF(posX, cenY), 1.8);
        renderDot(&painter, QPointF(posX, cenY + 3), 1.8);

      }

      // Draw the 3-dots resize handles
      if( isResizable() && !isShade() && !configuration().drawSizeGrip() )
      {

        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor(0, 0, 0, 66));

        painter.translate(x + w-9, y + h-9);
        renderDot(&painter, QPointF(2.5, 6.5), 1.8);
        renderDot(&painter, QPointF(5.5, 5.5), 1.8);
        renderDot(&painter, QPointF(6.5, 2.5), 1.8);
      }

    }

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
