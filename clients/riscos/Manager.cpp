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
#include <qlayout.h>

#include "../../options.h"
#include "../../workspace.h"

#include "Manager.h"
#include "Static.h"
#include "TitleBar.h"
#include "ResizeBar.h"

extern "C"
{
  Client * allocate(Workspace * workSpace, WId winId)
  {
    return new RiscOS::Manager(workSpace, winId);
  }
}

namespace RiscOS
{

Manager::Manager(
  Workspace * workSpace,
  WId id,
  QWidget * parent,
  const char * name
)
  : Client(workSpace, id, parent, name)
{
  Static::instance();

  setBackgroundMode(NoBackground);

  connect(options, SIGNAL(resetClients()), this, SLOT(slotReset()));

  titleBar_   = new TitleBar(this, this);
  resizeBar_  = new ResizeBar(this, this);

  // Border Window Border
  QHBoxLayout * windowLayout  = new QHBoxLayout(0, "windowLayout");
  windowLayout->addSpacing(1);
  windowLayout->addWidget(windowWrapper(), 1);
  windowLayout->addSpacing(1);
  
  // Titlebar (has own single pixel border)
  // Window
  // Resize bar (has own single pixel border)
  QVBoxLayout * mainLayout = new QVBoxLayout(this, 0, 0, "mainLayout");
  mainLayout->addWidget(titleBar_);
  mainLayout->addLayout(windowLayout, 1);
  mainLayout->addWidget(resizeBar_);

  updateDisplay();
}

Manager::~Manager()
{
}

  void
Manager::slotReset()
{
  Static::instance()->update();
  titleBar_->updateDisplay();
  resizeBar_->updateDisplay();
}
    
  void
Manager::captionChange(const QString &)
{
  titleBar_->updateText();
}

  void
Manager::paletteChange(const QPalette &)
{
  Static::instance()->update(); 
  titleBar_->updateDisplay();
}

  void
Manager::activeChange(bool)
{
  titleBar_->updateDisplay();
  resizeBar_->updateDisplay();
}

  void
Manager::maximizeChange(bool b)
{
  titleBar_->updateMaximise(b);
}

  void
Manager::maximizeAndRaise()
{
  maximize(MaximizeFull);
  workspace()->raiseClient(this);
  workspace()->requestFocus(this);
}

  void
Manager::maximizeVertically()
{
  maximize(MaximizeVertical);
  workspace()->raiseClient(this);
  workspace()->requestFocus(this);
}

  void
Manager::maximizeNoRaise()
{
  maximize(MaximizeFull);
}

  void
Manager::resize(int w, int h)
{
  Client::resize(w, h);
}

  void
Manager::updateDisplay()
{
  titleBar_->updateDisplay();
  resizeBar_->updateDisplay();
}

  void
Manager::setShade(bool)
{
#if 0
  // Hmm. This does screwy things to the layout.
  if (b)
    resizeBar_->hide();
  else
    resizeBar_->show();
#endif

  // And this is screwed. My window ends up the wrong size when unshaded.
//  Client::setShade(b);
}

  void
Manager::paintEvent(QPaintEvent * e)
{
  QRect r(e->rect());

  bool intersectsLeft =
    r.intersects(QRect(0, 0, 1, height()));

  bool intersectsRight =
    r.intersects(QRect(width() - 1, 0, width(), height()));

  if (intersectsLeft || intersectsRight) {

    QPainter p(this);
// ???    p.setPen(options->color(Options::Frame, isActive()));
    p.setPen(Qt::black);

    if (intersectsLeft)
      p.drawLine(0, r.top(), 0, r.bottom());
  
    if (intersectsRight)
      p.drawLine(width() - 1, r.top(), width() - 1, r.bottom());
  }
}

} // End namespace

// vim:ts=2:sw=2:tw=78
