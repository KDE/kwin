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
#include "tileset.h"

#include <cassert>
#include <cmath>

#include <KLocale>
#include <KColorUtils>

#include <QtGui/QApplication>
#include <QtGui/QLabel>
#include <QtGui/QPainter>
#include <QtGui/QBitmap>

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
    glowAnimation_( new Animation( 200, this ) ),
    titleAnimationData_( new TitleAnimationData( this ) ),
    glowIntensity_(0),
    initialized_( false ),
    forceActive_( false ),
    mouseButton_( Qt::NoButton ),
    itemData_( this ),
    sourceItem_( -1 )
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
    widget()->setAcceptDrops( true );

    // setup glow animation
    glowAnimation().data()->setStartValue( glowBias() );
    glowAnimation().data()->setEndValue( 1.0 );
    glowAnimation().data()->setTargetObject( this );
    glowAnimation().data()->setPropertyName( "glowIntensity" );
    glowAnimation().data()->setEasingCurve( QEasingCurve::InOutQuad );

    connect( glowAnimation().data(), SIGNAL( valueChanged( const QVariant& ) ), widget(), SLOT( update( void ) ) );
    connect( glowAnimation().data(), SIGNAL( finished( void ) ), widget(), SLOT( update( void ) ) );
    connect( glowAnimation().data(), SIGNAL( finished() ), this, SLOT( clearForceActive() ) );


    // title animation data
    titleAnimationData_.data()->initialize();
    connect( titleAnimationData_.data(), SIGNAL( pixmapsChanged() ), widget(), SLOT( update( void ) ) );

    // lists
    connect( itemData_.animation().data(), SIGNAL( finished() ), this, SLOT( clearTargetItem() ) );

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

    initialized_ = true;

    // first reset is needed to store Oxygen configuration
    reset(0);

  }

  //___________________________________________
  void OxygenClient::reset( unsigned long changed )
  {
    KCommonDecorationUnstable::reset( changed );

    // update window mask when compositing is changed
    if( !initialized_ ) return;
    if( changed & SettingCompositing )
    {
      updateWindowShape();
      widget()->update();
    }

    configuration_ = factory_->configuration( *this );

    // animations duration
    glowAnimation().data()->setDuration( configuration_.animationsDuration() );
    titleAnimationData_.data()->setDuration( configuration_.animationsDuration() );
    itemData_.animation().data()->setDuration( configuration_.animationsDuration() );
    itemData_.setAnimationsEnabled( useAnimations() );

    // reset title transitions
    titleAnimationData_.data()->reset();

    // should also update animations for buttons
    resetButtons();

    // also reset tab buttons
    for( int index = 0; index < itemData_.count(); index++ )
    {
      ClientGroupItemData& item( itemData_[index] );
      if( item.closeButton_ ) { item.closeButton_.data()->reset(0); }
    }

    // reset tab geometry
    itemData_.setDirty( true );

    // handle size grip
    if( configuration_.drawSizeGrip() )
    {

      if( !hasSizeGrip() ) createSizeGrip();

    } else if( hasSizeGrip() ) deleteSizeGrip();

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

      default: break;

    }

    return NULL;

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
    bool narrowSpacing( configuration().useNarrowButtonSpacing() );
    int frameBorder( configuration().frameBorder() );
    int buttonSize( configuration().hideTitleBar() ? 0 : configuration().buttonSize() );

    switch (lm)
    {
      case LM_BorderLeft:
      case LM_BorderRight:
      case LM_BorderBottom:
      {
        int border( 0 );
        if( respectWindowState && maximized )
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
        if( frameBorder == OxygenConfiguration::BorderNone && configuration().hideTitleBar() )
        {

          border = 0;

        } else if( !( respectWindowState && maximized )) {

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
        { border = 4; }

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
      return narrowSpacing ? 1:3;

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
  QRect OxygenClient::defaultTitleRect( bool active ) const
  {
    QRect titleRect( OxygenClient::titleRect() );
    titleRect.adjust( 0, -layoutMetric( LM_TitleEdgeTop ), 0, 0 );

    if( active && configuration().drawTitleOutline() && isActive() )
    {

      QRect textRect( titleBoundingRect( options()->font( true, false),  titleRect, caption() ) );
      titleRect.setLeft( textRect.left() - layoutMetric( LM_TitleBorderLeft ) );
      titleRect.setRight( textRect.right() + layoutMetric( LM_TitleBorderRight ) );

    } else {

      titleRect.setLeft( widget()->rect().left() + layoutMetric( LM_OuterPaddingLeft ) );
      titleRect.setRight( widget()->rect().right() - layoutMetric( LM_OuterPaddingRight ) );

    }

    return titleRect;

  }

  //_________________________________________________________
  QRect OxygenClient::titleBoundingRect( const QFont& font, QRect rect, const QString& caption ) const
  {

    // get title bounding rect
    QRect boundingRect = QFontMetrics( font ).boundingRect( rect, configuration().titleAlignment() | Qt::AlignVCenter, caption );

    // adjust to make sure bounding rect
    // 1/ has same vertical alignment as original titleRect
    // 2/ does not exceeds available horizontal space
    boundingRect.setTop( rect.top() );
    boundingRect.setBottom( rect.bottom() );

    // check bounding rect against input rect
    if( rect.left() > boundingRect.left() ) { boundingRect.setLeft( rect.left() ); }
    if( rect.right() < boundingRect.right() ) { boundingRect.setRight( rect.right() ); }

    return boundingRect;

  }

  //_________________________________________________________
  void OxygenClient::clearTargetItem( void )
  {

    if( itemData_.animationType() == AnimationLeave )
    { itemData_.setDirty( true ); }

  }

  //_________________________________________________________
  void OxygenClient::updateItemBoundingRects( bool alsoUpdate )
  {

    // make sure items are not animated
    itemData_.animate( AnimationNone );

    // maximum available space
    QRect titleRect( OxygenClient::titleRect() );

    // get tabs
    int items( clientGroupItems().count() );

    // make sure item data have the correct number of items
    while( itemData_.count() < items ) itemData_.push_back( ClientGroupItemData() );
    while( itemData_.count() > items )
    {
      if( itemData_.back().closeButton_ ) delete itemData_.back().closeButton_.data();
      itemData_.pop_back();
    }

    assert( !itemData_.isEmpty() );

    // create buttons
    if( itemData_.count() == 1 )
    {

      // remove button
      if( itemData_.front().closeButton_ )
      { delete itemData_.front().closeButton_.data(); }

      // set active rect
      itemData_.front().activeRect_ = titleRect.adjusted( 0, -layoutMetric( LM_TitleEdgeTop ), 0, 0 );

    } else {

      int left( titleRect.left() );
      int width( titleRect.width()/items );
      for( int index = 0; index < itemData_.count(); index++ )
      {

        ClientGroupItemData& item(itemData_[index]);

        // make sure button exists
        if( !item.closeButton_ )
        {
          item.closeButton_ = ClientGroupItemData::ButtonPointer( new OxygenButton( *this, "Close this tab", ButtonItemClose ) );
          item.closeButton_.data()->show();
          item.closeButton_.data()->installEventFilter( this );
        }

        // set active rect
        QRect local(  QPoint( left, titleRect.top() ), QSize( width, titleRect.height() ) );
        local.adjust( 0, -layoutMetric( LM_TitleEdgeTop ), 0, 0 );
        item.activeRect_ = local;
        left += width;

      }

    }

    if( itemData_.count() == 1 )
    {

      itemData_.front().reset( defaultTitleRect() );

    } else {

      for( int index = 0; index < itemData_.count(); index++ )
      { itemData_[index].reset( itemData_[index].activeRect_ ); }

    }

    // button activity
    itemData_.updateButtonActivity( visibleClientGroupItem() );

    // reset buttons location
    itemData_.updateButtons( alsoUpdate );
    itemData_.setDirty( false );

    return;

  }

  //_________________________________________________________
  QColor OxygenClient::titlebarTextColor(const QPalette &palette)
  {
    if( glowIsAnimated() ) return KColorUtils::mix(
      titlebarTextColor( palette, false ),
      titlebarTextColor( palette, true ),
      glowIntensity() );
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
      int height = 64 + configuration().buttonSize() - OxygenConfiguration::ButtonDefault;
      const QWidget* window( isPreview() ? OxygenClient::widget() : widget->window() );
      helper().renderWindowBackground(painter, rect, widget, window, palette, offset, height );

    }

  }

  //_________________________________________________________
  void OxygenClient::renderWindowBorder( QPainter* painter, const QRect& clipRect, const QWidget* widget, const QPalette& palette ) const
  {

    // check if outline is needed
    if( clientGroupItems().count() < 2 && !itemData_.isAnimated() && !( isActive() && configuration().drawTitleOutline() ) ) return;

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

    // make titlebar background darker for tabbed, non-outline window
    if( ( clientGroupItems().count() >= 2 || itemData_.isAnimated() ) && !(configuration().drawTitleOutline() && isActive() ) )
    {

      QPoint topLeft( r.topLeft()-position );
      QRect rect( topLeft, QSize( r.width(), titleHeight ) );

      QLinearGradient lg( rect.topLeft(), rect.bottomLeft() );
      lg.setColorAt( 0, helper().alphaColor( Qt::black, 0.05 ) );
      lg.setColorAt( 1, helper().alphaColor( Qt::black, 0.10 ) );
      painter->setBrush( lg );
      painter->setPen( Qt::NoPen );
      painter->drawRect( rect );

    }

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

    if( configuration().drawTitleOutline() && isActive() )
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
    if( glowIsAnimated() ) local = helper().alphaColor( color, glowIntensity() );
    helper().drawSeparator( painter, QRect(r.top(), titleTop+titleHeight-1.5, r.width(), 2).translated( -position ), local, Qt::Horizontal);

    if (clipRect.isValid()) { painter->restore(); }

  }

  //_________________________________________________________
  void OxygenClient::renderTitleOutline(  QPainter* painter, const QRect& rect, const QPalette& palette ) const
  {

    // center (for active windows only)
    {
      painter->save();
      int offset = 1;
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
    int offset = -3;
    int voffset = 5-shadowSize;
    QRect adjustedRect( rect.adjusted(offset, voffset, -offset, shadowSize) );
    helper().slab( palette.color( widget()->backgroundRole() ), 0, shadowSize )->render( adjustedRect, painter, TileSet::Top|TileSet::Left|TileSet::Right );

  }

  //_________________________________________________________
  void OxygenClient::renderTitleText( QPainter* painter, const QRect& rect, const QColor& color, const QColor& contrast ) const
  {

    if( !titleAnimationData_.data()->isValid() )
    {
        // contrast pixmap
        titleAnimationData_.data()->reset(
            rect,
            renderTitleText( rect, caption(), color ),
            renderTitleText( rect, caption(), contrast ) );
    }

    if( titleAnimationData_.data()->isDirty() )
    {

        // clear dirty flags
        titleAnimationData_.data()->setDirty( false );

        // finish current animation if running
        if( titleAnimationData_.data()->isAnimated() )
        { titleAnimationData_.data()->finishAnimation(); }

        if( !titleAnimationData_.data()->isLocked() )
        {

            // set pixmaps
            titleAnimationData_.data()->setPixmaps(
                rect,
                renderTitleText( rect, caption(), color ),
                renderTitleText( rect, caption(), contrast ) );

            titleAnimationData_.data()->startAnimation();
            renderTitleText( painter, rect, color, contrast );

        } else if( !caption().isEmpty() ) {

            renderTitleText( painter, rect, caption(), color, contrast );

        }

        // lock animations (this must be done whether or not
        // animation was actually started, in order to extend locking
        // every time title get changed too rapidly
        titleAnimationData_.data()->lockAnimations();

    } else if( titleAnimationData_.data()->isAnimated() ) {

        if( isMaximized() ) painter->translate( 0, 2 );
        if( !titleAnimationData_.data()->contrastPixmap().isNull() )
        {
            painter->translate( 0, 1 );
            painter->drawPixmap( rect.topLeft(), titleAnimationData_.data()->contrastPixmap() );
            painter->translate( 0, -1 );
        }

        painter->drawPixmap( rect.topLeft(), titleAnimationData_.data()->pixmap() );

        if( isMaximized() ) painter->translate( 0, -2 );

    } else if( !caption().isEmpty() ) {

      renderTitleText( painter, rect, caption(), color, contrast );

    }

  }

  //_______________________________________________________________________
  void OxygenClient::renderTitleText( QPainter* painter, const QRect& rect, const QString& caption, const QColor& color, const QColor& contrast, bool elide ) const
  {

    Qt::Alignment alignment( configuration().titleAlignment() | Qt::AlignVCenter );
    QString local( elide ? QFontMetrics( painter->font() ).elidedText( caption, Qt::ElideRight, rect.width() ):caption );

    // translate title down in case of maximized window
    if( isMaximized() ) painter->translate( 0, 2 );

    if( contrast.isValid() )
    {
      painter->setPen( contrast );
      painter->translate( 0, 1 );
      painter->drawText( rect, alignment, local );
      painter->translate( 0, -1 );
    }

    painter->setPen( color );
    painter->drawText( rect, alignment, local );

    // translate back
    if( isMaximized() ) painter->translate( 0, -2 );

  }

  //_______________________________________________________________________
  QPixmap OxygenClient::renderTitleText( const QRect& rect, const QString& caption, const QColor& color, bool elide ) const
  {

    QPixmap out( rect.size() );
    out.fill( Qt::transparent );
    if( caption.isEmpty() || !color.isValid() ) return out;

    QPainter painter( &out );
    painter.setFont( options()->font(isActive(), false) );
    Qt::Alignment alignment( configuration().titleAlignment() | Qt::AlignVCenter );
    QString local( elide ? QFontMetrics( painter.font() ).elidedText( caption, Qt::ElideRight, rect.width() ):caption );

    painter.setPen( color );
    painter.drawText( out.rect(), alignment, local );
    painter.end();
    return out;

  }

  //_______________________________________________________________________
  void OxygenClient::renderItem( QPainter* painter, int index, const QPalette& palette )
  {

    const ClientGroupItemData& item( itemData_[index] );

    // see if tag is active
    int itemCount( itemData_.count() );

    //
    if( !item.boundingRect_.isValid() ) return;

    // create rect in which text is to be drawn
    QRect textRect( item.boundingRect_.adjusted( 0, layoutMetric( LM_TitleEdgeTop )-1, 0, -1 ) );

    // add extra space needed for title outline
    if( itemCount > 1 || itemData_.isAnimated() )
    { textRect.adjust( layoutMetric( LM_TitleBorderLeft ), 0, -layoutMetric(LM_TitleBorderRight), 0 ); }

    // add extra space for the button
    if( itemCount > 1 && item.closeButton_ && item.closeButton_.data()->isVisible() )
    { textRect.adjust( 0, 0, - configuration().buttonSize() - layoutMetric(LM_TitleEdgeRight), 0 ); }

    // check if current item is active
    bool active( index == visibleClientGroupItem() );

    // get current item caption and update text rect
    const QList< ClientGroupItem >& items( clientGroupItems() );
    QString caption( itemCount == 1 ? OxygenClient::caption() : items[index].title() );

    // always make sure that titleRect never conflicts with window buttons
    QRect titleRect( OxygenClient::titleRect() );
    if( titleRect.left() > textRect.left() ) { textRect.setLeft( titleRect.left() ); }
    if( titleRect.right() < textRect.right() ) { textRect.setRight( titleRect.right() ); }

    // title outline
    if( itemCount == 1 )
    {

      if( itemData_.isAnimated() ) {

        textRect = titleBoundingRect( painter->font(), textRect, caption );
        renderTitleOutline( painter, item.boundingRect_, palette );

      } else if( (isActive()||glowIsAnimated()) && configuration().drawTitleOutline() ) {

        // adjust textRect
        textRect = titleBoundingRect( painter->font(), textRect, caption );

        // adjusts boundingRect accordingly
        QRect boundingRect( item.boundingRect_ );
        boundingRect.setLeft( textRect.left() - layoutMetric( LM_TitleBorderLeft ) );
        boundingRect.setRight( textRect.right() + layoutMetric( LM_TitleBorderRight ) );

        // render bounding rect around it with extra margins
        renderTitleOutline( painter, boundingRect, palette );

      }

    } else if( active ) {

      // in multiple tabs render title outline in all cases
      renderTitleOutline( painter, item.boundingRect_, palette );

    }

    // render text
    if( active || itemCount == 1 )
    {

      // for active tab, current caption is "merged" with old caption, if any
      renderTitleText(
        painter, textRect,
        titlebarTextColor( palette ),
        titlebarContrastColor( palette ) );


    } else {

      QColor background( backgroundPalette( widget(), palette ).color( widget()->window()->backgroundRole() ) );

      // add extra shade (as used in renderWindowBorder
      if( !( isActive() && configuration().drawTitleOutline() ) )
      { background = KColorUtils::mix( background, Qt::black, 0.10 ); }

      // otherwise current caption is rendered directly
      renderTitleText(
        painter, textRect, caption,
        titlebarTextColor( backgroundPalette( widget(), palette ), false ),
        titlebarContrastColor( background ) );

    }

    // render separators between inactive tabs
    if( !( active || itemCount == 1 ) && item.closeButton_ && item.closeButton_.data()->isVisible() )
    {

      // separators
      // draw Left separator
      QColor color( backgroundPalette( widget(), palette ).window().color() );
      bool isFirstItem(  index == 0 || (index == 1 && !itemData_[0].boundingRect_.isValid() ) );
      if( !active && ( ( isFirstItem && buttonsLeftWidth() > 0 ) || itemData_.isTarget( index ) ) )
      {

        QRect local( item.boundingRect_.topLeft()+QPoint(0,2), QSize( 2, item.boundingRect_.height()-3 ) );
        helper().drawSeparator( painter, local, color, Qt::Vertical);

      }

      // draw right separator
      if(
        ( index == itemCount-1 && buttonsRightWidth() > 0 ) ||
        ( index+1 < itemCount && ( itemData_.isTarget( index+1 ) ||
        !( index+1 == visibleClientGroupItem() && itemData_[index+1].boundingRect_.isValid() ) ) ) )
      {

        QRect local( item.boundingRect_.topRight()+QPoint(0,2), QSize( 2, item.boundingRect_.height()-3 ) );
        helper().drawSeparator( painter, local, color, Qt::Vertical);

      }

    }

  }

  //_______________________________________________________________________
  void OxygenClient::renderTargetRect( QPainter* p, const QPalette& palette )
  {
    if( itemData_.targetRect().isNull() || itemData_.isAnimationRunning() ) return;

    p->save();
    QColor color = palette.color(QPalette::Highlight);
    p->setPen(KColorUtils::mix(color, palette.color(QPalette::Active, QPalette::WindowText)));
    p->setBrush( helper().alphaColor( color, 0.5 ) );
    p->drawRect( itemData_.targetRect().adjusted( 4, 2, -4, -2 ) );
    p->restore();

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
    itemData_.setDirty( true );

    // reset animation
    if( animateActiveChange() )
    {
      glowAnimation().data()->setDirection( isActive() ? Animation::Forward : Animation::Backward );
      if(!glowIsAnimated()) { glowAnimation().data()->start(); }
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
    itemData_.setDirty( true );
    if( animateTitleChange() )
    { titleAnimationData_.data()->setDirty( true ); }

  }

  //_________________________________________________________
  QPalette OxygenClient::backgroundPalette( const QWidget* widget, QPalette palette ) const
  {

    if( configuration().drawTitleOutline() )
    {
      if( glowIsAnimated() && !isForcedActive() )
      {

        QColor inactiveColor( backgroundColor( widget, palette, false ) );
        QColor activeColor( backgroundColor( widget, palette, true ) );
        QColor mixed( KColorUtils::mix( inactiveColor, activeColor, glowIntensity() ) );
        palette.setColor( widget->window()->backgroundRole(), mixed );
        palette.setColor( QPalette::Button, mixed );

      } else if( isActive() || isForcedActive() ) {

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

  //_________________________________________________________
  QString OxygenClient::defaultButtonsLeft() const
  { return KCommonDecoration::defaultButtonsLeft(); }

  //_________________________________________________________
  QString OxygenClient::defaultButtonsRight() const
  { return "HIAX"; }

  //________________________________________________________________
  void OxygenClient::updateWindowShape()
  {

    if(isMaximized() || compositingActive() ) clearMask();
    else setMask( calcMask() );

  }

  //______________________________________________________________________________
  bool OxygenClient::eventFilter( QObject* object, QEvent* event )
  {

    // all dedicated event filtering is here to handle multiple tabs.
    // if tabs are disabled, do nothing
    if( !configuration().tabsEnabled() )
    { return KCommonDecorationUnstable::eventFilter( object, event ); }

    bool state = false;
    switch( event->type() )
    {

      case QEvent::Show:
      if( widget() == object )
      { itemData_.setDirty( true ); }
      break;

      case QEvent::MouseButtonPress:
      if( widget() == object )
      { state = mousePressEvent( static_cast< QMouseEvent* >( event ) ); }
      break;

      case QEvent::MouseButtonRelease:
      if( widget() == object ) state = mouseReleaseEvent( static_cast< QMouseEvent* >( event ) );
      else if( OxygenButton *btn = qobject_cast< OxygenButton* >( object ) )
      {
        QMouseEvent* mouseEvent( static_cast< QMouseEvent* >( event ) );
        if( mouseEvent->button() == Qt::LeftButton && btn->rect().contains( mouseEvent->pos() ) )
        { state = closeItem( btn ); }
      }

      break;

      case QEvent::MouseMove:
      state = mouseMoveEvent( static_cast< QMouseEvent* >( event ) );
      break;

      case QEvent::DragEnter:
      if(  widget() == object )
      { state = dragEnterEvent( static_cast< QDragEnterEvent* >( event ) ); }
      break;

      case QEvent::DragMove:
      if( widget() == object )
      { state = dragMoveEvent( static_cast< QDragMoveEvent* >( event ) ); }
      break;

      case QEvent::DragLeave:
      if( widget() == object )
      { state = dragLeaveEvent( static_cast< QDragLeaveEvent* >( event ) ); }
      break;

      case QEvent::Drop:
      if( widget() == object )
      { state = dropEvent( static_cast< QDropEvent* >( event ) ); }
      break;

      default: break;

    }
    return state || KCommonDecorationUnstable::eventFilter( object, event );

  }

  //_________________________________________________________
  void OxygenClient::resizeEvent( QResizeEvent* event )
  {
    itemData_.setDirty( true );
    KCommonDecorationUnstable::resizeEvent( event );
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
      if( configuration().useOxygenShadows() && glowIsAnimated() && !isForcedActive() )
      {

        int frame = maxAnimationIndex*glowIntensity();
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

    // make sure ItemData and clientGroupItems are synchronized
    /*
    this needs to be done before calling RenderWindowBorder
    since some painting in there depend on the clientGroups state
    */
    if(  itemData_.isDirty() || itemData_.count() != clientGroupItems().count() )
    { updateItemBoundingRects( false ); }

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

      // resize handles
      renderDots( &painter, frame, QColor(0, 0, 0, 66) );
      painter.restore();
    }

    if( !configuration().hideTitleBar() )
    {

        // title bounding rect
        painter.setFont( options()->font(isActive(), false) );

        // draw ClientGroupItems
        int itemCount( itemData_.count() );
        for( int i = 0; i < itemCount; i++ ) renderItem( &painter, i, palette );

        // draw target rect
        renderTargetRect( &painter, widget()->palette() );

        // separator
        if( itemCount == 1 && !itemData_.isAnimated() && drawSeparator() )
        { renderSeparator(&painter, frame, widget(), color ); }

    }

  }

  //_____________________________________________________________
  bool OxygenClient::mousePressEvent( QMouseEvent* event )
  {

    QPoint point = event->pos();
    if( itemClicked( point ) < 0 ) return false;
    dragPoint_ = point;

    mouseButton_ = event->button();
    bool accepted( false );
    if( buttonToWindowOperation( mouseButton_ ) == ClientGroupDragOp )
    {

      accepted = true;

    } else if( buttonToWindowOperation( mouseButton_ ) == OperationsOp ) {

      QPoint point = event->pos();
      int itemClicked( OxygenClient::itemClicked( point ) );
      displayClientMenu( itemClicked, widget()->mapToGlobal( event->pos() ) );
      mouseButton_ = Qt::NoButton;
      accepted = true; // displayClientMenu can possibly destroy the deco...

    }
    return accepted;
  }

  //_____________________________________________________________
  bool OxygenClient::mouseReleaseEvent( QMouseEvent* event )
  {

    bool accepted( false );
    if( mouseButton_ == event->button() &&
        buttonToWindowOperation( mouseButton_ ) != OperationsOp )
    {

      QPoint point = event->pos();

      int visibleItem = visibleClientGroupItem();
      int itemClicked( OxygenClient::itemClicked( point ) );
      if( itemClicked >= 0 && visibleItem != itemClicked )
      {
        setVisibleClientGroupItem( itemClicked );
        setForceActive( true );
        accepted = true;
      }

    }

    mouseButton_ = Qt::NoButton;
    return accepted;

  }

  //_____________________________________________________________
  bool OxygenClient::mouseMoveEvent( QMouseEvent* event )
  {

    // check button and distance to drag point
    if( configuration().hideTitleBar() || mouseButton_ == Qt::NoButton  || ( event->pos() - dragPoint_ ).manhattanLength() <= QApplication::startDragDistance() )
    { return false; }

    bool accepted( false );
    if( buttonToWindowOperation( mouseButton_ ) == ClientGroupDragOp )
    {

      QPoint point = event->pos();
      int itemClicked( OxygenClient::itemClicked( point ) );
      if( itemClicked < 0 ) return false;

      titleAnimationData_.data()->reset();

      QDrag *drag = new QDrag( widget() );
      QMimeData *groupData = new QMimeData();
      groupData->setData( clientGroupItemDragMimeType(), QString().setNum( itemId( itemClicked )).toAscii() );
      drag->setMimeData( groupData );
      sourceItem_ = OxygenClient::itemClicked( dragPoint_ );

      // get tab geometry
      QRect geometry;
      geometry = itemData_[itemClicked].boundingRect_;

      // remove space used for buttons
      if( itemData_.count() > 1  )
      {

        geometry.adjust( 0, 0,  - configuration().buttonSize() - layoutMetric(LM_TitleEdgeRight), 0 );

      } else if( !( isActive() && configuration().drawTitleOutline() ) ) {


        geometry.adjust(
          buttonsLeftWidth() + layoutMetric( LM_TitleEdgeLeft ) , 0,
          -( buttonsRightWidth() + layoutMetric( LM_TitleEdgeRight )), 0 );

      }

      drag->setPixmap( itemDragPixmap( itemClicked, geometry ) );

      // note: the pixmap is moved just above the pointer on purpose
      // because overlapping pixmap and pointer slows down the pixmap a lot.
      QPoint hotSpot( QPoint( event->pos().x() - geometry.left(), -1 ) );

      // make sure the horizontal hotspot position is not too far away (more than 1px)
      // from the pixmap
      if( hotSpot.x() < -1 ) hotSpot.setX(-1);
      if( hotSpot.x() > geometry.width() ) hotSpot.setX( geometry.width() );

      drag->setHotSpot( hotSpot );

      dragStartTimer_.start( 50, this );
      drag->exec( Qt::MoveAction );

      // detach tab from window
      if( drag->target() == 0 && itemData_.count() > 1 )
      {
        itemData_.setDirty( true );
        removeFromClientGroup( sourceItem_,
            widget()->frameGeometry().adjusted(
                layoutMetric( LM_OuterPaddingLeft ),
                layoutMetric( LM_OuterPaddingTop ),
                -layoutMetric( LM_OuterPaddingRight ),
                -layoutMetric( LM_OuterPaddingBottom )
            ).translated( QCursor::pos() - event->pos() +
                QPoint( layoutMetric( LM_OuterPaddingLeft ), layoutMetric( LM_OuterPaddingTop )))
        );
      }

      // reset button
      mouseButton_ = Qt::NoButton;
      accepted = true;

    }

    return accepted;

  }

  //_____________________________________________________________
  bool OxygenClient::dragEnterEvent( QDragEnterEvent* event )
  {

    // check if drag enter is allowed
    if( !event->mimeData()->hasFormat( clientGroupItemDragMimeType() ) || configuration().hideTitleBar() ) return false;

    //
    event->acceptProposedAction();
    if( event->source() != widget() )
    {

      QPoint position( event->pos() );
      itemData_.animate( AnimationEnter, itemClicked( position, true ) );

    } else if( itemData_.count() > 1 )  {

      QPoint position( event->pos() );
      int itemClicked( OxygenClient::itemClicked( position, false ) );
      itemData_.animate( AnimationEnter|AnimationSameTarget, itemClicked );

    }

    return true;

  }

  //_____________________________________________________________
  bool OxygenClient::dragLeaveEvent( QDragLeaveEvent* )
  {

    if( itemData_.animationType() & AnimationSameTarget )
    {

      if( dragStartTimer_.isActive() ) dragStartTimer_.stop();
      itemData_.animate( AnimationLeave|AnimationSameTarget, sourceItem_ );

    } else if( itemData_.isAnimated() ) {


      itemData_.animate( AnimationLeave );

    }


    return true;

  }

  //_____________________________________________________________
  bool OxygenClient::dragMoveEvent( QDragMoveEvent* event )
  {

    // check format
    if( !event->mimeData()->hasFormat( clientGroupItemDragMimeType() ) ) return false;
    if( event->source() != widget() )
    {

      QPoint position( event->pos() );
      itemData_.animate( AnimationMove, itemClicked( position, true ) );

    } else if( itemData_.count() > 1 )  {

      if( dragStartTimer_.isActive() ) dragStartTimer_.stop();

      QPoint position( event->pos() );
      int itemClicked( OxygenClient::itemClicked( position, false ) );
      itemData_.animate( AnimationMove|AnimationSameTarget, itemClicked );

    }

    return false;

  }

  //_____________________________________________________________
  bool OxygenClient::dropEvent( QDropEvent* event )
  {

    QPoint point = event->pos();
    itemData_.animate( AnimationNone );

    const QMimeData *groupData = event->mimeData();
    if( !groupData->hasFormat( clientGroupItemDragMimeType() ) ) return false;

    if( widget() == event->source() )
    {

      int from = OxygenClient::itemClicked( dragPoint_ );
      int itemClicked( OxygenClient::itemClicked( point, false ) );

      if( itemClicked > from )
      {
        itemClicked++;
        if( itemClicked >= clientGroupItems().count() )
        { itemClicked = -1; }
      }

      itemData_.setDirty( true );
      moveItemInClientGroup( from, itemClicked );
      widget()->update();

    } else {

      setForceActive( true );
      int itemClicked( OxygenClient::itemClicked( point, true ) );
      long source = QString( groupData->data( clientGroupItemDragMimeType() ) ).toLong();
      itemData_.setDirty( true );
      moveItemToClientGroup( source, itemClicked );

    }

    titleAnimationData_.data()->reset();
    return true;

  }

  //_____________________________________________________________
  void OxygenClient::timerEvent( QTimerEvent* event )
  {

    if( event->timerId() != dragStartTimer_.timerId() )
    { return KCommonDecorationUnstable::timerEvent( event ); }

    dragStartTimer_.stop();

    // do nothing if there is only one tab
    if( itemData_.count() > 1 )
    {
      itemData_.animate( AnimationMove|AnimationSameTarget, sourceItem_ );
      itemData_.animate( AnimationLeave|AnimationSameTarget, sourceItem_ );
    }

  }

  //_____________________________________________________________
  bool OxygenClient::closeItem( const OxygenButton* button )
  {

    for( int i=0; i <  itemData_.count(); i++ )
    {
      if( button == itemData_[i].closeButton_.data() )
      {
        itemData_.setDirty( true );
        closeClientGroupItem( i );
        return true;
      }
    }
    return false;

  }

  //________________________________________________________________
  QPixmap OxygenClient::itemDragPixmap( int index, const QRect& geometry )
  {
    bool itemValid( index >= 0 && index < clientGroupItems().count() );

    QPixmap pixmap( geometry.size() );
    QPainter painter( &pixmap );
    painter.setRenderHints(QPainter::SmoothPixmapTransform|QPainter::Antialiasing);

    painter.translate( -geometry.topLeft() );

    // render window background
    renderWindowBackground( &painter, geometry, widget(), widget()->palette() );

    // darken background if item is inactive
    bool itemActive = !( itemValid && index != visibleClientGroupItem() );
    if( !itemActive )
    {

      QLinearGradient lg( geometry.topLeft(), geometry.bottomLeft() );
      lg.setColorAt( 0, helper().alphaColor( Qt::black, 0.05 ) );
      lg.setColorAt( 1, helper().alphaColor( Qt::black, 0.10 ) );
      painter.setBrush( lg );
      painter.setPen( Qt::NoPen );
      painter.drawRect( geometry );

    }

    // render title text
    painter.setFont( options()->font(isActive(), false) );
    QRect textRect( geometry.adjusted( 0, layoutMetric( LM_TitleEdgeTop )-1, 0, -1 ) );

    if( itemValid )
    { textRect.adjust( layoutMetric( LM_TitleBorderLeft ), 0, -layoutMetric(LM_TitleBorderRight), 0 ); }

    QString caption( itemValid ? clientGroupItems()[index].title() : OxygenClient::caption() );
    renderTitleText( &painter, textRect, caption, titlebarTextColor( widget()->palette(), isActive() && itemActive ) );

    // floating frame
    helper().drawFloatFrame(
      &painter, geometry, widget()->palette().window().color(),
      true, false,
      KDecoration::options()->color(ColorTitleBar)
      );

    painter.end();

    // create pixmap mask
    QBitmap bitmap( geometry.size() );
    {
      bitmap.clear();
      QPainter painter( &bitmap );
      QPainterPath path;
      path.addRegion( helper().roundedMask( geometry.translated( -geometry.topLeft() ) ) );
      painter.fillPath( path, Qt::color1 );
    }

    pixmap.setMask( bitmap );
    return pixmap;

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
