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
#include <kstyle.h>
#include <kapp.h>

#include "ResizeMid.h"
#include "Manager.h"
#include "Static.h"

namespace Default
{

ResizeMid::ResizeMid(QWidget * parent, Manager * client)
  : DBWidget(parent, "ResizeMid"),
    client_(client)
{
  setFixedHeight(RESIZE_BAR_HEIGHT);
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
