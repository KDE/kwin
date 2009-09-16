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

#include <cassert>
#include <cmath>
#include <iostream>

#include <kdeversion.h>
#include <KGlobal>
#include <KLocale>
#include <KColorUtils>
#include <KDebug>

#include <QLabel>
#include <QPainter>
#include <QTextStream>
#include <QApplication>

using namespace std;
namespace Oxygen
{

  K_GLOBAL_STATIC_WITH_ARGS(OxygenHelper, globalHelper, ("oxygenDeco"))

  //___________________________________________
  OxygenHelper *oxygenHelper()
  { return globalHelper; }

  //___________________________________________
  static void oxkwincleanupBefore()
  {
    OxygenHelper *h = globalHelper;
    h->invalidateCaches();
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
    inactiveShadowTiles_( 0 ),
    activeShadowTiles_( 0 ),
    helper_(*globalHelper),
    initialized_( false )
  { qAddPostRoutine(oxkwincleanupBefore); }

  //___________________________________________
  OxygenClient::~OxygenClient()
  {

    // delete sizegrip if any
    if( hasSizeGrip() ) deleteSizeGrip();

    // delete tilesets
    if( inactiveShadowTiles_ ) delete inactiveShadowTiles_;
    if( activeShadowTiles_ ) delete activeShadowTiles_;

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
  bool OxygenClient::isMaximized() const
  { return maximizeMode()==MaximizeFull && !options()->moveResizeMaximizedWindows();  }

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

        }  else if( configuration().frameBorder() == OxygenConfiguration::BorderNone && isPreview() && !compositingActive() ) {

          border = 1;

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
      return OxygenFactory::shadowSize() - extraBorder;

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

    if( configuration().drawTitleOutline() )
    {

      return options()->color(ColorFont, isActive());

    } else if (isActive()) {

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
    if (clipRect.isValid()) {
      painter->setClipRegion(clipRect,Qt::IntersectClip);
    }

    QRect r = (isPreview()) ? OxygenClient::widget()->rect():window->rect();
    r.adjust( OxygenFactory::shadowSize(), OxygenFactory::shadowSize(), -OxygenFactory::shadowSize(), -OxygenFactory::shadowSize() );
    r.adjust(0,0, 1, 1);

    // mask and painting frame
    QRegion mask;
    QRect frame;

    // top line
    {
      int shadow_size = 5;
      int height = HFRAMESIZE;
      QRect rect( r.topLeft()-position, QSize( r.width(), height ) );
      helper().slab( palette.color( widget->backgroundRole() ), 0, shadow_size )->render( rect.adjusted(-shadow_size-1, 0, shadow_size+1, 2 ), painter, TileSet::Bottom );
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
  void OxygenClient::renderTitleOutline(  QPainter* painter, const QRect& rect, const QPalette& palette ) const
  {

    // shadow
    {
      int shadow_size = 7;
      int voffset = -shadow_size;
      if( !isMaximized() ) voffset += HFRAMESIZE;
      helper().slab( palette.color( widget()->backgroundRole() ), 0, shadow_size )->render( rect.adjusted(0, voffset, 0, 0 ), painter, TileSet::Bottom|TileSet::Left|TileSet::Right );
    }

    // center
    {
      int voffset = isMaximized() ? 0:HFRAMESIZE;
      renderWindowBackground(painter, rect.adjusted( 4, voffset, -4, -4 ), widget(), palette );
    }

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
      shadowTiles(
        backgroundPalette( widget(), palette ).color( widget()->backgroundRole() ),
        isActive() )->render( frame.adjusted( 4, 4, -4, -4),
        &painter, TileSet::Ring);
    }

    // adjust frame
    frame.adjust( OxygenFactory::shadowSize(), OxygenFactory::shadowSize(), -OxygenFactory::shadowSize(), -OxygenFactory::shadowSize() );

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
    if( isActive() && configuration().drawTitleOutline() && !isMaximized() )
    {
      renderWindowBorder( &painter, frame, widget(), backgroundPalette( widget(), palette ) );
    }

    // clipping
    if( compositingActive() ) painter.setClipping(false);

    // in preview mode and when frame border is 0,
    // one still draw a small rect around, unless kde is recent enough,
    // useOxygenShadow is set to true,
    // and copositing is active
    // (that makes a lot of ifs)
    if( isPreview() && configuration().frameBorder() == OxygenConfiguration::BorderNone && !compositingActive() )
    {
      painter.save();
      painter.setBrush( Qt::NoBrush );
      painter.setPen( QPen( helper().calcDarkColor( widget()->palette().window().color() ), 1 ) );

      QPainterPath path;
      QPoint first( frame.topLeft()+QPoint( 0, 6 ) );
      path.moveTo( first );
      path.quadTo( frame.topLeft(), frame.topLeft()+QPoint( 6, 0 ) );
      path.lineTo( frame.topRight()-QPoint( 6, 0 ) );
      path.quadTo( frame.topRight(), frame.topRight()+QPoint( 0, 6 ) );
      path.lineTo( frame.bottomRight() );
      path.lineTo( frame.bottomLeft() );
      path.lineTo( first );
      painter.drawPath( path );
      painter.restore();
    }

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

    if( isActive() && configuration().drawTitleOutline() )
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

    // draw title text
    painter.setPen( titlebarTextColor( backgroundPalette( widget(), palette ) ) );

    painter.drawText( titleRect, configuration().titleAlignment() | Qt::AlignVCenter, caption() );
    painter.setRenderHint(QPainter::Antialiasing);

    // adjust if there are shadows
    if (compositingActive()) frame.adjust(-1,-1, 1, 1);

    // dimensions
    int x,y,w,h;
    frame.getRect(&x, &y, &w, &h);

    // separator
    if( drawSeparator() )
    { helper().drawSeparator(&painter, QRect(x, titleTop+titleHeight-1.5, w, 2), color, Qt::Horizontal); }

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

  //________________________________________________________________
  OxygenConfiguration OxygenClient::configuration( void ) const
  { return configuration_; }

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

  //_________________________________________________________________
  TileSet *OxygenClient::shadowTiles(const QColor& color, bool active)
  {

    OxygenShadowConfiguration shadowConfiguration( OxygenFactory::shadowConfiguration( active && useOxygenShadows() ) );

    ShadowTilesOption opt;
    opt.active      = active;
    opt.width       = shadowConfiguration.shadowSize();
    opt.windowColor = color;
    opt.innerColor   = shadowConfiguration.innerColor();

    ShadowTilesOption currentOpt = active ? activeShadowTilesOption_:inactiveShadowTilesOption_;

    bool optionChanged = !(currentOpt == opt );
    if (active && activeShadowTiles_ )
    {

      if( optionChanged) delete activeShadowTiles_;
      else return activeShadowTiles_;

    } else if (!active && inactiveShadowTiles_ ) {

      if( optionChanged ) delete inactiveShadowTiles_;
      else return inactiveShadowTiles_;

    }

    kDebug( 1212 ) << " creating tiles - active: " << active << endl;
    TileSet *tileSet = 0;

    //
    qreal size( OxygenFactory::shadowSize() );
    if( active && configuration().drawTitleOutline() && configuration().frameBorder() == OxygenConfiguration::BorderNone )
    {

      // a more complex tile set is needed for the configuration above:
      // top corners must be beveled with the "active titlebar color" while
      // bottom corners must be beveled with the "window background color".
      // this requires generating two shadow pixmaps and tiling them in the tileSet.
      QPixmap shadow = QPixmap( size*2, size*2 );
      shadow.fill( Qt::transparent );

      QPainter p( &shadow );
      p.setRenderHint( QPainter::Antialiasing );

      QPixmap shadowTop = shadowPixmap( color, active );
      QRect topRect( shadow.rect() );
      topRect.setBottom( int( size )-1 );
      p.setClipRect( topRect );
      p.drawPixmap( QPointF( 0, 0 ), shadowTop );

      QPixmap shadowBottom = shadowPixmap( widget()->palette().color( widget()->backgroundRole() ), active );
      QRect bottomRect( shadow.rect() );
      bottomRect.setTop( int( size ) );
      p.setClipRect( bottomRect );
      p.drawPixmap( QPointF( 0, 0 ), shadowBottom );
      p.end();

      tileSet = new TileSet( shadow, size, size, 1, 1);

    } else {

      tileSet = new TileSet(
        shadowPixmap( color, active ),
        size, size, 1, 1);

    }

    // store option and style
    if( active )
    {

      activeShadowTilesOption_ = opt;
      activeShadowTiles_ = tileSet;

    } else {

      inactiveShadowTilesOption_ = opt;
      inactiveShadowTiles_ = tileSet;

    }

    return tileSet;
  }

  //_________________________________________________________________
  QPixmap OxygenClient::shadowPixmap(const QColor& color, bool active ) const
  {

    OxygenShadowConfiguration shadowConfiguration( OxygenFactory::shadowConfiguration( active && useOxygenShadows() ) );

    static const qreal fixedSize = 25.5;
    qreal size( OxygenFactory::shadowSize() );
    qreal shadowSize( shadowConfiguration.shadowSize() );

    QPixmap shadow = QPixmap( size*2, size*2 );
    shadow.fill( Qt::transparent );

    QPainter p( &shadow );
    p.setRenderHint( QPainter::Antialiasing );
    p.setPen( Qt::NoPen );

    // offsets are scaled with the shadow size
    // so that the ratio Top-shadow/Bottom-shadow is kept constant
    // when shadow size is chqnged
    qreal hoffset = shadowConfiguration.horizontalOffset()*shadowSize/fixedSize;
    qreal voffset = shadowConfiguration.verticalOffset()*shadowSize/fixedSize;

    if( active && useOxygenShadows() )
    {

      {

        int values[5] = {255, 220, 180, 25, 0};
        qreal x[5] = {4.4, 4.5, 5, 5.5, 6.5};

        // the first point of this gradient does not scale
        QRadialGradient rg( size, size, shadowSize );

        QColor c = color;
        for( int i = 0; i<5; i++ )
        { c.setAlpha( values[i] ); rg.setColorAt( x[0]/shadowSize+(x[i]-x[0])/fixedSize, c ); }

        p.setBrush( rg );
        p.drawRect( shadow.rect() );

      }

      {

        // this gradient scales with shadow size
        int nPoints = 8;
        qreal values[8] = {1, 0.58, 0.43, 0.30, 0.22, 0.15, 0.08, 0 };
        qreal x[8] = {0, 4.5, 5.5, 6.5, 7.5, 8.5, 11.5, 14.4};

        // this gradient scales with shadow size
        QRadialGradient rg(
          size+hoffset,
          size+voffset,
          shadowSize );

        QColor c = shadowConfiguration.innerColor();
        for( int i = 0; i<nPoints; i++ )
        { c.setAlphaF( values[i] ); rg.setColorAt( x[i]/fixedSize, c ); }

        p.setBrush( rg );
        p.drawRect( shadow.rect() );

      }

      {
        QRadialGradient rg = QRadialGradient( size, size, size );
        QColor c = color;

        c.setAlpha( 255 );  rg.setColorAt( 4.0/size, c );
        c.setAlpha( 0 );  rg.setColorAt( 4.01/size, c );

        p.setBrush( rg );
        p.drawRect( shadow.rect() );
      }

    } else {


      {

        int nPoints = 9;
        qreal values[9] = { 0.17, 0.12, 0.11, 0.075, 0.06, 0.035, 0.025, 0.01, 0 };
        qreal x[9] = {0, 4.5, 6.6, 8.5, 11.5, 14.5, 17.5, 21.5, 25.5 };
        QRadialGradient rg = QRadialGradient(
          size+20.0*hoffset,
          size+20.0*voffset,
          shadowSize );

        QColor c = shadowConfiguration.outerColor();
        for( int i = 0; i<nPoints; i++ )
        { c.setAlphaF( values[i] ); rg.setColorAt( x[i]/fixedSize, c ); }

        p.setBrush( rg );
        p.drawRect( shadow.rect()  );

      }

      {

        int nPoints = 7;
        qreal values[7] = {0.55, 0.25, 0.20, 0.1, 0.06, 0.015, 0 };
        qreal x[7] = {0, 4.5, 5.5, 7.5, 8.5, 11.5, 14.5 };
        QRadialGradient rg = QRadialGradient(
          size+10.0*hoffset,
          size+10.0*voffset,
          shadowSize );

        QColor c = shadowConfiguration.midColor();
        for( int i = 0; i<nPoints; i++ )
        { c.setAlphaF( values[i] ); rg.setColorAt( x[i]/fixedSize, c ); }

        p.setBrush( rg );
        p.drawRect( shadow.rect() );

      }

      {
        int nPoints = 5;
        qreal values[5] = { 1, 0.32, 0.22, 0.03, 0 };
        qreal x[5] = { 0, 4.5, 5.0, 5.5, 6.5 };
        QRadialGradient rg = QRadialGradient(
          size+hoffset,
          size+voffset,
          shadowSize );

        QColor c = shadowConfiguration.innerColor();
        for( int i = 0; i<nPoints; i++ )
        { c.setAlphaF( values[i] ); rg.setColorAt( x[i]/fixedSize, c ); }

        p.setBrush( rg );
        p.drawRect( shadow.rect() );

      }

      {
        QRadialGradient rg = QRadialGradient( size, size, size );
        QColor c = color;

        c.setAlpha( 255 );  rg.setColorAt( 4.0/size, c );
        c.setAlpha( 0 );  rg.setColorAt( 4.01/size, c );

        p.setBrush( rg );
        p.drawRect( shadow.rect() );
      }

    }

    // draw the corner of the window - actually all 4 corners as one circle
    // this is all fixedSize. Does not scale with shadow size
    QLinearGradient lg = QLinearGradient(0.0, size-4.5, 0.0, size+4.5);
    if( configuration().frameBorder() < OxygenConfiguration::BorderTiny )
    {

      lg.setColorAt(0.52, helper().backgroundTopColor(color));
      lg.setColorAt(1.0, helper().backgroundBottomColor(color) );

    } else {

      QColor light = helper().calcLightColor( helper().backgroundTopColor(color) );
      QColor dark = helper().calcDarkColor(helper().backgroundBottomColor(color));

      lg.setColorAt(0.52, light);
      lg.setColorAt(1.0, dark);

    }

    p.setBrush( Qt::NoBrush );
    if( configuration().frameBorder() == OxygenConfiguration::BorderNone )
    { p.setClipRect( QRectF( 0, 0, 2*size, size ) ); }

    p.setPen(QPen(lg, 0.8));
    p.drawEllipse(QRectF(size-4, size-4, 8, 8));

    p.end();
    return shadow;
  }

}
