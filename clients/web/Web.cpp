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

#include <unistd.h> // for usleep
#include <math.h>

#include <qlayout.h>
#include <qpainter.h>

#include <netwm.h>
#include <kwin/workspace.h>
#undef Bool

#include <kconfig.h>
#include <kstddirs.h>

#include <kapp.h>
#include <kwin/options.h>

#include "Web.h"
#include "WebButton.h"
#include "WebButtonHelp.h"
#include "WebButtonIconify.h"
#include "WebButtonClose.h"
#include "WebButtonSticky.h"
#include "WebButtonMaximize.h"
#include "WebButtonLower.h"

using namespace KWinInternal;

extern "C"
{
  Client * allocate(Workspace * ws, WId w, int tool)
  {
    return new Web(ws, w, tool != 0);
  }

  void init()
  {
    // Empty.
  }

  void reset()
  {
    Workspace::self()->slotResetAllClientsDelayed();
    // Empty.
  }

  void deinit()
  {
    // Empty.
  }
}

Web::Web(Workspace * ws, WId w, bool tool, QWidget * parent, const char * name)
  : Client        (ws, w, parent, name, WResizeNoErase),
    tool_         (tool),
    mainLayout_   (0),
    titleSpacer_  (0)
{
  setBackgroundMode(NoBackground);

  _resetLayout();

  leftButtonList_   .setAutoDelete(true);
  rightButtonList_  .setAutoDelete(true);

  connect(options, SIGNAL(resetClients()), this, SLOT(slotReset()));
}

Web::~Web()
{
  // Empty.
}

  void
Web::resizeEvent(QResizeEvent * e)
{
  Client::resizeEvent(e);
  doShape();
  repaint();
}

  void
Web::captionChange(const QString &)
{
  repaint();
}

  void
Web::paintEvent(QPaintEvent * pe)
{
  QRect titleRect(titleSpacer_->geometry());
  titleRect.setTop(1);

  QPainter p(this);

  p.setPen(Qt::black);
  p.setBrush(colorGroup().background());

  p.setClipRegion(pe->region() - titleRect);

  p.drawRect(rect());

  p.setClipRegion(pe->region());

  p.fillRect(titleRect, options->color(Options::TitleBar, isActive()));

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

  p.setFont(options->font(isActive(), tool_));

  p.setPen(options->color(Options::Font, isActive()));

  p.drawText(titleSpacer_->geometry(), AlignCenter, caption());
}

  void
Web::doShape()
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
Web::showEvent(QShowEvent *)
{
  doShape();
  repaint();
}

  void
Web::windowWrapperShowEvent(QShowEvent *)
{
  doShape();
  repaint();
}

  void
Web::mouseDoubleClickEvent(QMouseEvent * e)
{
  if (titleSpacer_->geometry().contains(e->pos()))
  {
    workspace()
      ->performWindowOperation(this, options->operationTitlebarDblClick());
  }
}

  void
Web::stickyChange(bool b)
{
  emit(stkyChange(b));
}

  void
Web::maximizeChange(bool b)
{
  emit(maxChange(b));
}

  void
Web::activeChange(bool)
{
  repaint();
}

  Client::MousePosition
Web::mousePosition(const QPoint & p) const
{
  int x = p.x();
  int y = p.y();

  if (y < titleSpacer_->geometry().height())
  {
    // rikkus: this style is not designed to be resizable at the top edge.

#if 0
    if ((y < 4 && x < 20) || x < 4)
      return Client::TopLeft;
    else if ((y < 4 && x > width() - 20) || x > width() - 4)
      return Client::TopRight;
    else if (y < 4)
      return Client::Top;
    else
#endif
      return Client::Center;
  }
  else if (y < height() - 20)
  {
    if (x < 4)
      return Client::Left;
    else
      return Client::Right;
  }
  else
  {
    if (x < 20)
      return Client::BottomLeft;
    else
      if (x > width() - 20)
        return Client::BottomRight;
      else
        return Client::Bottom;
  }

  return Client::mousePosition(p);
}

  void
Web::slotReset()
{
  _resetLayout();
  repaint();
}

  void
Web::slotMaximize(int button)
{
  switch (button)
  {
    case MidButton:
      maximize(MaximizeVertical);
      break;

    case RightButton:
      maximize(MaximizeHorizontal);
      break;

    case LeftButton:
    default:
      maximize(MaximizeFull);
  }
}

  WebButton *
Web::_createButton(const QString & s, QWidget * parent)
{
  WebButton * b = 0;

  if (("Help" == s) && providesContextHelp())
  {
    b = new WebButtonHelp(parent);
    connect(b, SIGNAL(help()), this, SLOT(contextHelp()));
  }

  else if ("Sticky" == s)
  {
    b = new WebButtonSticky(isSticky(), parent);
    connect(b, SIGNAL(toggleSticky()), this, SLOT(toggleSticky()));
    connect(this, SIGNAL(stkyChange(bool)), b, SLOT(slotStickyChange(bool)));
  }

  else if ("Iconify" == s && isMinimizable())
  {
    b = new WebButtonIconify(parent);
    connect(b, SIGNAL(iconify()), this, SLOT(iconify()));
  }

  else if ("Maximize" == s && isMaximizable())
  {
    b = new WebButtonMaximize(isMaximized(), parent);
    connect(b, SIGNAL(maximize(int)), this, SLOT(slotMaximize(int)));
    connect(this, SIGNAL(maxChange(bool)), b, SLOT(slotMaximizeChange(bool)));
  }

  else if ("Close" == s)
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
Web::_createButtons()
{
  leftButtonList_   .clear();
  rightButtonList_  .clear();

  QString buttons = options->titleButtonsLeft() + "|" + options->titleButtonsRight();
  QList<WebButton> *buttonList = &leftButtonList_;
  for (unsigned int i = 0; i < buttons.length(); ++i)
  {
    WebButton * tb = 0;
    switch (buttons[i].latin1())
    {
      case 'S': // Sticky
        tb = _createButton("Sticky", this);
        break;

      case 'H': // Help
        tb = _createButton("Help", this);
        break;

      case 'I': // Minimize
        tb = _createButton("Iconify", this);
        break;

      case 'A': // Maximize
        tb = _createButton("Maximize", this);
        break;

      case 'X': // Close
        tb = _createButton("Close", this);
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
Web::_resetLayout()
{
  KConfig c(locate("config", "kwinwebrc"));
  c.setGroup("General");
  shape_ = c.readBoolEntry("Shape", true);

  //  ____________________________________
  // |  |                              |  |
  // |Xo|     titleSpacer              |v^| <--- topLayout
  // |__|______________________________|__|
  // | |                                | |
  // | |                                | |
  // | |     windowWrapper              | |
  // | |                                | | <--- midLayout
  // | |                                | |
  // | |                                | |
  // | |________________________________| |
  // |____________________________________|

  const uint sideMargin    = 4;
  const uint bottomMargin  = 4;
  const uint textVMargin   = 2;

  QFontMetrics fm(options->font(isActive(), tool_));

  uint titleHeight = QMAX(14, fm.height() + textVMargin * 2);

  if (0 != titleHeight % 2)
    titleHeight += 1;

  delete mainLayout_;

  mainLayout_  = new QVBoxLayout(this, 0, 0);

  titleSpacer_ =
    new QSpacerItem
    (
     0,
     titleHeight,
     QSizePolicy::Expanding,
     QSizePolicy::Fixed
    );

  QHBoxLayout * topLayout   = new QHBoxLayout(mainLayout_, 0, 0);

  _createButtons();

  // Add left-side buttons.

  for (QListIterator<WebButton> it(leftButtonList_); it.current(); ++it)
  {
    topLayout->addWidget(it.current(), Qt::AlignVCenter);
    topLayout->setStretchFactor(it.current(), 0);
    it.current()->setFixedSize(titleHeight, titleHeight);
  }

  topLayout->addItem(titleSpacer_);

  // Add right-side buttons.

  for (QListIterator<WebButton> it(rightButtonList_); it.current(); ++it)
  {
    topLayout->addWidget(it.current(), Qt::AlignVCenter);
    it.current()->setFixedSize(titleHeight, titleHeight);
  }

  // -------------------------------------------------------------------

  QHBoxLayout * midLayout   = new QHBoxLayout(mainLayout_, 0, 0);
 
  midLayout->addSpacing(sideMargin);
  midLayout->addWidget(windowWrapper());
  midLayout->addSpacing(sideMargin);

  // -------------------------------------------------------------------

  mainLayout_->addSpacing(bottomMargin);

  // Make sure that topLayout doesn't stretch - midLayout should take
  // all spare space.

  mainLayout_->setStretchFactor(topLayout, 0);
  mainLayout_->setStretchFactor(midLayout, 1);
}

  void
Web::animateIconifyOrDeiconify(bool /* iconify */)
{
  // Nice and simple ;)
}

#include "Web.moc"
// vim:ts=2:sw=2:tw=78:set et:
