/*
  RISC OS KWin client
  
  Copyright 2000
    Rik Hemsley <rik@kde.org>

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

// Qt includes
#include <qlayout.h>
#include <qpixmap.h>
#include <qpainter.h>
#include <qpushbutton.h>

// Local includes
#include "../../options.h"
#include "../../workspace.h"
#include "riscosclient.h"

#include "title_inactive_left.xpm"
#include "title_inactive.xpm"
#include "title_inactive_right.xpm"
#include "title_active_left.xpm"
#include "title_active.xpm"
#include "title_active_right.xpm"
#include "resize_bar_left.xpm"
#include "resize_bar_mid.xpm"
#include "resize_bar_right.xpm"
#include "base_up.xpm"
#include "base_down.xpm"
#include "close.xpm"
#include "unmax.xpm"
#include "max.xpm"
#include "lower.xpm"
#include "bar.xpm"

#include <X11/Xlib.h>

extern "C"
{
  Client * allocate(Workspace * workSpace, WId winId)
  {
    return new RiscOSClient(workSpace, winId);
  }
}

RiscOSButton::RiscOSButton(RiscOSClient * parent)
  : QButton(parent, "RiscOSButton"),
    client_(parent)
{
  setFixedSize(18, 18);
  px_base_up_   = QPixmap(base_up_xpm);
  px_base_down_ = QPixmap(base_down_xpm);
  
  XSetWindowAttributes wsa;
  wsa.save_under = true;
	XChangeWindowAttributes(qt_xdisplay(), winId(), 0, &wsa);
}

  void
RiscOSButton::drawButton(QPainter * p)
{
  if (isDown())
    p->drawPixmap(0, 0, px_base_down_);
  else
    p->drawPixmap(0, 0, px_base_up_);

  p->drawPixmap(3, 3, px_symbol_);
}

  void
RiscOSButton::setSymbol(const QPixmap & p)
{
  px_symbol_ = p;
  repaint(false);
}

RiscOSLowerButton::RiscOSLowerButton(RiscOSClient * parent)
  : RiscOSButton(parent)
{
// TODO  connect(this, SIGNAL(clicked()), client(), (SLOT(lowerAndDeactivate())));
  setSymbol(QPixmap(lower_xpm));
}

RiscOSCloseButton::RiscOSCloseButton(RiscOSClient * parent)
  : RiscOSButton(parent)
{
  connect(this, SIGNAL(clicked()), client(), (SLOT(closeWindow())));
  setSymbol(QPixmap(close_xpm));
}

RiscOSIconifyButton::RiscOSIconifyButton(RiscOSClient * parent)
  : RiscOSButton(parent)
{
  connect(this, SIGNAL(clicked()), client(), (SLOT(iconify())));
  setSymbol(QPixmap(bar_xpm));
}

RiscOSMaximiseButton::RiscOSMaximiseButton(RiscOSClient * parent)
  : RiscOSButton(parent)
{
  setSymbol(QPixmap(max_xpm));
}

  void
RiscOSMaximiseButton::setOn(bool on)
{
  if (on)
    setSymbol(QPixmap(unmax_xpm));
  else
    setSymbol(QPixmap(max_xpm));
}

  void
RiscOSMaximiseButton::mouseReleaseEvent(QMouseEvent * e)
{
  RiscOSButton::mouseReleaseEvent(e);

  if (!rect().contains(e->pos()))
    return;

  switch (e->button())
  {
    case RightButton:
      client()->maximizeNoRaise();
      break;

    case MidButton:
      client()->maximizeVertically();
      break;

    case LeftButton:
    default:
      client()->maximizeAndRaise();
      break;
  }
}

RiscOSResizeButton::RiscOSResizeButton(RiscOSClient * parent)
  : RiscOSButton(parent)
{
}

RiscOSTitleBar::RiscOSTitleBar(RiscOSClient * parent)
  : QWidget(parent, "RiscOSTitleBar"),
    client_(parent)
{
  setFixedHeight(18);

  px_inactive_left_   = QPixmap(title_inactive_left_xpm);
  px_inactive_        = QPixmap(title_inactive_xpm);
  px_inactive_right_  = QPixmap(title_inactive_right_xpm);
  px_active_left_     = QPixmap(title_active_left_xpm);
  px_active_          = QPixmap(title_active_xpm);
  px_active_right_    = QPixmap(title_active_right_xpm);
  
  XSetWindowAttributes wsa;
  wsa.save_under = true;
	XChangeWindowAttributes(qt_xdisplay(), winId(), 0, &wsa);

  buf_.resize(128,18);
  _updatePixmap();
}

  void
RiscOSTitleBar::_updatePixmap()
{
  if (size().width() > buf_.width())
    buf_.resize(size());

  QPainter p;
  p.begin(&buf_);

  if (client_->isActive()) {

    p.drawPixmap(0, 0, px_active_left_);
    p.drawTiledPixmap(2, 0, width() - 4, 18, px_active_);
    p.drawPixmap(width() - 2, 0, px_active_right_);

  } else {
    
    p.drawPixmap(0, 0, px_inactive_left_);
    p.drawTiledPixmap(2, 0, width() - 4, 18, px_inactive_);
    p.drawPixmap(width() - 2, 0, px_inactive_right_);

  }

  p.setPen(client_->colour());
  p.setFont(client_->font());
  p.setFont(QFont("times", 10, QFont::Bold));
  p.drawText(3, 0, width() - 6, 18, AlignCenter, client_->caption());
}

  void
RiscOSTitleBar::resizeEvent(QResizeEvent * e)
{
  QWidget::resizeEvent(e);
  _updatePixmap();
}

  void
RiscOSTitleBar::paintEvent(QPaintEvent * e)
{
  QRect r(e->rect());
  bitBlt(this, r.topLeft(), &buf_, r, Qt::CopyROP);
}
  
  void
RiscOSTitleBar::mousePressEvent(QMouseEvent * e)
{
  switch (e->button()) {

    case MidButton:
      client_->workspace()->clientPopup(client_)->popup(e->globalPos());
      break;

    case LeftButton:
      client_->workspace()->raiseClient(client_);
      client_->workspace()->requestFocus(client_);

    case RightButton:

      clientPosToMousePos_ = e->globalPos() - client_->pos();
      break;

    default:
      break;
  }
}

  void
RiscOSTitleBar::mouseReleaseEvent(QMouseEvent * e)
{
}

  void
RiscOSTitleBar::mouseMoveEvent(QMouseEvent * e)
{
    QPoint adjustedForCursor = e->globalPos() - clientPosToMousePos_;

    QPoint adjustedForSnap =
      client_->workspace()->adjustClientPosition(client_, adjustedForCursor);

    client_->move(adjustedForSnap);

// Do we really need this ?
//  QApplication::syncX();
}

  void
RiscOSTitleBar::mouseDoubleClickEvent(QMouseEvent * e)
{
  client_->setShade(!client_->isShade());
}

  void
RiscOSTitleBar::update()
{
  _updatePixmap();
  repaint(false);
}

RiscOSResizeBar::RiscOSResizeBar(RiscOSClient * parent)
  : QWidget(parent, "RiscOSResizeBar")
{
  setFixedHeight(8);

  left_   = new RiscOSResizeLeft(this, parent);
  mid_    = new RiscOSResizeMid(this, parent);
  right_  = new RiscOSResizeRight(this, parent);
  
  QHBoxLayout * layout = new QHBoxLayout(this);

  layout->addWidget(left_);
  layout->addWidget(mid_, 1);
  layout->addWidget(right_);

  XSetWindowAttributes wsa;
  wsa.save_under = true;

	XChangeWindowAttributes(qt_xdisplay(), winId(), 0, &wsa);
	XChangeWindowAttributes(qt_xdisplay(), left_->winId(), 0, &wsa);
	XChangeWindowAttributes(qt_xdisplay(), mid_->winId(), 0, &wsa);
	XChangeWindowAttributes(qt_xdisplay(), right_->winId(), 0, &wsa);
}
  
  void
RiscOSResizeBar::update()
{
  mid_->update();
}

RiscOSResizeMid::RiscOSResizeMid(RiscOSResizeBar * parent, RiscOSClient * c)
  : QWidget(parent, "RiscOSResizeMid"),
    client_(c)
{
  setCursor(Qt::sizeVerCursor);

  px_left_  = QPixmap(resize_bar_left_xpm);
  px_mid_   = QPixmap(resize_bar_mid_xpm);
  px_right_ = QPixmap(resize_bar_right_xpm);
}

   void
RiscOSResizeMid::_updatePixmap()
{
  if (size().width() > buf_.width())
    buf_.resize(size());

  QPainter p;
  p.begin(&buf_);
  p.drawLine(0, 8, width(), 8);

  p.drawPixmap(0, 0, px_left_);
  p.drawTiledPixmap(2, 0, width() - 4, 7, px_mid_);
  p.drawPixmap(width() - 2, 0, px_right_);
}

  void
RiscOSResizeMid::update()
{
  _updatePixmap();
  repaint(false);
}
 
  void
RiscOSResizeMid::resizeEvent(QResizeEvent * e)
{
  QWidget::resizeEvent(e);
  _updatePixmap();
}

  void
RiscOSResizeMid::paintEvent(QPaintEvent * e)
{
  QRect r(e->rect());
  bitBlt(this, r.topLeft(), &buf_, r, Qt::CopyROP);
}

  void
RiscOSResizeMid::mouseMoveEvent(QMouseEvent * e)
{
  QRect g = client_->geometry();
  g.setBottom(e->globalPos().y());

  QSize adjustedSize = client_->adjustedSize(g.size());

  if (adjustedSize != client_->size()) {
    g.setBottom(g.top() + adjustedSize.height());
    client_->setGeometry(g);
  }
}

RiscOSResizeLeft::RiscOSResizeLeft(RiscOSResizeBar * parent, RiscOSClient * c)
  : QWidget(parent, "RiscOSResizeLeft"),
    client_(c)
{
  setCursor(Qt::sizeBDiagCursor);

  setFixedSize(30, 8);
  setBackgroundColor(Qt::black);
  QPixmap pixmap;
  pixmap.resize(30, 8);
  pixmap.fill(Qt::black);
  QPainter p(&pixmap);
  p.drawPixmap(1,   0, QPixmap(resize_bar_left_xpm));
  p.drawPixmap(3,   0, QPixmap(resize_bar_mid_xpm));
  p.drawPixmap(28,  0, QPixmap(resize_bar_right_xpm));
  setBackgroundPixmap(pixmap);
}

  void
RiscOSResizeLeft::mouseMoveEvent(QMouseEvent * e)
{
  QRect g = client_->geometry();
  g.setBottom(e->globalPos().y());
  g.setLeft(e->globalPos().x());

	QSize adjustedSize = client_->adjustedSize(g.size());

  if (adjustedSize != client_->size()) {
    g.setLeft(g.right() - adjustedSize.width());
    g.setBottom(g.top() + adjustedSize.height());
    client_->setGeometry(g);
  }
}

RiscOSResizeRight::RiscOSResizeRight(RiscOSResizeBar * parent, RiscOSClient * c)
  : QWidget(parent, "RiscOSResizeRight"),
    client_(c)
{
  setCursor(Qt::sizeFDiagCursor);

  setFixedSize(30, 8);
  setBackgroundColor(Qt::black);
  QPixmap pixmap;
  pixmap.resize(30, 8);
  pixmap.fill(Qt::black);
  QPainter p(&pixmap);
  p.drawPixmap(0,   0, QPixmap(resize_bar_left_xpm));
  p.drawPixmap(2,   0, QPixmap(resize_bar_mid_xpm));
  p.drawPixmap(27,  0, QPixmap(resize_bar_right_xpm));
  setBackgroundPixmap(pixmap);
}

  void
RiscOSResizeRight::mouseMoveEvent(QMouseEvent * e)
{
  QRect g = client_->geometry();
  g.setBottom(e->globalPos().y());
  g.setRight(e->globalPos().x());

	QSize adjustedSize = client_->adjustedSize(g.size());

  if (adjustedSize != client_->size()) {
    g.setRight(g.left() + adjustedSize.width());
    g.setBottom(g.top() + adjustedSize.height());
    client_->setGeometry(g);
  }
}

RiscOSClient::RiscOSClient(
  Workspace * workSpace,
  WId winId,
  QWidget * parent,
  const char * name
)
  : Client(workSpace, winId, parent, name, WResizeNoErase)
{
  setMouseTracking(false); // I don't want this !
  setBackgroundColor(Qt::black);

  connect(options, SIGNAL(resetClients()), this, SLOT(slotReset()));

  lower_  = new RiscOSLowerButton(this);
  close_  = new RiscOSCloseButton(this);

  title_  = new RiscOSTitleBar(this);

  iconify_  = new RiscOSIconifyButton(this);
  maximize_ = new RiscOSMaximiseButton(this);

  resizeBar_ = new RiscOSResizeBar(this);

  // Layout

  // Border | Close | Titlebar | Max | Lower | Border
  QHBoxLayout * titleLayout   = new QHBoxLayout(0, "titleLayout");
  titleLayout->addSpacing(1);
  titleLayout->addWidget(lower_);
  titleLayout->addSpacing(1);
  titleLayout->addWidget(close_);
  titleLayout->addSpacing(1);
  titleLayout->addWidget(title_, 1);
  titleLayout->addSpacing(1);
  titleLayout->addWidget(iconify_);
  titleLayout->addSpacing(1);
  titleLayout->addWidget(maximize_);
  titleLayout->addSpacing(1);
 
  // Border
  // Window
  // Border
  QHBoxLayout * windowLayout  = new QHBoxLayout(0, "windowLayout");
  windowLayout->addSpacing(1);
  windowLayout->addWidget(windowWrapper(), 1);
  windowLayout->addSpacing(1);
  
  // Border
  // Titlebar
  // Window layout
  // Resize bar
  QVBoxLayout * mainLayout = new QVBoxLayout(this, 0, 0, "mainLayout");
  mainLayout->addSpacing(1);
  mainLayout->addLayout(titleLayout);
  mainLayout->addSpacing(1);
  mainLayout->addLayout(windowLayout, 1);
  mainLayout->addSpacing(1);
  mainLayout->addWidget(resizeBar_);
  
  title_->update();
  resizeBar_->update();
}

  void
RiscOSClient::slotReset()
{
  // Empty.
}
    
  void
RiscOSClient::captionChange(const QString &)
{
  title_->update();
}

  void
RiscOSClient::activeChange(bool b)
{
  title_->update();
}

  void
RiscOSClient::maximizeChange(bool b)
{
  maximize_->setOn(b);
}

  void
RiscOSClient::maximizeAndRaise()
{
  maximize(MaximizeFull);
  workspace()->raiseClient(this);
  workspace()->requestFocus(this);
}

  void
RiscOSClient::maximizeVertically()
{
  maximize(MaximizeVertical);
  workspace()->raiseClient(this);
  workspace()->requestFocus(this);
}

  void
RiscOSClient::maximizeNoRaise()
{
  maximize(MaximizeFull);
}

  void
RiscOSClient::resize(int w, int h)
{
  Client::resize(w, h);
}

  void
RiscOSClient::setShade(bool b)
{
#if 0
  // Hmm. This does screwy things to the layout.
  if (b)
    resizeBar_->hide();
  else
    resizeBar_->show();
#endif

  // And this is screwed. My window ends up the wrong size when unshaded.
//  Client::setShade(b);
}

// vim:ts=2:sw=2:tw=78
