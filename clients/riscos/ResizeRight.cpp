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

#include <qpixmap.h>

#include "ResizeRight.h"
#include "Manager.h"
#include "Static.h"

namespace RiscOS
{

ResizeRight::ResizeRight(QWidget * parent, Manager * client)
  : QWidget(parent, "ResizeRight"),
    client_(client)
{
  setCursor(Qt::sizeFDiagCursor);

  setFixedSize(30, 10);

  update();
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

  void
ResizeRight::update()
{
  setBackgroundPixmap(Static::instance()->resize(client_->isActive()));
}

} // End namespace

// vim:ts=2:sw=2:tw=78
