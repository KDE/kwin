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
#include "../../workspace.h"

#include "TitleText.h"
#include "Manager.h"
#include "Static.h"

namespace RiscOS
{

TitleText::TitleText(QWidget * parent, Manager * client)
  : DBWidget(parent, "TitleText"),
    client_(client)
{
  setFixedHeight(20);
}

TitleText::~TitleText()
{
}

  void
TitleText::updatePixmap()
{
  QPainter p(&buf());

  p.drawPixmap(0, 0, Static::instance()->titleTextLeft(client_->isActive()));
  p.drawPixmap(width() - 3, 0, Static::instance()->titleTextRight(client_->isActive()));
  p.drawTiledPixmap(3, 0, width() - 6, 20, Static::instance()->titleTextMid(client_->isActive()));

  p.setPen(options->color(Options::Font, client_->isActive()));
  p.setFont(options->font());
  p.drawText(4, 0, width() - 8, 18, AlignCenter, client_->caption());
}

  void
TitleText::mousePressEvent(QMouseEvent * e)
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
TitleText::mouseReleaseEvent(QMouseEvent *)
{
  // Anything to do ?
}

  void
TitleText::mouseMoveEvent(QMouseEvent * e)
{
  // Need to be a little clever here.

  QPoint adjustedForCursor = e->globalPos() - clientPosToMousePos_;

  QPoint adjustedForSnap =
    client_->workspace()->adjustClientPosition(client_, adjustedForCursor);

  client_->move(adjustedForSnap);
}

  void
TitleText::mouseDoubleClickEvent(QMouseEvent *)
{
  client_->setShade(!client_->isShade());
}

} // End namespace

// vim:ts=2:sw=2:tw=78

