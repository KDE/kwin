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

#include "../../options.h"
#include "Button.h"
#include "Static.h"

using namespace KWinInternal;

namespace RiscOS
{

Button::Button(QWidget * parent, const QString& tip)
  : KWinWidgetButton   (parent, "Button", 0, tip),
    alignment_(Left),
    down_     (false),
    active_   (false)
{
  setBackgroundColor(Qt::black);

  setFixedSize
    (
     Static::instance()->titleHeight() - 1,
     Static::instance()->titleHeight()
    );

  connect(parent, SIGNAL(activeChanged(bool)), this, SLOT(setActive(bool)));
}

Button::~Button()
{
  // Empty.
}

  void
Button::setAlignment(Alignment a)
{
  alignment_ = a;
  repaint();
}

  void
Button::setActive(bool b)
{
  active_ = b;
  repaint();
}

  Button::Alignment
Button::alignment() const
{
  return alignment_;
}

  void
Button::mousePressEvent(QMouseEvent *e)
{
  down_ = true;
  repaint();
  KWinWidgetButton::mousePressEvent(e);
}

  void
Button::mouseReleaseEvent(QMouseEvent *e)
{
  down_ = false;
  repaint();
  KWinWidgetButton::mouseReleaseEvent(e);
}

  void
Button::setPixmap(const QPixmap & p)
{
  if (QPixmap::defaultDepth() <= 8)
    aPixmap_ = iPixmap_ = p;

  else {

    QRgb light;
    QRgb * data;
    QRgb w = qRgb(255, 255, 255);

    QImage aTx(p.convertToImage());
    QImage iTx(aTx.copy());

    light  = options->color(Options::ButtonBg, true).light(150).rgb();

    if (light == qRgb(0, 0, 0))
      light = qRgb(228, 228, 228);

    data = (QRgb *)aTx.bits();

    for (int x = 0; x < 144; x++)
      if (data[x] == w)
        data[x] = light;

    light  = options->color(Options::ButtonBg, false).light(150).rgb();

    if (light == qRgb(0, 0, 0))
      light = qRgb(228, 228, 228);

    data = (QRgb *)iTx.bits();

    for (int x = 0; x < 144; x++)
      if (data[x] == w)
        data[x] = light;

    aPixmap_.convertFromImage(aTx);
    iPixmap_.convertFromImage(iTx);

  }

  repaint();
}

  void
Button::paintEvent(QPaintEvent *)
{
  bitBlt(this, alignment_ == Left ? 1 : 0, 0,
      &Static::instance()->buttonBase(active_, down_));

  int i = width() / 2 - 6;

  if (active_)
    bitBlt(this, alignment_ == Left ? i + 1 : i, i + 1, &aPixmap_);
  else
    bitBlt(this, alignment_ == Left ? i + 1 : i, i + 1, &iPixmap_);
}

} // End namespace

// vim:ts=2:sw=2:tw=78
#include "Button.moc"
