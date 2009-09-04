//////////////////////////////////////////////////////////////////////////////
// nitrogenclient.cpp
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

#include <cassert>
#include <cmath>
#include <iostream>

#include <kdeversion.h>
#include <KGlobal>
#include <KLocale>
#include <KColorUtils>

#include <QPainter>
#include <QTextStream>
#include <QApplication>

#include "nitrogen.h"
#include "nitrogenbutton.h"
#include "nitrogenclient.h"
#include "nitrogenclient.moc"
#include "nitrogensizegrip.h"

using namespace std;
namespace Nitrogen
{
  
  K_GLOBAL_STATIC_WITH_ARGS(NitrogenHelper, globalHelper, ("nitrogenDeco"))
    
  //___________________________________________
  NitrogenHelper *nitrogenHelper()
  { return globalHelper; }
  
  //___________________________________________
  static void oxkwincleanupBefore()
  {
    NitrogenHelper *h = globalHelper;
    h->invalidateCaches();
  }
  
  //___________________________________________
  void renderDot(QPainter *p, const QPointF &point, qreal diameter)
  {
    p->drawEllipse(QRectF(point.x()-diameter/2, point.y()-diameter/2, diameter, diameter));
  }
  
  //___________________________________________
  NitrogenClient::NitrogenClient(KDecorationBridge *b, KDecorationFactory *f): 
    KCommonDecorationUnstable(b, f), 
    colorCacheInvalid_(true),
    size_grip_( 0 ),
    shadowTiles_( 0 ),
    glowTiles_( 0 ),
    helper_(*globalHelper),
    initialized_( false )
  { qAddPostRoutine(oxkwincleanupBefore); }
  
  //___________________________________________
  NitrogenClient::~NitrogenClient()
  {
    
    // delete sizegrip if any
    if( hasSizeGrip() ) deleteSizeGrip();
    
    // delete tilesets
    if( shadowTiles_ ) delete shadowTiles_; 
    if( glowTiles_ ) delete glowTiles_;
    
  }
  
  //___________________________________________
  QString NitrogenClient::visibleName() const
  { return i18n("nitrogen"); }
  
  //___________________________________________
  void NitrogenClient::init()
  {
    
    KCommonDecoration::init();
    widget()->setAttribute(Qt::WA_NoSystemBackground );
    widget()->setAutoFillBackground( false );
    initialized_ = true;
    
    resetConfiguration();
    
  }
  
  //___________________________________________
  bool NitrogenClient::isMaximized() const
  { return maximizeMode()==MaximizeFull && !options()->moveResizeMaximizedWindows();  }
  
  //___________________________________________
  bool NitrogenClient::decorationBehaviour(DecorationBehaviour behaviour) const
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
  int NitrogenClient::layoutMetric(LayoutMetric lm, bool respectWindowState, const KCommonDecorationButton *btn) const
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
        }  else if( configuration().frameBorder() == NitrogenConfiguration::BorderNone && isPreview() && !compositingActive() ) {
          border = 1;
        }  else if( lm == LM_BorderBottom && frameBorder >= NitrogenConfiguration::BorderTiny ) {
            
          // for tiny border, the convention is to have a larger bottom area in order to 
          // make resizing easier
          border = qMax(frameBorder, 4);
            
        } else {
            
          border = frameBorder;          
            
        }
          
        return border + extraBorder;
      }
      
      case LM_TitleEdgeTop:
      {
        int border = 0;
        if( !( respectWindowState && maximized )) 
        { border = TFRAMESIZE; }
        
        return border + extraBorder;
        
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
        
        return border + extraBorder;
        
      }
      
      case LM_TitleBorderLeft:
      case LM_TitleBorderRight:
      return 5;
      
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
      return SHADOW_WIDTH - extraBorder;
      
      default:
      return KCommonDecoration::layoutMetric(lm, respectWindowState, btn);
    }
    
  }
  
  //_________________________________________________________
  KCommonDecorationButton *NitrogenClient::createButton(::ButtonType type)
  {
    switch (type) {
      case MenuButton:
      return new NitrogenButton(*this, i18n("Menu"), ButtonMenu);
      
      case HelpButton:
      return new NitrogenButton(*this, i18n("Help"), ButtonHelp);
      
      case MinButton:
      return new NitrogenButton(*this, i18n("Minimize"), ButtonMin);
      
      case MaxButton:
      return new NitrogenButton(*this, i18n("Maximize"), ButtonMax);
      
      case CloseButton:
      return new NitrogenButton(*this, i18n("Close"), ButtonClose);
      
      case AboveButton:
      return new NitrogenButton(*this, i18n("Keep Above Others"), ButtonAbove);
      
      case BelowButton:
      return new NitrogenButton(*this, i18n("Keep Below Others"), ButtonBelow);
      
      case OnAllDesktopsButton:
      return new NitrogenButton(*this, i18n("On All Desktops"), ButtonSticky);
      
      case ShadeButton:
      return new NitrogenButton(*this, i18n("Shade Button"), ButtonShade);
      
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
  QRegion NitrogenClient::calcMask( void ) const
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
    
    // disable bottom corners when border frame is too small
    if( configuration().frameBorder() < NitrogenConfiguration::BorderTiny ) bottom = 0;
    
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
  QColor NitrogenClient::titlebarTextColor(const QPalette &palette)
  {
    
    if( !configuration().overwriteColors() ) 
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
  void NitrogenClient::renderWindowBackground( QPainter* painter, const QRect& rect, const QWidget* widget, const QPalette& palette ) const
  {
    
    if( configuration().blendColor() == NitrogenConfiguration::NoBlending ) 
    { 
      
      painter->fillRect( rect, backgroundPalette( widget, palette ).color( widget->window()->backgroundRole() ) );
      
    } else { 
      
      int offset( configuration().buttonSize() - 22 );
      
      offset += layoutMetric( LM_OuterPaddingBottom );      
      helper().renderWindowBackground(painter, rect, widget, backgroundPalette( widget, palette ), offset );
      
    }  
    
  }
  
  //_________________________________________________________
  void NitrogenClient::activeChange( void )
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
  void NitrogenClient::shadeChange( void  )
  { 
    if( hasSizeGrip() ) sizeGrip().setVisible( !isShade() ); 
    KCommonDecorationUnstable::shadeChange();
  }

  //_________________________________________________________
  QPalette NitrogenClient::backgroundPalette( const QWidget* widget, QPalette palette ) const
  {    
    if( !configuration().overwriteColors() )
    { palette.setColor( widget->window()->backgroundRole(), options()->color( KDecorationDefines::ColorTitleBar, isActive() ) ); }
    
    return palette;
    
  }
  
  //________________________________________________________________
  void NitrogenClient::drawStripes(QPainter *p, QPalette &palette, const int start, const int end, const int topMargin)
  {
    
    QColor color( KDecoration::options()->color(ColorTitleBar) );
    
    QLinearGradient stripeGradient(QPoint(start,0), QPoint(end,0));
    stripeGradient.setColorAt(0.0, Qt::transparent);
    stripeGradient.setColorAt(0.05, KDecoration::options()->color(ColorTitleBar));
    stripeGradient.setColorAt(1.0, Qt::transparent);
    QPen pen1(stripeGradient, 0.5);

    QLinearGradient stripeGradient2(QPoint(start,0), QPoint(end,0));
    stripeGradient2.setColorAt(0.0, Qt::transparent);
    stripeGradient2.setColorAt(0.05, helper_.calcLightColor(palette.color(QPalette::Window)));
    stripeGradient2.setColorAt(1.0, Qt::transparent);
    QPen pen2(stripeGradient2, 0.5);
        
    bool antialiasing = p->testRenderHint(QPainter::Antialiasing);
    p->setRenderHint(QPainter::Antialiasing, false);
    const int titleHeight = layoutMetric(LM_TitleHeight);
    double voffset = 4.0*configuration().buttonSize()/22;
    double scale = double(titleHeight-voffset)/4;
    double base = titleHeight + topMargin;
    for (int i = 0; i < 3; ++i)
    {
      
      // calculate offset for each stripe. 
      int offset = int(base - voffset) - int(scale)*(i+1);
      p->setPen(pen1);
      p->drawLine(QPoint(start, offset), QPoint(end, offset));
      p->setPen(pen2);
      p->drawLine(QPoint(start, offset+1), QPoint(end, offset+1));
    }
    p->setRenderHint(QPainter::Antialiasing, antialiasing);
  }
  
  //________________________________________________________________
  void NitrogenClient::updateWindowShape()
  { 
    
    if(isMaximized() || compositingActive() )
    {
    
      clearMask();
    
    } else {
      
      setMask( calcMask() ); 
      
    }
    
  }
  
  
  //___________________________________________
  void NitrogenClient::resetConfiguration( void )
  { 
    
    if( !initialized_ ) return;
    
    configuration_ = NitrogenFactory::configuration( *this );     
    
    // handle size grip
    if( configuration_.drawSizeGrip() ) 
    {
      
      if( !hasSizeGrip() ) createSizeGrip(); 
      
    } else if( hasSizeGrip() ) deleteSizeGrip();
    
  }
  
  //_________________________________________________________
  void NitrogenClient::paintEvent( QPaintEvent* event )
  {
    
    
    // factory
    if(!( initialized_ && NitrogenFactory::initialized() ) ) return;
    
    // palette
    QPalette palette = widget()->palette();
    palette.setCurrentColorGroup( (isActive() ) ? QPalette::Active : QPalette::Inactive );
    
    // painter
    QPainter painter(widget());
    painter.setClipRegion( event->region() );
    
    // define frame
    QRect frame = widget()->rect();
    
    // base color
    QColor color = ( configuration().overwriteColors() ) ? 
      palette.window().color() : 
      options()->color( ColorTitleBar, isActive());
    
    // draw shadows
    if( compositingActive() && !isMaximized() )
    {
      shadowTiles(
        color,KDecoration::options()->color(ColorTitleBar),
        SHADOW_WIDTH, configuration().useOxygenShadows() && isActive() )->render( frame.adjusted( 4, 4, -4, -4),
        &painter, TileSet::Ring);
      
    }
    
    // adjust frame
    frame.adjust( SHADOW_WIDTH, SHADOW_WIDTH, -SHADOW_WIDTH, -SHADOW_WIDTH );
    
    //  adjust mask
    if( compositingActive() || isPreview() )
    {
    
      if( isMaximized() ) {
        
        painter.setClipRect( frame ); 
      
      } else {
        
        int x, y, w, h;
        frame.getRect(&x, &y, &w, &h);
        
        // multipliers
        int left = 1;
        int right = 1;
        int top = 1;
        int bottom = 1;
      
        // disable bottom corners when border frame is too small
        if( configuration().frameBorder() < NitrogenConfiguration::BorderTiny ) bottom = 0;
        QRegion mask( x+5*left,   y+0*top, w-5*(left+right), h-0*(top+bottom));
        mask += QRegion(x+0*left, y+5*top, w-0*(left+right), h-5*(top+bottom));
        mask += QRegion(x+2*left, y+2*top, w-2*(left+right), h-2*(top+bottom));
        mask += QRegion(x+3*left, y+1*top, w-3*(left+right), h-1*(top+bottom));
        mask += QRegion(x+1*left, y+3*top, w-1*(left+right), h-3*(top+bottom));      
      
        painter.setClipRegion( mask );
        
      }
      
    
    }
    
    // window background
    renderWindowBackground( &painter, frame, widget(), palette );
    
    // clipping
    if( compositingActive() ) painter.setClipping(false);

    // in preview mode and when frame border is 0, 
    // one still draw a small rect around, unless kde is recent enough,
    // useOxygenShadow is set to true, 
    // and copositing is active
    // (that makes a lot of ifs)
    if( isPreview() && configuration().frameBorder() == NitrogenConfiguration::BorderNone && !compositingActive() )
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
    
    // draw title text    
    painter.setPen( titlebarTextColor( backgroundPalette( widget(), palette ) ) );
    painter.setFont( options()->font(isActive(), false) );
    
    painter.drawText(titleLeft, titleTop-1, titleWidth, titleHeight, configuration().titleAlignment() | Qt::AlignVCenter, caption() );
    painter.setRenderHint(QPainter::Antialiasing);
    
    // adjust if there are shadows
    if (compositingActive()) frame.adjust(-1,-1, 1, 1);
    
    // dimensions
    int x,y,w,h;
    frame.getRect(&x, &y, &w, &h);
    
    // separator
    if( isActive() && configuration().drawSeparator() ) 
    { helper().drawSeparator(&painter, QRect(x, titleTop+titleHeight-1.5, w, 2), color, Qt::Horizontal); }
    
    // draw stripes as indicator for active windows
    if( isActive() && configuration().showStripes() )
    {
      
      Qt::Alignment align = configuration().titleAlignment();
      if (widget()->layoutDirection() == Qt::RightToLeft)
      {
        
        if (align == Qt::AlignLeft) align = Qt::AlignRight;
        else if (align == Qt::AlignRight) align = Qt::AlignLeft;
        
      }
      
      if (align & Qt::AlignLeft) {
        
        int left = titleLeft + QFontMetrics(options()->font(isActive(), false)).width( caption() ) + 4;
        int right = titleLeft + titleWidth;
        drawStripes(&painter, palette, left, right, titleTop);
        
      } else if (align & Qt::AlignRight) {
        
        int left = titleLeft;
        int right = titleLeft + titleWidth - QFontMetrics(options()->font(isActive(), false)).width( caption() ) - 4;
        drawStripes(&painter, palette, right, left, titleTop);
        
      } else if (align & Qt::AlignHCenter) {
        
        int textWidth = QFontMetrics(options()->font(isActive(), false)).width( caption() );
        int left = titleLeft;
        int centerLeft = titleLeft + titleWidth/2 - textWidth/2 - 4;
        int centerRight = titleLeft + titleWidth/2 + textWidth/2 + 4;
        int right = titleLeft + titleWidth;
        drawStripes(&painter, palette, centerLeft, left, titleTop);
        drawStripes(&painter, palette, centerRight, right, titleTop);
        
      }
      
    }
    
    // shadow and resize handles
    if( configuration().frameBorder() >= NitrogenConfiguration::BorderTiny && !isMaximized() )
    {
      
      helper().drawFloatFrame(
        &painter, frame, color, !compositingActive(), isActive(),
        KDecoration::options()->color(ColorTitleBar)
        );

      if( isResizable() )
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
      if( isResizable() && !configuration().drawSizeGrip() )
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
  NitrogenConfiguration NitrogenClient::configuration( void ) const
  { return configuration_; }
  
  //_________________________________________________________________
  void NitrogenClient::createSizeGrip( void )
  {
    
    assert( !hasSizeGrip() );
    if( ( isResizable() && windowId() != 0 ) || isPreview() )
    { size_grip_ = new NitrogenSizeGrip( this ); }
    
  }
  
  //_________________________________________________________________
  void NitrogenClient::deleteSizeGrip( void )
  {
    assert( hasSizeGrip() );
    size_grip_->deleteLater();
    size_grip_ = 0;
  }    
  
  //_________________________________________________________________
  TileSet *NitrogenClient::shadowTiles(const QColor& color, const QColor& glow, qreal size, bool active)
  {
    ShadowTilesOption opt;
    opt.active      = active;
    opt.width       = size;
    opt.windowColor = color;
    opt.glowColor   = glow;
    
    ShadowTilesOption currentOpt = active ? shadowTilesOption_ : glowTilesOption_;
    
    bool optionChanged = true;
    if (currentOpt.active == opt.active
      && currentOpt.width == opt.width
      && opt.windowColor == opt.windowColor
      && opt.glowColor == opt.glowColor)
      optionChanged = false;
    
    if (active && glowTiles_ )
    { 
    
      if( optionChanged) delete glowTiles_;
      else return glowTiles_;
      
    } else if (!active && shadowTiles_ ) {
      
      if( optionChanged ) delete shadowTiles_;
      else return shadowTiles_;
      
    }
    
    TileSet *tileSet = 0;
    
    //---------------------------------------------------------------
    // Create new glow/shadow tiles
    
    QColor light = helper().calcLightColor( helper().backgroundTopColor(color) );
    QColor dark = helper().calcDarkColor(helper().backgroundBottomColor(color));
    
    QPixmap shadow = QPixmap( size*2, size*2 );
    shadow.fill( Qt::transparent );
    
    // draw the corner of the window - actually all 4 corners as one circle
    QLinearGradient lg = QLinearGradient(0.0, size-4.5, 0.0, size+4.5);
    if( configuration().frameBorder() < NitrogenConfiguration::BorderTiny )
    {
      
      lg.setColorAt(0, helper().backgroundTopColor(color) );
      lg.setColorAt(0.52, helper().backgroundTopColor(color));
      lg.setColorAt(1.0, helper().backgroundBottomColor(color) );
      
    } else {
      
      lg.setColorAt(0.52, light);
      lg.setColorAt(1.0, dark);
    
    }
    
    QPainter p( &shadow );
    p.setRenderHint( QPainter::Antialiasing );
    p.setPen( Qt::NoPen );
    
    if (active)
    {
      //---------------------------------------------------------------
      // Active shadow texture
      
      QRadialGradient rg( size, size, size );
      QColor c = color;
      c.setAlpha( 255 );  rg.setColorAt( 4.4/size, c );
      c.setAlpha( 220 );  rg.setColorAt( 4.5/size, c );
      c.setAlpha( 180 );  rg.setColorAt( 5/size, c );
      c.setAlpha( 25 );  rg.setColorAt( 5.5/size, c );
      c.setAlpha( 0 );  rg.setColorAt( 6.5/size, c );
      
      p.setBrush( rg );
      p.drawRect( shadow.rect() );
      
      rg = QRadialGradient( size, size, size );
      c = color;
      c.setAlpha( 255 );  rg.setColorAt( 4.4/size, c );
      c = glow;
      c.setAlpha( 0.58*255 );  rg.setColorAt( 4.5/size, c );
      c.setAlpha( 0.43*255 );  rg.setColorAt( 5.5/size, c );
      c.setAlpha( 0.30*255 );  rg.setColorAt( 6.5/size, c );
      c.setAlpha( 0.22*255 );  rg.setColorAt( 7.5/size, c );
      c.setAlpha( 0.15*255 );  rg.setColorAt( 8.5/size, c );
      c.setAlpha( 0.08*255 );  rg.setColorAt( 11.5/size, c );
      c.setAlpha( 0);  rg.setColorAt( 14.5/size, c );
      p.setRenderHint( QPainter::Antialiasing );
      p.setBrush( rg );
      p.drawRect( shadow.rect() );
      
      p.setBrush( Qt::NoBrush );
      p.setPen(QPen(lg, 0.8));
      p.drawEllipse(QRectF(size-4, size-4, 8, 8));
      
      p.end();
      
      tileSet = new TileSet(shadow, size, size, 1, 1);
      glowTilesOption_ = opt;
      glowTiles_ = tileSet;
    } else {
      
      //---------------------------------------------------------------
      // Inactive shadow texture
      
      QRadialGradient rg = QRadialGradient( size, size+4, size );
      QColor c = QColor( Qt::black );
      c.setAlpha( 0.12*255 );  rg.setColorAt( 4.5/size, c );
      c.setAlpha( 0.11*255 );  rg.setColorAt( 6.6/size, c );
      c.setAlpha( 0.075*255 );  rg.setColorAt( 8.5/size, c );
      c.setAlpha( 0.06*255 );  rg.setColorAt( 11.5/size, c );
      c.setAlpha( 0.035*255 );  rg.setColorAt( 14.5/size, c );
      c.setAlpha( 0.025*255 );  rg.setColorAt( 17.5/size, c );
      c.setAlpha( 0.01*255 );  rg.setColorAt( 21.5/size, c );
      c.setAlpha( 0.0*255 );  rg.setColorAt( 25.5/size, c );
      p.setRenderHint( QPainter::Antialiasing );
      p.setPen( Qt::NoPen );
      p.setBrush( rg );
      p.drawRect( shadow.rect() );
      
      rg = QRadialGradient( size, size+2, size );
      c = QColor( Qt::black );
      c.setAlpha( 0.25*255 );  rg.setColorAt( 4.5/size, c );
      c.setAlpha( 0.20*255 );  rg.setColorAt( 5.5/size, c );
      c.setAlpha( 0.13*255 );  rg.setColorAt( 7.5/size, c );
      c.setAlpha( 0.06*255 );  rg.setColorAt( 8.5/size, c );
      c.setAlpha( 0.015*255 );  rg.setColorAt( 11.5/size, c );
      c.setAlpha( 0.0*255 );  rg.setColorAt( 14.5/size, c );
      p.setRenderHint( QPainter::Antialiasing );
      p.setPen( Qt::NoPen );
      p.setBrush( rg );
      p.drawRect( shadow.rect() );
      
      rg = QRadialGradient( size, size+0.2, size );
      c = color;
      c = QColor( Qt::black );
      c.setAlpha( 0.35*255 );  rg.setColorAt( 0/size, c );
      c.setAlpha( 0.32*255 );  rg.setColorAt( 4.5/size, c );
      c.setAlpha( 0.22*255 );  rg.setColorAt( 5.0/size, c );
      c.setAlpha( 0.03*255 );  rg.setColorAt( 5.5/size, c );
      c.setAlpha( 0.0*255 );  rg.setColorAt( 6.5/size, c );
      p.setRenderHint( QPainter::Antialiasing );
      p.setPen( Qt::NoPen );
      p.setBrush( rg );
      p.drawRect( shadow.rect() );
      
      rg = QRadialGradient( size, size, size );
      c = color;
      c.setAlpha( 255 );  rg.setColorAt( 4.0/size, c );
      c.setAlpha( 0 );  rg.setColorAt( 4.01/size, c );
      p.setRenderHint( QPainter::Antialiasing );
      p.setPen( Qt::NoPen );
      p.setBrush( rg );
      p.drawRect( shadow.rect() );
      
      // draw the corner of the window - actually all 4 corners as one circle
      p.setBrush( Qt::NoBrush );
      p.setPen(QPen(lg, 0.8));
      p.drawEllipse(QRectF(size-4, size-4, 8, 8));
      
      p.end();
      
      tileSet = new TileSet(shadow, size, size, 1, 1);
      shadowTilesOption_ = opt;
      shadowTiles_       = tileSet;
    }
    
    return tileSet;
  }
  
}
