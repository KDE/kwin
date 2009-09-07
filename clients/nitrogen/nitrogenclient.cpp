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
#include <KDebug>

#include <QLabel>
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
  
  K_GLOBAL_STATIC_WITH_ARGS(OxygenHelper, globalHelper, ("nitrogenDeco"))
    
  //___________________________________________
  OxygenHelper *nitrogenHelper()
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
  { return i18n("Nitrogen"); }
  
  //___________________________________________
  void NitrogenClient::init()
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
      for( QList<QLabel*>::iterator iter = children.begin(); iter != children.end(); iter++ )
      { (*iter)->setAutoFillBackground( false ); }
      
    }
    
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
    
    // disable bottom corners when border frame is too small and window is not shaded
    if( configuration().frameBorder() < NitrogenConfiguration::BorderTiny && !isShade() ) bottom = 0;
    
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
  void NitrogenClient::renderWindowBackground( QPainter* painter, const QRect& rect, const QWidget* widget, const QPalette& palette ) const
  {
    
    if( configuration().blendColor() == NitrogenConfiguration::NoBlending ) 
    { 
      
      painter->fillRect( rect, palette.color( widget->window()->backgroundRole() ) );
      
    } else { 
      
      int offset = layoutMetric( LM_OuterPaddingTop );
      int height = 64 + configuration().buttonSize() - 22;
      const QWidget* window( isPreview() ? NitrogenClient::widget() : widget->window() );
      helper().renderWindowBackground(painter, rect, widget, window, palette, offset, height );
      
    }  
    
  }
  
  //_________________________________________________________
  void NitrogenClient::renderWindowBorder( QPainter* painter, const QRect& clipRect, const QWidget* widget, const QPalette& palette ) const
  {
    
    const QWidget* window = (isPreview()) ? NitrogenClient::widget() : widget->window();
    
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
    if (clipRect.isValid()) {
      painter->save();
      painter->setClipRegion(clipRect,Qt::IntersectClip);
    }
    
    painter->setPen( Qt::NoPen );
    
    QColor color = palette.color(window->backgroundRole());
    QColor top = helper().backgroundTopColor( color );
    QColor bottom = helper().backgroundBottomColor( color );
    
    QRect r = (isPreview()) ? NitrogenClient::widget()->rect():window->rect();
    r.adjust( SHADOW_WIDTH, SHADOW_WIDTH, -SHADOW_WIDTH, -SHADOW_WIDTH );
    r.adjust(0,0, 1, 1);
    
    // draw top line
    {
      
      int shadow_size = 5;
      int height = HFRAMESIZE;
      QRect rect( r.topLeft()-position, QSize( r.width(), height ) );
      helper().slab( palette.color( widget->backgroundRole() ), 0, shadow_size )->render( rect.adjusted(-shadow_size-1, 0, shadow_size+1, 2 ), painter, TileSet::Bottom );
      
      int offset = layoutMetric( LM_OuterPaddingTop );
      int gradient_height = 64 + configuration().buttonSize() - 22;
      const QWidget* window( isPreview() ? NitrogenClient::widget() : widget->window() );
      helper().renderWindowBackground(painter, rect, widget, window, palette, offset, gradient_height );
      
    }
    
    // draw bottom line
    if( configuration().frameBorder() >= NitrogenConfiguration::BorderTiny )
    {
      int height = qMin( HFRAMESIZE, layoutMetric( LM_BorderBottom ) );
      painter->setBrush( bottom );
      painter->drawRect( QRect( r.bottomLeft()-position-QPoint(0,height), QSize( r.width(), height ) ) );
    }
    
    // left and right
    if( configuration().frameBorder() >= NitrogenConfiguration::BorderTiny )
    {
      
      QLinearGradient gradient(0, r.top(), 0, r.height() );
      gradient.setColorAt(0.0, top);
      gradient.setColorAt(0.5, color);
      gradient.setColorAt(1.0, bottom);
      
      painter->setBrush( gradient );
      
      // left
      {
        int width = qMin( HFRAMESIZE, layoutMetric( LM_BorderLeft ) );
        painter->drawRect( QRect( r.topLeft()-position, QSize( width, r.height() ) ) );
      }
      
      // right
      {
        int width = qMin( HFRAMESIZE, layoutMetric( LM_BorderRight ) );
        painter->drawRect( QRect( r.topRight()-position-QPoint(width,0), QSize( width, r.height() ) ) );
      }
      
    }
    
    // restore painter
    if (clipRect.isValid()) painter->restore();

  }
  
  //_________________________________________________________
  void NitrogenClient::renderTitleOutline(  QPainter* painter, const QRect& rect, const QPalette& palette ) const
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
      int offset = layoutMetric( LM_OuterPaddingTop );
      int height = 64 + configuration().buttonSize() - 22;
      int voffset = isMaximized() ? 0:HFRAMESIZE;
      const QWidget* window( isPreview() ? widget() : widget()->window() );
      helper().renderWindowBackground(painter, rect.adjusted( 4, voffset, -4, -4 ), widget(), window, palette, offset, height );
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
  void NitrogenClient::maximizeChange( void  )
  { 
    if( hasSizeGrip() ) sizeGrip().setVisible( !( isShade() || isMaximized() ) ); 
    KCommonDecorationUnstable::maximizeChange();
  }

  //_________________________________________________________
  void NitrogenClient::shadeChange( void  )
  { 
    if( hasSizeGrip() ) sizeGrip().setVisible( !( isShade() || isMaximized() ) ); 
    KCommonDecorationUnstable::shadeChange();
  }

  //_________________________________________________________
  QPalette NitrogenClient::backgroundPalette( const QWidget* widget, QPalette palette ) const
  {    
    
    if( configuration().drawTitleOutline() && isActive() )
    { palette.setColor( widget->window()->backgroundRole(), options()->color( KDecorationDefines::ColorTitleBar, true ) ); }
    
    return palette;
    
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
    QColor color = palette.window().color();
    
    // draw shadows
    if( compositingActive() && !isMaximized() )
    {
      shadowTiles(
        backgroundPalette( widget(), palette ).color( widget()->backgroundRole() ),
        KDecoration::options()->color(ColorTitleBar),
        SHADOW_WIDTH, isActive() )->render( frame.adjusted( 4, 4, -4, -4),
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
      
        // disable bottom corners when border frame is too small and window is not shaded
        if( configuration().frameBorder() == NitrogenConfiguration::BorderNone && !isShade() ) bottom = 0;
        QRegion mask( x+5*left,   y+0*top, w-5*(left+right), h-0*(top+bottom));
        mask += QRegion(x+0*left, y+5*top, w-0*(left+right), h-5*(top+bottom));
        mask += QRegion(x+2*left, y+2*top, w-2*(left+right), h-2*(top+bottom));
        mask += QRegion(x+3*left, y+1*top, w-3*(left+right), h-1*(top+bottom));
        mask += QRegion(x+1*left, y+3*top, w-1*(left+right), h-3*(top+bottom));      

        if( configuration().frameBorder() == NitrogenConfiguration::BorderNone )
        { mask += QRegion(x+0*left, y+4*top, w-0*(left+right), h-4*(top+bottom)); }

        painter.setClipRegion( mask );
        
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
    if( configuration().frameBorder() >= NitrogenConfiguration::BorderTiny && !isMaximized() )
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
  NitrogenConfiguration NitrogenClient::configuration( void ) const
  { return configuration_; }
  
  //_________________________________________________________________
  void NitrogenClient::createSizeGrip( void )
  {
    
    assert( !hasSizeGrip() );
    if( ( isResizable() && windowId() != 0 ) || isPreview() )
    { 
      size_grip_ = new NitrogenSizeGrip( this ); 
      sizeGrip().setVisible( !( isMaximized() || isShade() ) );
    }
    
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
    
    ShadowTilesOption currentOpt = active ? glowTilesOption_:shadowTilesOption_;
    
    bool optionChanged = !(currentOpt == opt );
    if (active && glowTiles_ )
    { 
    
      if( optionChanged) delete glowTiles_;
      else return glowTiles_;
      
    } else if (!active && shadowTiles_ ) {
      
      if( optionChanged ) delete shadowTiles_;
      else return shadowTiles_;
      
    }
    
    kDebug( 1212 ) << " creating tiles - active: " << active << endl;
    TileSet *tileSet = 0;
    
    if( active && configuration().drawTitleOutline() && configuration().frameBorder() == NitrogenConfiguration::BorderNone )
    {
      
      //---------------------------------------------------------------
      // Create new glow/shadow tiles
      QPixmap shadow = QPixmap( size*2, size*2 );
      shadow.fill( Qt::transparent );
      
      QPainter p( &shadow );
      p.setRenderHint( QPainter::Antialiasing );
     
      QPixmap shadowTop = shadowPixmap( color, glow, size, active );
      QRect topRect( shadow.rect() );
      topRect.setBottom( int( size )-1 );
      p.setClipRect( topRect );
      p.drawPixmap( QPointF( 0, 0 ), shadowTop );
      
      QPixmap shadowBottom = shadowPixmap( widget()->palette().color( widget()->backgroundRole() ), glow, size, active );
      QRect bottomRect( shadow.rect() );
      bottomRect.setTop( int( size ) );
      p.setClipRect( bottomRect );
      p.drawPixmap( QPointF( 0, 0 ), shadowBottom );
      p.end();

      tileSet = new TileSet( shadow, size, size, 1, 1);
      
    } else {
      
      tileSet = new TileSet(
        shadowPixmap( color, glow, size, active ), 
        size, size, 1, 1);
    
    }
    
    // store option and style
    if( active )
    {
      
      glowTilesOption_ = opt;
      glowTiles_ = tileSet;
      
    } else {
      
      shadowTilesOption_ = opt;
      shadowTiles_ = tileSet;
      
    }
    
    return tileSet;
  }
  
  QPixmap NitrogenClient::shadowPixmap(const QColor& color, const QColor& glow, qreal size, bool active) const
  {
    
    //---------------------------------------------------------------
    // Create new glow/shadow tiles
    QPixmap shadow = QPixmap( size*2, size*2 );
    shadow.fill( Qt::transparent );
    
    QPainter p( &shadow );
    p.setRenderHint( QPainter::Antialiasing );
    p.setPen( Qt::NoPen );
    
    if( useOxygenShadows() )
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
        
    }
    
    // draw the corner of the window - actually all 4 corners as one circle
    QLinearGradient lg = QLinearGradient(0.0, size-4.5, 0.0, size+4.5);
    if( configuration().frameBorder() < NitrogenConfiguration::BorderTiny )
    {
      
      // for no 
      lg.setColorAt(0.52, helper().backgroundTopColor(color));
      lg.setColorAt(1.0, helper().backgroundBottomColor(color) );
      
    } else {
      
      QColor light = helper().calcLightColor( helper().backgroundTopColor(color) );
      QColor dark = helper().calcDarkColor(helper().backgroundBottomColor(color));
    
      lg.setColorAt(0.52, light);
      lg.setColorAt(1.0, dark);
    
    }

    p.setBrush( Qt::NoBrush );
    p.setPen(QPen(lg, 0.8));
    p.drawEllipse(QRectF(size-4, size-4, 8, 8));
      
    p.end();
    return shadow;
  }
  
}
