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

#include "../../options.h"
#include "../../workspace.h"

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
  setBackgroundMode(NoBackground);

  connect(options, SIGNAL(resetClients()), this, SLOT(slotReset()));

  titleBar_   = new TitleBar(this);
  resizeBar_  = new ResizeBar(this, this);

  activateLayout();
}

Manager::~Manager()
{
}

  void
Manager::slotReset()
{
  Static::instance()->update();
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
Manager::setShade(bool)
{
  // Wait for parent class version to work.
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
    
  Client::MousePosition
Manager::mousePosition(const QPoint & p) const
{
  if (titleBar_->rect().contains(p))
    return Client::Center;
  else
    return Client::Nowhere;
}

  void
Manager::toggleSticky()
{
  setSticky(!isSticky());
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
  _updateLayout();
}

  void
Manager::_updateLayout()
{
  titleBar_       ->  setGeometry(
    0,
    0,
    width(),
    Static::instance()->titleHeight()
  );

  windowWrapper() ->  setGeometry(
    1,
    Static::instance()->titleHeight(),
    width() - 2,
    height() - Static::instance()->titleHeight() - RESIZE_BAR_HEIGHT
  );

  resizeBar_      ->  setGeometry(
    0,
    height() - RESIZE_BAR_HEIGHT,
    width(),
    RESIZE_BAR_HEIGHT
  );

  _updateDisplay();
}

  void
Manager::activateLayout()
{
  _updateLayout();
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
