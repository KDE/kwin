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

#include <qlayout.h>

#include "Manager.h"
#include "TitleBar.h"
#include "TitleText.h"
#include "CloseButton.h"
#include "IconifyButton.h"
#include "StickyButton.h"
#include "MaximiseButton.h"

namespace Default
{

TitleBar::TitleBar(Manager * client)
  : QWidget(client)
{
  close_      = new CloseButton     (this);
  text_       = new TitleText       (this, client);
  iconify_    = new IconifyButton   (this);
  maximise_   = new MaximiseButton  (this);
  sticky_     = new StickyButton    (this);

  // Close | Text | Sticky | Iconify | Maximise

  QHBoxLayout * layout = new QHBoxLayout(this);
  layout->setMargin(3);

  layout->addWidget(close_);
  layout->addSpacing(2);
  layout->addWidget(text_, 1);
  layout->addSpacing(2);
  layout->addWidget(sticky_);
  layout->addWidget(iconify_);
  layout->addWidget(maximise_);

  connect(
    close_,     SIGNAL(closeClient()),
    client,     SLOT(closeWindow())
  );

  connect(
    sticky_,    SIGNAL(toggleSticky()),
    client,     SLOT(toggleSticky())
  );

  connect(
    client,     SIGNAL(stickyStatusChanged(bool)),
    sticky_,    SLOT(setOn(bool))
  );

  connect(
    iconify_,   SIGNAL(iconifyClient()),
    client,     SLOT(iconify())
  );

  connect(
    maximise_,  SIGNAL(maximiseClient()),
    client,     SLOT(maximize())
  );

  connect(
    maximise_,  SIGNAL(vMaxClient()),
    client,     SLOT(vMax())
  );

  connect(
    maximise_,  SIGNAL(raiseClient()),
    client,     SLOT(raise())
  );

  connect(
    client,     SIGNAL(maximiseChanged(bool)),
    maximise_,  SLOT(setOn(bool))
  );
}
 
  void
TitleBar::updateDisplay()
{
  close_    ->updateDisplay();
  text_     ->updateDisplay();
  iconify_  ->updateDisplay();
  maximise_ ->updateDisplay();
}
  
  void
TitleBar::updateText()
{
  text_->updateDisplay();
}

TitleBar::~TitleBar()
{
}

  void
TitleBar::resizeEvent(QResizeEvent *)
{
  int sizeProblem = 0;

  if (width() < 120) sizeProblem = 3;
  else if (width() < 160) sizeProblem = 2;
  else if (width() < 200) sizeProblem = 1;

  switch (sizeProblem) {

    case 1:
      close_    ->show();
      sticky_   ->hide();
      iconify_  ->show();
      maximise_ ->hide();
      break;

    case 2:
      close_    ->show();
      sticky_   ->hide();
      iconify_  ->hide();
      maximise_ ->hide();
      break;

    case 3:
      close_    ->hide();
      sticky_   ->hide();
      iconify_  ->hide();
      maximise_ ->hide();
      break;

    case 0:
    default:
      close_    ->show();
      sticky_    ->show();
      iconify_  ->show();
      maximise_ ->show();
      break;
  }
}

  void
TitleBar::setActive(bool b)
{
  sticky_->setActive(b);
  close_->setActive(b);
  text_->setActive(b);
  iconify_->setActive(b);
  maximise_->setActive(b);
}

  void
TitleBar::paintEvent(QPaintEvent * e)
{
  QRect r(e->rect());

  bool intersectsLeft =
    r.intersects(QRect(0, 0, 1, height()));

  bool intersectsRight =
    r.intersects(QRect(width() - 1, 0, width(), height()));

  bool intersectsTop =
    r.intersects(QRect(0, 0, width(), 1));

  if (intersectsTop || intersectsLeft || intersectsRight) {

    QPainter p(this);
    p.setPen(Qt::black);

    if (intersectsTop)
      p.drawLine(r.left(), 0, r.right(), 0);

    if (intersectsLeft)
      p.drawLine(0, r.top(), 0, r.bottom());

    if (intersectsRight)
      p.drawLine(width() - 1, r.top(), width() - 1, r.bottom());
  }
}

} // End namespace

// vim:ts=2:sw=2:tw=78
