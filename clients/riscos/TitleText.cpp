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

#include <qpainter.h>

#include "../../options.h"

#include "TitleText.h"
#include "Manager.h"
#include "Static.h"

namespace RiscOS
{

TitleText::TitleText(QWidget * parent, Manager * client)
  : DBWidget(parent, "TitleText"),
    client_(client),
    active_(false)
{
}

TitleText::~TitleText()
{
}

  void
TitleText::setActive(bool b)
{
  active_ = b;
  updateDisplay();
}

  void
TitleText::updatePixmap()
{
  QPainter p(&buf());

  Static * s = Static::instance();

  p.drawPixmap(0, 0, s->titleTextLeft(active_));
  p.drawPixmap(width() - 3, 0, s->titleTextRight(active_));
  p.drawTiledPixmap(3, 0, width() - 6, 20, s->titleTextMid(active_));
  p.setPen(options->color(Options::Font, active_));
  p.setFont(options->font());
  p.drawText(4, 0, width() - 8, 18, AlignCenter, client_->caption());
}

  void
TitleText::mousePressEvent(QMouseEvent * e)
{
  client_->fakeMouseEvent(e, this);
}

  void
TitleText::mouseReleaseEvent(QMouseEvent * e)
{
  client_->fakeMouseEvent(e, this);
}

  void
TitleText::mouseMoveEvent(QMouseEvent * e)
{
  client_->fakeMouseEvent(e, this);
}

  void
TitleText::mouseDoubleClickEvent(QMouseEvent * e)
{
  client_->fakeMouseEvent(e, this);
}

} // End namespace

// vim:ts=2:sw=2:tw=78

