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

#include "DBWidget.h"

namespace Default
{

DBWidget::DBWidget(QWidget * parent, const char * name)
  : QWidget(parent, name, WResizeNoErase | WRepaintNoErase | WPaintUnclipped |
WNorthWestGravity)
{
  buf_.resize(20, 20);
  setBackgroundMode(NoBackground);
}

  void
DBWidget::updateDisplay()
{
  updatePixmap();
  repaint(false);
}

  void
DBWidget::paintEvent(QPaintEvent * e)
{
  QRect r(e->rect());
  bitBlt(this, r.topLeft(), &buf_, r, Qt::CopyROP);
}

  void
DBWidget::resizeEvent(QResizeEvent * e)
{
  QWidget::resizeEvent(e);

  if (  (buf_.width() < size().width()) ||
        (QABS(size().width() - buf_.width()) > 128) )
    buf_.resize(((size().width() / 128)* 128) + 128, size().height());

  updateDisplay();
}

} // End namespace

// vim:ts=2:sw=2:tw=78
