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
#include "button_base_up.xpm"
#include "button_base_down.xpm"
#include "button_close.xpm"
#include "button_unmax.xpm"
#include "button_max.xpm"
#include "button_lower.xpm"
#include "button_iconify.xpm"

#include <X11/Xlib.h>

extern "C"
{
  Client * allocate(Workspace * workSpace, WId winId)
  {
    return new RiscOS::Manager(workSpace, winId);
  }
}

using namespace RiscOS;

QPixmap * px_button_base_up       = 0L;
QPixmap * px_button_base_down     = 0L;
QPixmap * px_button_iconify       = 0L;
QPixmap * px_button_close         = 0L;
QPixmap * px_button_lower         = 0L;
QPixmap * px_button_max           = 0L;
QPixmap * px_button_unmax         = 0L;
QPixmap * px_title_inactive_left  = 0L;
QPixmap * px_title_inactive       = 0L;
QPixmap * px_title_inactive_right = 0L;
QPixmap * px_title_active_left    = 0L;
QPixmap * px_title_active         = 0L;
QPixmap * px_title_active_right   = 0L;
QPixmap * px_resize_left          = 0L;
QPixmap * px_resize_mid           = 0L;
QPixmap * px_resize_right         = 0L;

Button::Button(Manager * parent)
  : QButton(parent, "Button"),
    client_(parent),
    px_symbol_(0L)
{
  setFixedSize(18, 18);
  
  XSetWindowAttributes wsa;
  wsa.save_under = true;
	XChangeWindowAttributes(qt_xdisplay(), winId(), 0, &wsa);
}

  void
Button::drawButton(QPainter * p)
{
  if (isDown())
    p->drawPixmap(0, 0, *px_button_base_down);
  else
    p->drawPixmap(0, 0, *px_button_base_up);

  if (px_symbol_ != 0)
    p->drawPixmap(3, 3, *px_symbol_);
}

  void
Button::setSymbol(QPixmap * p)
{
  px_symbol_ = p;
  repaint(false);
}

LowerButton::LowerButton(Manager * parent)
  : Button(parent)
{
// TODO  connect(this, SIGNAL(clicked()), client(), (SLOT(lowerAndDeactivate())));
  setSymbol(px_button_lower);
}

CloseButton::CloseButton(Manager * parent)
  : Button(parent)
{
  connect(this, SIGNAL(clicked()), client(), (SLOT(closeWindow())));
  setSymbol(px_button_close);
}

IconifyButton::IconifyButton(Manager * parent)
  : Button(parent)
{
  connect(this, SIGNAL(clicked()), client(), (SLOT(iconify())));
  setSymbol(px_button_iconify);
}

MaximiseButton::MaximiseButton(Manager * parent)
  : Button(parent)
{
  setSymbol(px_button_unmax);
}

  void
MaximiseButton::setOn(bool on)
{
  if (on)
    setSymbol(px_button_unmax);
  else
    setSymbol(px_button_max);
}

  void
MaximiseButton::mouseReleaseEvent(QMouseEvent * e)
{
  Button::mouseReleaseEvent(e);

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

ResizeButton::ResizeButton(Manager * parent)
  : Button(parent)
{
}

TitleBar::TitleBar(Manager * parent)
  : QWidget(parent, "TitleBar"),
    client_(parent)
{
  setFixedHeight(18);

  XSetWindowAttributes wsa;
  wsa.save_under = true;
	XChangeWindowAttributes(qt_xdisplay(), winId(), 0, &wsa);

  buf_ = new QPixmap;
  buf_->resize(128, 18);
  _updatePixmap();
}

TitleBar::~TitleBar()
{
  delete buf_;
}

  void
TitleBar::_updatePixmap()
{
  if (size().width() > buf_->width())
    buf_->resize(size());

  QPainter p;
  p.begin(buf_);

  if (client_->isActive()) {

    p.drawPixmap(0, 0, *px_title_active_left);
    p.drawTiledPixmap(2, 0, width() - 4, 18, *px_title_active);
    p.drawPixmap(width() - 2, 0, *px_title_active_right);
    p.setPen(Qt::white);
    p.setFont(options->font());
    p.drawText(3, 0, width() - 6, 18, AlignCenter, client_->caption());

  } else {
    
    p.drawPixmap(0, 0, *px_title_inactive_left);
    p.drawTiledPixmap(2, 0, width() - 4, 18, *px_title_inactive);
    p.drawPixmap(width() - 2, 0, *px_title_inactive_right);
    p.setPen(Qt::black);
    p.setFont(options->font());
    p.drawText(3, 0, width() - 6, 18, AlignCenter, client_->caption());

  }
}

  void
TitleBar::resizeEvent(QResizeEvent * e)
{
  QWidget::resizeEvent(e);
  _updatePixmap();
}

  void
TitleBar::paintEvent(QPaintEvent * e)
{
  QRect r(e->rect());
  bitBlt(this, r.topLeft(), buf_, r, Qt::CopyROP);
}
  
  void
TitleBar::mousePressEvent(QMouseEvent * e)
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
TitleBar::mouseReleaseEvent(QMouseEvent * e)
{
}

  void
TitleBar::mouseMoveEvent(QMouseEvent * e)
{
    QPoint adjustedForCursor = e->globalPos() - clientPosToMousePos_;

    QPoint adjustedForSnap =
      client_->workspace()->adjustClientPosition(client_, adjustedForCursor);

    client_->move(adjustedForSnap);

// Do we really need this ?
//  QApplication::syncX();
}

  void
TitleBar::mouseDoubleClickEvent(QMouseEvent * e)
{
  client_->setShade(!client_->isShade());
}

  void
TitleBar::update()
{
  _updatePixmap();
  repaint(false);
}

ResizeBar::ResizeBar(Manager * parent)
  : QWidget(parent, "ResizeBar")
{
  setFixedHeight(8);

  left_   = new ResizeLeft(this, parent);
  mid_    = new ResizeMid(this, parent);
  right_  = new ResizeRight(this, parent);
  
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
ResizeBar::update()
{
  mid_->update();
}

ResizeMid::ResizeMid(ResizeBar * parent, Manager * c)
  : QWidget(parent, "ResizeMid"),
    client_(c)
{
  setCursor(Qt::sizeVerCursor);
  buf_ = new QPixmap;
  buf_->resize(128, 8);
}

ResizeMid::~ResizeMid()
{
  delete buf_;
}

   void
ResizeMid::_updatePixmap()
{
  if (size().width() > buf_->width())
    buf_->resize(size());

  QPainter p;
  p.begin(buf_);
  p.drawLine(0, 8, width(), 8);

  p.drawPixmap(0, 0, *px_resize_left);
  p.drawTiledPixmap(2, 0, width() - 4, 7, *px_resize_mid);
  p.drawPixmap(width() - 2, 0, *px_resize_right);
}

  void
ResizeMid::update()
{
  _updatePixmap();
  repaint(false);
}
 
  void
ResizeMid::resizeEvent(QResizeEvent * e)
{
  QWidget::resizeEvent(e);
  _updatePixmap();
}

  void
ResizeMid::paintEvent(QPaintEvent * e)
{
  QRect r(e->rect());
  bitBlt(this, r.topLeft(), buf_, r, Qt::CopyROP);
}

  void
ResizeMid::mouseMoveEvent(QMouseEvent * e)
{
  QRect g = client_->geometry();
  g.setBottom(e->globalPos().y());

  QSize adjustedSize = client_->adjustedSize(g.size());

  if (adjustedSize != client_->size()) {
    g.setBottom(g.top() + adjustedSize.height());
    client_->setGeometry(g);
  }
}

ResizeLeft::ResizeLeft(ResizeBar * parent, Manager * c)
  : QWidget(parent, "ResizeLeft"),
    client_(c)
{
  setCursor(Qt::sizeBDiagCursor);

  setFixedSize(30, 8);
  setBackgroundColor(Qt::black);
  QPixmap pixmap;
  pixmap.resize(30, 8);
  pixmap.fill(Qt::black);
  QPainter p(&pixmap);
  p.drawPixmap(1,   0, *px_resize_left);
  p.drawPixmap(3,   0, *px_resize_mid);
  p.drawPixmap(28,  0, *px_resize_right);
  setBackgroundPixmap(pixmap);
}

  void
ResizeLeft::mouseMoveEvent(QMouseEvent * e)
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

ResizeRight::ResizeRight(ResizeBar * parent, Manager * c)
  : QWidget(parent, "ResizeRight"),
    client_(c)
{
  setCursor(Qt::sizeFDiagCursor);

  setFixedSize(30, 8);
  setBackgroundColor(Qt::black);
  QPixmap pixmap;
  pixmap.resize(30, 8);
  pixmap.fill(Qt::black);
  QPainter p(&pixmap);
  p.drawPixmap(0,   0, *px_resize_left);
  p.drawPixmap(2,   0, *px_resize_mid);
  p.drawPixmap(27,  0, *px_resize_right);
  setBackgroundPixmap(pixmap);
}

  void
ResizeRight::mouseMoveEvent(QMouseEvent * e)
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

Manager::Manager(
  Workspace * workSpace,
  WId winId,
  QWidget * parent,
  const char * name
)
  : Client(workSpace, winId, parent, name, WResizeNoErase),
    pixmapsLoaded_(false)
{
  setMouseTracking(false); // I don't want this !
  setBackgroundColor(Qt::black);

  _loadPixmaps();

  connect(options, SIGNAL(resetClients()), this, SLOT(slotReset()));

  lower_      = new LowerButton     (this);
  close_      = new CloseButton     (this);
  title_      = new TitleBar        (this);
  iconify_    = new IconifyButton   (this);
  maximize_   = new MaximiseButton  (this);
  resizeBar_  = new ResizeBar       (this);

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
Manager::slotReset()
{
  // Empty.
}
    
  void
Manager::captionChange(const QString &)
{
  title_->update();
}

  void
Manager::activeChange(bool b)
{
  title_->update();
}

  void
Manager::maximizeChange(bool b)
{
  maximize_->setOn(b);
}

  void
Manager::maximizeAndRaise()
{
  maximize(MaximizeFull);
  workspace()->raiseClient(this);
  workspace()->requestFocus(this);
}

  void
Manager::maximizeVertically()
{
  maximize(MaximizeVertical);
  workspace()->raiseClient(this);
  workspace()->requestFocus(this);
}

  void
Manager::maximizeNoRaise()
{
  maximize(MaximizeFull);
}

  void
Manager::resize(int w, int h)
{
  Client::resize(w, h);
}

  void
Manager::setShade(bool b)
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

  void
Manager::_loadPixmaps()
{
  if (pixmapsLoaded_) {

    delete px_button_base_up;       px_button_base_up       = 0L;
    delete px_button_base_down;     px_button_base_down     = 0L;
    delete px_button_iconify;       px_button_iconify       = 0L;
    delete px_button_close;         px_button_close         = 0L;
    delete px_button_lower;         px_button_lower         = 0L;
    delete px_button_max;           px_button_max           = 0L;
    delete px_button_unmax;         px_button_unmax         = 0L;
    delete px_title_inactive_left;  px_title_inactive_left  = 0L;
    delete px_title_inactive;       px_title_inactive       = 0L;
    delete px_title_inactive_right; px_title_inactive_right = 0L;
    delete px_title_active_left;    px_title_active_left    = 0L;
    delete px_title_active;         px_title_active         = 0L;
    delete px_title_active_right;   px_title_active_right   = 0L;
    delete px_resize_left;          px_resize_left          = 0L;
    delete px_resize_mid;           px_resize_mid           = 0L;
    delete px_resize_right;         px_resize_right         = 0L;
  }

  px_button_base_up       = new QPixmap(button_base_up_xpm);
  px_button_base_down     = new QPixmap(button_base_down_xpm);
  px_button_iconify       = new QPixmap(button_iconify_xpm);
  px_button_close         = new QPixmap(button_close_xpm);
  px_button_lower         = new QPixmap(button_lower_xpm);
  px_button_max           = new QPixmap(button_max_xpm);
  px_button_unmax         = new QPixmap(button_unmax_xpm);
  px_title_inactive_left  = new QPixmap(title_inactive_left_xpm);
  px_title_inactive       = new QPixmap(title_inactive_xpm);
  px_title_inactive_right = new QPixmap(title_inactive_right_xpm);
  px_title_active_left    = new QPixmap(title_active_left_xpm);
  px_title_active         = new QPixmap(title_active_xpm);
  px_title_active_right   = new QPixmap(title_active_right_xpm);
  px_resize_left          = new QPixmap(resize_bar_left_xpm);
  px_resize_mid           = new QPixmap(resize_bar_mid_xpm);
  px_resize_right         = new QPixmap(resize_bar_right_xpm);

  pixmapsLoaded_ = true;
}

// vim:ts=2:sw=2:tw=78
