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

#include <unistd.h> // for usleep
#include <config.h> // for usleep on non-linux platforms
#include <math.h> // for sin and cos

#include <qpainter.h>
#include <qimage.h>
#include <qlayout.h>

#include <kapp.h>
#include <netwm.h>

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
  Client * allocate(Workspace * workSpace, WId winId, int tool)
  {
    if (tool)
      return new RiscOS::ToolManager(workSpace, winId);
    else
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

  QStringList leftButtons = Static::instance()->leftButtons();
  QStringList rightButtons = Static::instance()->rightButtons();

  QVBoxLayout * l = new QVBoxLayout(this, 0, 0);
  l->setResizeMode(QLayout::FreeResize);

  lower_      = new LowerButton     (this);
  close_      = new CloseButton     (this);
  sticky_     = new StickyButton    (this);
  iconify_    = new IconifyButton   (this);
  maximise_   = new MaximiseButton  (this);
  help_       = new HelpButton      (this);

  buttonDict_.insert("Lower",    lower_);
  buttonDict_.insert("Close",    close_);
  buttonDict_.insert("Sticky",   sticky_);
  buttonDict_.insert("Iconify",  iconify_);
  buttonDict_.insert("Maximize", maximise_);
  buttonDict_.insert("Help",     help_);

  if (!providesContextHelp())
    help_->hide();

  QStringList::ConstIterator it;

  for (it = leftButtons.begin(); it != leftButtons.end(); ++it)
    if (buttonDict_[*it])
      buttonDict_[*it]->setAlignment(Button::Left);

  for (it = rightButtons.begin(); it != rightButtons.end(); ++it)
    if (buttonDict_[*it])
      buttonDict_[*it]->setAlignment(Button::Right);

  QHBoxLayout * titleLayout = new QHBoxLayout(l);
  titleLayout->setResizeMode(QLayout::FreeResize);

  for (it = leftButtons.begin(); it != leftButtons.end(); ++it)
    if (buttonDict_[*it])
      titleLayout->addWidget(buttonDict_[*it]);

  titleSpacer_ =
    new QSpacerItem(
      0,
      Static::instance()->titleHeight(),
      QSizePolicy::Expanding,
      QSizePolicy::Fixed
    );

  titleLayout->addItem(titleSpacer_);

  for (it = rightButtons.begin(); it != rightButtons.end(); ++it)
    if (buttonDict_[*it])
      titleLayout->addWidget(buttonDict_[*it]);

  QHBoxLayout * midLayout = new QHBoxLayout(l);
  midLayout->setResizeMode(QLayout::FreeResize);
  midLayout->addSpacing(1);
  midLayout->addWidget(windowWrapper());
  midLayout->addSpacing(1);

  l->addSpacing(Static::instance()->resizeHeight());

  connect(options, SIGNAL(resetClients()), this, SLOT(slotReset()));
}

Manager::~Manager()
{
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

  // Title bar.

  QRect tr = titleSpacer_->geometry();
  bitBlt(this, tr.topLeft(), &titleBuf_);

  // Resize bar.

  int rbt = height() - Static::instance()->resizeHeight(); // Resize bar top.

  bitBlt(this, 0, rbt, &(s->resize(active)));
  bitBlt(this, 30, rbt, &(s->resizeMidLeft(active)));

  p.drawTiledPixmap(
      32,
      rbt,
      width() - 34,
      Static::instance()->resizeHeight(),
      s->resizeMidMid(active)
  );

  bitBlt(this, width() - 32, rbt, &(s->resizeMidRight(active)));
  bitBlt(this, width() - 30, rbt, &(s->resize(active)));
}

  void
Manager::resizeEvent(QResizeEvent * e)
{
  Client::resizeEvent(e);
  updateButtonVisibility();
  updateTitleBuffer();
  repaint();
}

  void
Manager::updateButtonVisibility()
{
  int sizeProblem = 0;

  if (width() < 80) sizeProblem = 3;
  else if (width() < 100) sizeProblem = 2;
  else if (width() < 180) sizeProblem = 1;

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

  layout()->activate();
}

  void
Manager::updateTitleBuffer()
{
  bool active = isActive();

  Static * s = Static::instance();

  QRect tr = titleSpacer_->geometry();

  if (tr.width() == 0 || tr.height() == 0)
    titleBuf_.resize(8, 8);
  else
    titleBuf_.resize(tr.size());

  QPainter p(&titleBuf_);

  p.drawPixmap(0, 0, s->titleTextLeft(active));

  p.drawTiledPixmap(
      3,
      0,
      tr.width() - 6,
      Static::instance()->titleHeight(),
      s->titleTextMid(active)
    );

  p.setPen(options->color(Options::Font, active));

  p.setFont(options->font(true)); // XXX false doesn't work right at the moment

  p.drawText(
      4,
      2,
      tr.width() - 8,
      Static::instance()->titleHeight() - 4,
      AlignCenter, caption()
  );

  p.drawPixmap(tr.width() - 3, 0, s->titleTextRight(active));
}

  Client::MousePosition
Manager::mousePosition(const QPoint & p) const
{
  MousePosition m = Center;

  if (p.y() > (height() - Static::instance()->resizeHeight())) {
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

  void
Manager::slotReset()
{
  for (QDictIterator<Button> it(buttonDict_); it.current(); ++it)
    it.current()->update();
  Static::instance()->update();
  repaint();
}

  void
Manager::captionChange(const QString &)
{
  updateTitleBuffer();
  repaint();
}

  void
Manager::paletteChange(const QPalette &)
{
  slotReset();
}

  void
Manager::activeChange(bool b)
{
  emit(activeChanged(b));
  updateTitleBuffer();
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
Manager::slotLower()
{
  workspace()->lowerClient(this);
}

  void
Manager::slotRaise()
{
  workspace()->raiseClient(this);
}

  void
Manager::slotVMax()
{
  maximize(MaximizeVertical);
}

  void
Manager::slotSetSticky(bool b)
{
  setSticky(b);
}

  void
Manager::slotHelp()
{
  contextHelp();
}

  void
Manager::animateIconifyOrDeiconify(bool iconify)
{
  animate(iconify, Static::instance()->animationStyle());
}

void Manager::animate(bool iconify, int style)
{
  switch (style) {

    case 1:
      {
        // Double twisting double back, with pike ;)

        if (!iconify) // No animation for restore.
          return;

        // Go away quick.
        hide();
        qApp->syncX();

        NETRect r = netWinInfo()->iconGeometry();

        if (!QRect(r.pos.x, r.pos.y, r.size.width, r.size.height).isValid())
          return;

        // Algorithm taken from Window Maker (http://www.windowmaker.org)

        int sx = x();
        int sy = y();
        int sw = width();
        int sh = height();
        int dx = r.pos.x;
        int dy = r.pos.y;
        int dw = r.size.width;
        int dh = r.size.height;

        double steps = 12;

        double xstep = double((dx-sx)/steps);
        double ystep = double((dy-sy)/steps);
        double wstep = double((dw-sw)/steps);
        double hstep = double((dh-sh)/steps);

        double cx = sx;
        double cy = sy;
        double cw = sw;
        double ch = sh;

        double finalAngle = 3.14159265358979323846;

        double delta  = finalAngle / steps;

        QPainter p(workspace()->desktopWidget());
        p.setRasterOp(Qt::NotROP);

        for (double angle = 0; ; angle += delta) {

          if (angle > finalAngle)
            angle = finalAngle;

          double dx = (cw / 10) - ((cw / 5) * sin(angle));
          double dch = (ch / 2) * cos(angle);
          double midy = cy + (ch / 2);

          QPoint p1(cx + dx, midy - dch);
          QPoint p2(cx + cw - dx, p1.y());
          QPoint p3(cx + dw + dx, midy + dch);
          QPoint p4(cx - dx, p3.y());
       
          XGrabServer(qt_xdisplay());

          p.drawLine(p1, p2);
          p.drawLine(p2, p3);
          p.drawLine(p3, p4);
          p.drawLine(p4, p1);

          p.flush();

          usleep(500);

          p.drawLine(p1, p2);
          p.drawLine(p2, p3);
          p.drawLine(p3, p4);
          p.drawLine(p4, p1);

          XUngrabServer(qt_xdisplay());

          kapp->processEvents();

          cx += xstep;
          cy += ystep;
          cw += wstep;
          ch += hstep;

          if (angle >= finalAngle)
            break;
        }
      }
      break;

    case 2:
      {
        // KVirc style ? Maybe. For qwertz.

        if (!iconify) // No animation for restore.
          return;

        // Go away quick.
        hide();
        qApp->syncX();

        int stepCount = 12;

        QRect r(geometry());

        int dx = r.width() / (stepCount * 2);
        int dy = r.height() / (stepCount * 2);

        QPainter p(workspace()->desktopWidget());
        p.setRasterOp(Qt::NotROP);

        for (int step = 0; step < stepCount; step++) {

          r.moveBy(dx, dy);
          r.setWidth(r.width() - 2 * dx);
          r.setHeight(r.height() - 2 * dy);

          XGrabServer(qt_xdisplay());

          p.drawRect(r);
          p.flush();
          usleep(200);
          p.drawRect(r);

          XUngrabServer(qt_xdisplay());

          kapp->processEvents();
        }
      }
      break;


    default:
      {
        NETRect r = netWinInfo()->iconGeometry();

        QRect icongeom(r.pos.x, r.pos.y, r.size.width, r.size.height);

        if (!icongeom.isValid())
          return;

        QRect wingeom(x(), y(), width(), height());

        QPainter p(workspace()->desktopWidget());

        p.setRasterOp(Qt::NotROP);

        if (iconify)
          p.setClipRegion(
              QRegion(workspace()->desktopWidget()->rect()) - wingeom
          );

        XGrabServer(qt_xdisplay());

        p.drawLine(wingeom.bottomRight(), icongeom.bottomRight());
        p.drawLine(wingeom.bottomLeft(), icongeom.bottomLeft());
        p.drawLine(wingeom.topLeft(), icongeom.topLeft());
        p.drawLine(wingeom.topRight(), icongeom.topRight());

        p.flush();

        qApp->syncX();

        usleep(30000);

        p.drawLine(wingeom.bottomRight(), icongeom.bottomRight());
        p.drawLine(wingeom.bottomLeft(), icongeom.bottomLeft());
        p.drawLine(wingeom.topLeft(), icongeom.topLeft());
        p.drawLine(wingeom.topRight(), icongeom.topRight());

        XUngrabServer(qt_xdisplay());
      }
      break;
  }
}


ToolManager::ToolManager(
  Workspace * workSpace,
  WId id,
  QWidget * parent,
  const char * name
)
  : Manager(workSpace, id, parent, name)
{
}

ToolManager::~ToolManager()
{
}

} // End namespace

// vim:ts=2:sw=2:tw=78
#include "Manager.moc"
