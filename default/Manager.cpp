/*
  Default KWin client

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

#include <qpainter.h>
#include <qlayout.h>

#include "../options.h"
#include "../workspace.h"

#include "Manager.h"
#include "Static.h"
#include "TitleBar.h"
#include "ResizeBar.h"

extern "C"
{
  Client * allocate(Workspace * workSpace, WId winId)
  {
    return new Default::Manager(workSpace, winId);
  }
}

namespace Default
{

Manager::Manager(
  Workspace * workSpace,
  WId id,
  QWidget * parent,
  const char * name
)
  : Client(workSpace, id, parent, name)
{

  connect(options, SIGNAL(resetClients()), this, SLOT(slotReset()));

  titleBar_   = new TitleBar(this);
  titleBar_->setFixedHeight( Static::instance()->titleHeight() );
  resizeBar_  = new ResizeBar(this, this);
  resizeBar_->setFixedHeight( RESIZE_BAR_HEIGHT );

  QVBoxLayout* vbox = new QVBoxLayout( this );
  vbox->addWidget( titleBar_ );
  QHBoxLayout* hbox = new QHBoxLayout;
  vbox->addLayout( hbox, 10 );
  hbox->addSpacing( 3 );
  hbox->addWidget( windowWrapper(), 10 );
  hbox->addSpacing( 3 );
  vbox->addWidget( resizeBar_ );
  
  _updateDisplay();
}

Manager::~Manager()
{
}

  void
Manager::slotReset()
{
  Static::instance()->update();
  titleBar_->setFixedHeight( Static::instance()->titleHeight() );
  _updateDisplay();
}

  void
Manager::captionChange(const QString &)
{
  titleBar_->updateText();
}

  void
Manager::stickyChange(bool b)
{
  emit(stickyStatusChanged(b));
}

  void
Manager::paletteChange(const QPalette &)
{
  Static::instance()->update();
  _updateDisplay();
}

  void
Manager::activeChange(bool b)
{
  titleBar_->setActive(b);
}

  void
Manager::maximizeChange(bool b)
{
  emit(maximiseChanged(b));
}

  void
Manager::_updateDisplay()
{
  titleBar_->updateDisplay();
  resizeBar_->updateDisplay();
}

  void
Manager::paintEvent(QPaintEvent * e)
{
  QRect r(e->rect());

  bool intersectsLeft =
    r.intersects(QRect(0, 0, 1, height()));

  bool intersectsRight =
    r.intersects(QRect(width() - 1, 0, width(), height()));

  if (intersectsLeft || intersectsRight) {

    QPainter p(this);
    p.setPen(Qt::black);

    if (intersectsLeft)
      p.drawLine(0, r.top(), 0, r.bottom());

    if (intersectsRight)
      p.drawLine(width() - 1, r.top(), width() - 1, r.bottom());
  }
}

  void
Manager::raise()
{
  workspace()->raiseClient(this);
}

  void
Manager::vMax()
{
  maximize(MaximizeVertical);
}

  void
Manager::resizeEvent(QResizeEvent * e)
{
  Client::resizeEvent(e);
}


  void
Manager::fakeMouseEvent(QMouseEvent * e, QWidget * w)
{
  QPoint adjustedPos = w->pos() + e->pos();

  QMouseEvent fake(e->type(), adjustedPos, e->button(), e->state());
  Client::event(&fake);
}


} // End namespace

// vim:ts=2:sw=2:tw=78
#include "Manager.moc"
