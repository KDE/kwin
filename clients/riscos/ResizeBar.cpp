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

#include "ResizeBar.h"
#include "ResizeMid.h"
#include "ResizeSide.h"

#include <X11/Xlib.h>

namespace Default
{

ResizeBar::ResizeBar(QWidget * parent, Manager * client)
  : QWidget (parent, "ResizeBar"),
    client_ (client)
{
  setBackgroundMode(NoBackground);

  left_   = new ResizeSide(this, client_, ResizeSide::Left);
  mid_    = new ResizeMid(this, client_);
  right_  = new ResizeSide(this, client_, ResizeSide::Right);
  
  mid_->move(30, 0);
}
  
  void
ResizeBar::updateDisplay()
{
  left_ ->updateDisplay();
  mid_  ->updateDisplay();
  right_->updateDisplay();
}

  void
ResizeBar::resizeEvent(QResizeEvent *)
{
  mid_->resize(width() - 60, mid_->height());
  right_->move(width() - 30, 0);
}

} // End namespace

// vim:ts=2:sw=2:tw=78
#include "ResizeBar.moc"
