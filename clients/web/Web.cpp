/*
  'Web' kwin client

  Copyright (C) 2001 Rik Hemsley (rikkus) <rik@kde.org>

  This program is free software; you can redistribute it and/or
  modify it under the terms of the GNU General Public
  License as published by the Free Software Foundation; either
  version 2 of the License, or (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; see the file COPYING.  If not, write to
  the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
  Boston, MA 02111-1307, USA.
*/

#include <qlabel.h>
#include <qlayout.h>
#include <qpainter.h>

#include <kconfig.h>

#include "Web.h"
#include "WebButton.h"
#include "WebButtonHelp.h"
#include "WebButtonIconify.h" // minimize button
#include "WebButtonClose.h"
#include "WebButtonSticky.h" // onAllDesktops button
#include "WebButtonMaximize.h"
#include "WebButtonLower.h"

extern "C"
{
  KDecorationFactory *create_factory()
  {
    return new Web::WebFactory();
  }
}

namespace Web {

WebClient::WebClient(KDecorationBridge* bridge, KDecorationFactory* factory)
  : KDecoration(bridge, factory),
    mainLayout_   (0),
    titleSpacer_  (0)
{
  // Empty.
}

WebClient::~WebClient()
{
  // Empty.
}

  void
WebClient::init()
{
  createMainWidget(WNoAutoErase);
  widget()->installEventFilter( this );
  widget()->setBackgroundMode(NoBackground);

  // title height
  const uint textVMargin   = 2;
  QFontMetrics fm(options()->font(isActive(), isTool()));

  // border size
  switch(options()->preferredBorderSize( factory())) {
    case BorderLarge:
      borderSize_ = 8;
      break;
    case BorderVeryLarge:
      borderSize_ = 12;
      break;
    case BorderHuge:
      borderSize_ = 18;
      break;
    case BorderVeryHuge:
      borderSize_ = 27;
      break;
    case BorderOversized:
      borderSize_ = 40;
      break;
    case BorderNormal:
    default:
      borderSize_ = 4;
  }
  titleHeight_ = QMAX(QMAX(14, fm.height() + textVMargin * 2), borderSize_);

  _resetLayout();

  leftButtonList_   .setAutoDelete(true);
  rightButtonList_  .setAutoDelete(true);
}

  void
WebClient::reset( unsigned long changed )
{
  if (changed & SettingColors)
  {
    // repaint the whole thing
    widget()->repaint(false);
  } else if (changed & SettingFont) {
    // font has changed -- update title height
    // title height
    const uint textVMargin   = 2;
    QFontMetrics fm(options()->font(isActive(), isTool()));
    titleHeight_ = QMAX(14, fm.height() + textVMargin * 2);
    // update buttons
    for (QPtrListIterator<WebButton> it(leftButtonList_); it.current(); ++it)
    {
      it.current()->setFixedSize(titleHeight_, titleHeight_);
    }
    for (QPtrListIterator<WebButton> it(rightButtonList_); it.current(); ++it)
    {
      it.current()->setFixedSize(titleHeight_, titleHeight_);
    }
//     for (int n=0; n<NumButtons; n++) {
//         if (m_button[n]) m_button[n]->setSize(s_titleHeight-1);
//     }
    // update the spacer
    titleSpacer_->changeSize(0, titleHeight_, QSizePolicy::Expanding,
                             QSizePolicy::Fixed);
    widget()->repaint(false);
  }
}

  void
WebClient::resizeEvent(QResizeEvent *)
{
  doShape();
  widget()->repaint();
}

  void
WebClient::captionChange()
{
  widget()->repaint();
}

void WebClient::iconChange()
{
// Empty.
}

  void
WebClient::paintEvent(QPaintEvent * pe)
{
  QRect titleRect(titleSpacer_->geometry());
  titleRect.setTop(1);

  QPainter p(widget());

  p.setPen(Qt::black);
  p.setBrush(options()->colorGroup(ColorFrame, isActive()).background());

  p.setClipRegion(pe->region() - titleRect);

  p.drawRect(widget()->rect());

  p.setClipRegion(pe->region());

  p.fillRect(titleRect, options()->color(ColorTitleBar, isActive()));

  if (shape_)
  {
    int r(width());
    int b(height());

    // Draw edge of top-left corner inside the area removed by the mask.

    p.drawPoint(3, 1);
    p.drawPoint(4, 1);
    p.drawPoint(2, 2);
    p.drawPoint(1, 3);
    p.drawPoint(1, 4);

    // Draw edge of top-right corner inside the area removed by the mask.

    p.drawPoint(r - 5, 1);
    p.drawPoint(r - 4, 1);
    p.drawPoint(r - 3, 2);
    p.drawPoint(r - 2, 3);
    p.drawPoint(r - 2, 4);

    // Draw edge of bottom-left corner inside the area removed by the mask.

    p.drawPoint(1, b - 5);
    p.drawPoint(1, b - 4);
    p.drawPoint(2, b - 3);
    p.drawPoint(3, b - 2);
    p.drawPoint(4, b - 2);

    // Draw edge of bottom-right corner inside the area removed by the mask.

    p.drawPoint(r - 2, b - 5);
    p.drawPoint(r - 2, b - 4);
    p.drawPoint(r - 3, b - 3);
    p.drawPoint(r - 4, b - 2);
    p.drawPoint(r - 5, b - 2);
  }

  p.setFont(options()->font(isActive(), isTool()));

  p.setPen(options()->color(ColorFont, isActive()));

  p.drawText(titleSpacer_->geometry(), AlignCenter, caption());
}

  void
WebClient::doShape()
{
  if (!shape_)
    return;

  QRegion mask(0, 0, width(), height());

  int r(width());
  int b(height());

  // Remove top-left corner.

  mask -= QRegion(0, 0, 5, 1);
  mask -= QRegion(0, 1, 3, 1);
  mask -= QRegion(0, 2, 2, 1);
  mask -= QRegion(0, 3, 1, 2);

  // Remove top-right corner.

  mask -= QRegion(r - 5, 0, 5, 1);
  mask -= QRegion(r - 3, 1, 3, 1);
  mask -= QRegion(r - 2, 2, 2, 1);
  mask -= QRegion(r - 1, 3, 1, 2);

  // Remove bottom-left corner.

  mask -= QRegion(0, b - 5, 1, 3);
  mask -= QRegion(0, b - 3, 2, 1);
  mask -= QRegion(0, b - 2, 3, 1);
  mask -= QRegion(0, b - 1, 5, 1);

  // Remove bottom-right corner.

  mask -= QRegion(r - 5, b - 1, 5, 1);
  mask -= QRegion(r - 3, b - 2, 3, 1);
  mask -= QRegion(r - 2, b - 3, 2, 1);
  mask -= QRegion(r - 1, b - 5, 1, 2);

  setMask(mask);
}

  void
WebClient::showEvent(QShowEvent *)
{
  doShape();
  widget()->repaint();
}

  void
WebClient::windowWrapperShowEvent(QShowEvent *)
{
  doShape();
  widget()->repaint();
}

  void
WebClient::mouseDoubleClickEvent(QMouseEvent * e)
{
  if (titleSpacer_->geometry().contains(e->pos()))
  {
    titlebarDblClickOperation();
  }
}

  void
WebClient::desktopChange()
{
  emit(oadChange(isOnAllDesktops()));
}

  void
WebClient::maximizeChange()
{
  emit(maxChange(maximizeMode()==MaximizeFull));
}

  void
WebClient::activeChange()
{
  widget()->repaint();
}

  WebClient::MousePosition
WebClient::mousePosition(const QPoint & p) const
{
  int x = p.x();
  int y = p.y();
  int corner = 14 + 3*borderSize_/2;

  if (y < titleSpacer_->geometry().height())
  {
    // rikkus: this style is not designed to be resizable at the top edge.

#if 0
    if ((y < 4 && x < corner) || x < 4)
      return Client::TopLeft;
    else if ((y < 4 && x > width() - corner) || x > width() - 4)
      return Client::TopRight;
    else if (y < 4)
      return Client::Top;
    else
#endif
      return KDecoration::Center;
  }
  else if (y < height() - borderSize_)
  {
    if (x < borderSize_)
      return KDecoration::Left;
    else
      if (x > width() - borderSize_)
        return KDecoration::Right;
      else
        return KDecoration::Center;
  }
  else
  {
    if (x < 12 + corner)
      return KDecoration::BottomLeft2;
    else
      if (x > width() - corner)
        return KDecoration::BottomRight2;
      else
        return KDecoration::Bottom;
  }

  return KDecoration::mousePosition(p);
}

  void
WebClient::slotMaximize(int button)
{
  switch (button)
  {
    case MidButton:
      maximize(maximizeMode() ^ MaximizeVertical);
      break;

    case RightButton:
      maximize(maximizeMode() ^ MaximizeHorizontal);
      break;

    case LeftButton:
    default:
      maximize(maximizeMode() == MaximizeFull ? MaximizeRestore : MaximizeFull);
  }
}

  WebButton *
WebClient::_createButton(const QString & s, QWidget * parent)
{
  WebButton * b = 0;

  if (("Help" == s) && providesContextHelp())
  {
    b = new WebButtonHelp(parent);
    connect(b, SIGNAL(help()), this, SLOT(showContextHelp()));
  }

  else if ("OnAllDesktops" == s)
  {
    b = new WebButtonSticky(isOnAllDesktops(), parent);
    connect(b, SIGNAL(toggleSticky()), this, SLOT(toggleOnAllDesktops()));
    connect(this, SIGNAL(oadChange(bool)), b, SLOT(slotOnAllDesktopsChange(bool)));
  }

  else if ("Minimize" == s && isMinimizable())
  {
    b = new WebButtonIconify(parent);
    connect(b, SIGNAL(minimize()), this, SLOT(minimize()));
  }

  else if ("Maximize" == s && isMaximizable())
  {
    b = new WebButtonMaximize((maximizeMode()==MaximizeFull), parent);
    connect(b, SIGNAL(maximize(int)), this, SLOT(slotMaximize(int)));
    connect(this, SIGNAL(maxChange(bool)), b, SLOT(slotMaximizeChange(bool)));
  }

  else if ("Close" == s && isCloseable())
  {
    b = new WebButtonClose(parent);
    connect(b, SIGNAL(closeWindow()), this, SLOT(closeWindow()));
  }

  else if ("Lower" == s)
  {
    b = new WebButtonLower(parent);
    connect(b, SIGNAL(lowerWindow()), this, SLOT(slotLowerWindow()));
  }

  if (0 != b)
  {
    b->setShape(shape_);
  }

  return b;
}

  void
WebClient::_createButtons()
{
  leftButtonList_   .clear();
  rightButtonList_  .clear();

  QString buttons = options()->titleButtonsLeft() + "|" + options()->titleButtonsRight();
  QPtrList<WebButton> *buttonList = &leftButtonList_;
  for (unsigned int i = 0; i < buttons.length(); ++i)
  {
    WebButton * tb = 0;
    switch (buttons[i].latin1())
    {
      case 'S': // OnAllDesktops
        tb = _createButton("OnAllDesktops", widget());
        break;

      case 'H': // Help
        tb = _createButton("Help", widget());
        break;

      case 'I': // Minimize
        tb = _createButton("Minimize", widget());
        break;

      case 'A': // Maximize
        tb = _createButton("Maximize", widget());
        break;

      case 'X': // Close
        tb = _createButton("Close", widget());
        break;

      case '|':
        buttonList = &rightButtonList_;
        break;
    }
    if (0 != tb)
      buttonList->append(tb);
  }

  if (!leftButtonList_.isEmpty())
    leftButtonList_.first()->setPosition(WebButton::Left);

  if (!rightButtonList_.isEmpty())
    rightButtonList_.last()->setPosition(WebButton::Right);
}

  void
WebClient::_resetLayout()
{
  KConfig c("kwinwebrc");
  c.setGroup("General");
  shape_ = c.readBoolEntry("Shape", true);

  //  ____________________________________
  // |  |                              |  |
  // |Xo|     titleSpacer              |v^| <--- topLayout
  // |__|______________________________|__|
  // | |                                | |
  // | |                                | |
  // | |     fake window                | |
  // | |                                | | <--- midLayout
  // | |                                | |
  // | |                                | |
  // | |________________________________| |
  // |____________________________________|

  const uint sideMargin    = borderSize_;
  const uint bottomMargin  = borderSize_;

  if (0 != titleHeight_ % 2)
    titleHeight_ += 1;

  delete mainLayout_;

  mainLayout_  = new QVBoxLayout(widget(), 0, 0);

  titleSpacer_ = new QSpacerItem ( 0, titleHeight_, QSizePolicy::Expanding,
      QSizePolicy::Fixed);

  QBoxLayout * topLayout = new QBoxLayout(mainLayout_, QBoxLayout::LeftToRight, 0, 0);

  _createButtons();

  // Add left-side buttons.

  for (QPtrListIterator<WebButton> it(leftButtonList_); it.current(); ++it)
  {
    topLayout->addWidget(it.current(), Qt::AlignVCenter);
    topLayout->setStretchFactor(it.current(), 0);
    it.current()->setFixedSize(titleHeight_, titleHeight_);
  }

  topLayout->addItem(titleSpacer_);

  // Add right-side buttons.

  for (QPtrListIterator<WebButton> it(rightButtonList_); it.current(); ++it)
  {
    topLayout->addWidget(it.current(), Qt::AlignVCenter);
    it.current()->setFixedSize(titleHeight_, titleHeight_);
  }

  // -------------------------------------------------------------------

  QHBoxLayout * midLayout   = new QHBoxLayout(mainLayout_, 0, 0);

  midLayout->addSpacing(sideMargin);
  if( isPreview())
    midLayout->addWidget(new QLabel( i18n( "<center><b>Web</b></center>" ), widget()));
  else
    midLayout->addItem( new QSpacerItem( 0, 0 )); // no widget in the middle
  midLayout->addSpacing(sideMargin);

  // -------------------------------------------------------------------

  mainLayout_->addSpacing(bottomMargin);

  // Make sure that topLayout doesn't stretch - midLayout should take
  // all spare space.

  mainLayout_->setStretchFactor(topLayout, 0);
  mainLayout_->setStretchFactor(midLayout, 1);
}

void WebClient::borders(int& left, int& right, int& top, int& bottom) const
{
    left = borderSize_;
    right = borderSize_;
    top =  titleHeight_;
    bottom = borderSize_;
}

void WebClient::resize( const QSize& s )
{
    widget()->resize( s );
}

QSize WebClient::minimumSize() const
{
    return QSize( 200, 50 );
}

bool WebClient::isTool()
{
  NET::WindowType type = windowType(NET::NormalMask|NET::ToolbarMask|NET::UtilityMask|NET::MenuMask);
  return ((type==NET::Toolbar)||(type==NET::NET::Utility)||(type==NET::Menu));
}

bool WebClient::eventFilter( QObject* o, QEvent* e )
{
    if( o != widget())
    return false;
    switch( e->type())
    {
    case QEvent::Resize:
        resizeEvent(static_cast< QResizeEvent* >( e ) );
        return true;
    case QEvent::Paint:
        paintEvent(static_cast< QPaintEvent* >( e ) );
        return true;
    case QEvent::MouseButtonDblClick:
        mouseDoubleClickEvent(static_cast< QMouseEvent* >( e ) );
        return true;
    case QEvent::MouseButtonPress:
        processMousePressEvent(static_cast< QMouseEvent* >( e ) );
        return true;
    default:
        break;
    }
    return false;
}


KDecoration* WebFactory::createDecoration( KDecorationBridge* b )
{
  return(new WebClient(b, this));
}

bool WebFactory::reset(unsigned long changed)
{
  // Do we need to "hit the wooden hammer" ?
  bool needHardReset = true;
  if (changed & SettingColors || changed & SettingFont)
  {
    needHardReset = false;
  }

  if (needHardReset) {
    return true;
  } else {
    resetDecorations(changed);
    return false;
  }
}

QValueList< WebFactory::BorderSize > WebFactory::borderSizes() const
{ // the list must be sorted
  return QValueList< BorderSize >() << BorderNormal << BorderLarge <<
      BorderVeryLarge <<  BorderHuge << BorderVeryHuge << BorderOversized;
}

}

#include "Web.moc"
// vim:ts=2:sw=2:tw=78:set et:
