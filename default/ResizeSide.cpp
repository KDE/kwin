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

#include <qpixmap.h>
#include <kstyle.h>
#include <kapp.h>

#include "ResizeSide.h"
#include "Manager.h"
#include "Static.h"

namespace Default
{

ResizeSide::ResizeSide(QWidget * parent, Manager * client, Side s)
  : QWidget (parent, "ResizeSide"),
    client_ (client),
    side_   (s)
{
  setCursor(side_ == Left ? Qt::sizeBDiagCursor : Qt::sizeFDiagCursor);
  setFixedSize(RESIZE_SIDE_WIDTH, RESIZE_BAR_HEIGHT);
  updateDisplay();
}

  void
ResizeSide::mouseMoveEvent(QMouseEvent * e)
{
  QRect g = client_->geometry();
  g.setBottom(e->globalPos().y());
  if (side_ == Left)
    g.setLeft(e->globalPos().x());
  else
    g.setRight(e->globalPos().x());

	QSize adjustedSize = client_->adjustedSize(g.size());

  if (adjustedSize != client_->size()) {

    if (side_ == Left)
      g.setLeft(g.right() - adjustedSize.width());
    else
      g.setRight(g.left() + adjustedSize.width());

    g.setBottom(g.top() + adjustedSize.height());

    client_->setGeometry(g);
  }
}

  void
ResizeSide::updateDisplay()
{
  QPixmap pix(size());
  QPainter p(&pix);

  QColorGroup g(
      client_->isActive() ?
      palette().active() :
      palette().inactive()
  );

  QBrush b(g.button());

  QStyle * style = kapp->kstyle();

  if (0 != style)
    style->drawPanel(&p, 0, 0, width(), height(), g, false, 2, &b);
  else
    kapp->style().drawPanel(&p, 0, 0, width(), height(), g, false, 2, &b);

  setBackgroundPixmap(pix);
}

  void
ResizeSide::mousePressEvent(QMouseEvent * e)
{
  if (e->button() == LeftButton)
    client_->raise();
}

} // End namespace

// vim:ts=2:sw=2:tw=78
