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

#include <qlayout.h>

#include "Manager.h"
#include "TitleBar.h"
#include "TitleText.h"
#include "CloseButton.h"
#include "IconifyButton.h"
#include "LowerButton.h"
#include "MaximiseButton.h"

namespace RiscOS
{

TitleBar::TitleBar(Manager * client)
  : QWidget(client)
{
  setBackgroundMode(NoBackground);

  lower_      = new LowerButton     (this);
  close_      = new CloseButton     (this);
  text_       = new TitleText       (this, client);
  iconify_    = new IconifyButton   (this);
  maximise_   = new MaximiseButton  (this);

  lower_    ->setAlign(Button::Left);
  close_    ->setAlign(Button::Left);
  iconify_  ->setAlign(Button::Right);
  maximise_ ->setAlign(Button::Right);

  // Lower | Close | Text | Iconify | Maximise

  QHBoxLayout * layout   = new QHBoxLayout(this);

  layout->addWidget(lower_);
  layout->addWidget(close_);
  layout->addWidget(text_, 1);
  layout->addWidget(iconify_);
  layout->addWidget(maximise_);

  connect(lower_,     SIGNAL(lowerClient()),          client,     SLOT(lower()));
  connect(close_,     SIGNAL(closeClient()),          client,     SLOT(closeWindow()));
  connect(iconify_,   SIGNAL(iconifyClient()),        client,     SLOT(iconify()));
  connect(maximise_,  SIGNAL(maximiseClient()),       client,     SLOT(maximize()));
  connect(maximise_,  SIGNAL(vMaxClient()),           client,     SLOT(vMax()));
  connect(maximise_,  SIGNAL(raiseClient()),          client,     SLOT(raise()));
  connect(client,     SIGNAL(maximiseChanged(bool)),  maximise_,  SLOT(setOn(bool)));
}
 
  void
TitleBar::updateDisplay()
{
  lower_    ->updateDisplay();
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

  if (width() < 80) sizeProblem = 3;
  else if (width() < 100) sizeProblem = 2;
  else if (width() < 120) sizeProblem = 1;

  switch (sizeProblem) {

    case 1:
      lower_    ->hide();
      iconify_  ->show();
      maximise_ ->hide();
      close_    ->show();
      break;

    case 2:
      lower_    ->hide();
      iconify_  ->hide();
      maximise_ ->hide();
      close_    ->show();
      break;

    case 3:
      lower_    ->hide();
      iconify_  ->hide();
      maximise_ ->hide();
      close_    ->hide();
      break;

    case 0:
    default:
      lower_    ->show();
      iconify_  ->show();
      maximise_ ->show();
      close_    ->show();
      break;
  }
}

  void
TitleBar::setActive(bool b)
{
  lower_->setActive(b);
  close_->setActive(b);
  text_->setActive(b);
  iconify_->setActive(b);
  maximise_->setActive(b);
}

} // End namespace

// vim:ts=2:sw=2:tw=78
