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

#include "ResizeMid.h"
#include "Manager.h"
#include "Static.h"
#include "Utils.h"

namespace RiscOS
{

ResizeMid::ResizeMid(QWidget * parent, Manager * client)
  : DBWidget(parent, "ResizeMid"),
    client_(client)
{
  setFixedHeight(10);
  setCursor(Qt::sizeVerCursor);
}

ResizeMid::~ResizeMid()
{
  // Empty.
}

   void
ResizeMid::updatePixmap()
{
  QPainter p(&buf());

  p.drawPixmap(0, 0, Static::instance()->resizeMidLeft(client_->isActive()));
  p.drawPixmap(width() - 2, 0, Static::instance()->resizeMidRight(client_->isActive()));
  p.drawTiledPixmap(2, 0, width() - 4, 10, Static::instance()->resizeMidMid(client_->isActive()));
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

  void
ResizeMid::mousePressEvent(QMouseEvent * e)
{
  if (e->button() == LeftButton)
    client_->raise();
}

} // End namespace

// vim:ts=2:sw=2:tw=78
