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
#include <qimage.h>
#include <qlayout.h>
#include "../../options.h"
#include "../../workspace.h"

#include "Manager.h"
#include "Static.h"
#include "LowerButton.h"
#include "CloseButton.h"
#include "IconifyButton.h"
#include "MaximiseButton.h"
#include "StickyButton.h"
#include "HelpButton.h"

extern "C"
{
  Client * allocate(Workspace * workSpace, WId winId, int)
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
  setBackgroundMode(NoBackground);

  connect(options, SIGNAL(resetClients()), this, SLOT(slotReset()));

  QVBoxLayout * l = new QVBoxLayout(this, 0, 0);

  lower_      = new LowerButton     (this);
  close_      = new CloseButton     (this);
  sticky_     = new StickyButton    (this);
  iconify_    = new IconifyButton   (this);
  maximise_   = new MaximiseButton  (this);
  help_       = new HelpButton      (this);

  if (!providesContextHelp())
    help_->hide();

  lower_    ->setAlignment(Button::Left);
  close_    ->setAlignment(Button::Left);
  sticky_   ->setAlignment(Button::Left);
  help_     ->setAlignment(Button::Right);
  iconify_  ->setAlignment(Button::Right);
  maximise_ ->setAlignment(Button::Right);

  // Lower | Close | Text | Iconify | Maximise

  QHBoxLayout * titleLayout = new QHBoxLayout(l);

  titleLayout->addWidget(lower_);
  titleLayout->addWidget(close_);
  titleLayout->addWidget(sticky_);
  titleSpacer_ = new QSpacerItem(0, 20);
  titleLayout->addItem(titleSpacer_);
  titleLayout->addWidget(help_);
  titleLayout->addWidget(iconify_);
  titleLayout->addWidget(maximise_);

  QHBoxLayout * midLayout = new QHBoxLayout(l);
  midLayout->addSpacing(1);
  midLayout->addWidget(windowWrapper());
  midLayout->addSpacing(1);

  l->addSpacing(10);

  connect(lower_,     SIGNAL(lowerClient()),          SLOT(lower()));
  connect(close_,     SIGNAL(closeClient()),          SLOT(closeWindow()));
  connect(iconify_,   SIGNAL(iconifyClient()),        SLOT(iconify()));
  connect(sticky_,    SIGNAL(stickClient()),          SLOT(stick()));
  connect(sticky_,    SIGNAL(unstickClient()),        SLOT(unstick()));
  connect(maximise_,  SIGNAL(maximiseClient()),       SLOT(maximize()));
  connect(maximise_,  SIGNAL(vMaxClient()),           SLOT(vMax()));
  connect(maximise_,  SIGNAL(raiseClient()),          SLOT(raise()));
  connect(help_,      SIGNAL(help()),                 SLOT(help()));

  connect(
      this,       SIGNAL(maximiseChanged(bool)),
      maximise_,  SLOT(setOn(bool)));

  connect(
      this,       SIGNAL(stickyChanged(bool)),
      sticky_,    SLOT(setOn(bool)));
}

Manager::~Manager()
{
}

  void
Manager::slotReset()
{
  Static::instance()->update();
  repaint();
}

  void
Manager::captionChange(const QString &)
{
  repaint();
}

  void
Manager::paletteChange(const QPalette &)
{
  Static::instance()->update();
  repaint();
}

  void
Manager::activeChange(bool b)
{
  lower_      ->setActive(b);
  close_      ->setActive(b);
  sticky_     ->setActive(b);
  iconify_    ->setActive(b);
  maximise_   ->setActive(b);
  help_       ->setActive(b);

  repaint();
}

  void
Manager::maximizeChange(bool b)
{
  emit(maximiseChanged(b));
}

  void
Manager::stickyChange(bool b)
{
  emit(stickyChanged(b));
}

  void
Manager::paintEvent(QPaintEvent * e)
{
  QPainter p(this);

  QRect r(e->rect());

  bool intersectsLeft =
    r.intersects(QRect(0, 0, 1, height()));

  bool intersectsRight =
    r.intersects(QRect(width() - 1, 0, width(), height()));

  if (intersectsLeft || intersectsRight) {

    p.setPen(Qt::black);

    if (intersectsLeft)
      p.drawLine(0, r.top(), 0, r.bottom());

    if (intersectsRight)
      p.drawLine(width() - 1, r.top(), width() - 1, r.bottom());
  }

  Static * s = Static::instance();

  bool active = isActive();

  QRect tr = titleSpacer_->geometry();

  // Title bar.
  p.drawPixmap(tr.left(), 0, s->titleTextLeft(active));

  p.drawTiledPixmap(tr.left() + 3, 0, tr.width() - 6, 20, s->titleTextMid(active));
  p.setPen(options->color(Options::Font, active));
  p.setFont(options->font(active));
  p.drawText(tr.left() + 4, 0, tr.width() - 8, 18, AlignCenter, caption());

  p.drawPixmap(tr.right() - 2, 0, s->titleTextRight(active));

  // Resize bar.

  int rbt = height() - 10; // Resize bar top.

  p.drawPixmap(0, rbt, s->resize(active));

  p.drawPixmap(30, rbt, s->resizeMidLeft(active));
  p.drawTiledPixmap(32, rbt, width() - 34, 10, s->resizeMidMid(active));
  p.drawPixmap(width() - 32, rbt, s->resizeMidRight(active));

  p.drawPixmap(width() - 30, rbt, s->resize(active));
}

  void
Manager::lower()
{
  workspace()->lowerClient(this);
}

  void
Manager::raise()
{
  workspace()->raiseClient(this);
}

  void
Manager::vMax()
{
  maximize(MaximizeVertical);
}

  void
Manager::stick()
{
  setSticky(true);
}

  void
Manager::unstick()
{
  setSticky(false);
}

  void
Manager::help()
{
  contextHelp();
}

  void
Manager::resizeEvent(QResizeEvent *)
{
  int sizeProblem = 0;

  if (width() < 80) sizeProblem = 3;
  else if (width() < 100) sizeProblem = 2;
  else if (width() < 120) sizeProblem = 1;

  switch (sizeProblem) {

    case 1:
      lower_    ->hide();
      sticky_   ->hide();
      help_     ->hide();
      iconify_  ->show();
      maximise_ ->hide();
      close_    ->show();
      break;

    case 2:
      lower_    ->hide();
      sticky_   ->hide();
      help_     ->hide();
      iconify_  ->hide();
      maximise_ ->hide();
      close_    ->show();
      break;

    case 3:
      lower_    ->hide();
      sticky_   ->hide();
      help_     ->hide();
      iconify_  ->hide();
      maximise_ ->hide();
      close_    ->hide();
      break;

    case 0:
    default:
      lower_    ->show();
      sticky_   ->show();
      if (providesContextHelp())
        help_->show();
      iconify_  ->show();
      maximise_ ->show();
      close_    ->show();
      break;
  }
}

  Client::MousePosition
Manager::mousePosition(const QPoint & p) const
{
  MousePosition m = Center;

  if (p.y() > (height() - 10)) {
     // Keep order !
    if (p.x() >= (width() - 30))
      m = BottomRight;
    else if (p.x() <= 30)
      m = BottomLeft;
    else
      m = Bottom;
  }

  return m;
}

  void
Manager::mouseDoubleClickEvent(QMouseEvent * e)
{
  if (titleSpacer_->geometry().contains(e->pos()))
    workspace()
      ->performWindowOperation(this, options->operationTitlebarDblClick());
  workspace()->requestFocus(this);
}

} // End namespace

// vim:ts=2:sw=2:tw=78
#include "Manager.moc"
