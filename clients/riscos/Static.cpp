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

#include <qimage.h>
#include <qpixmap.h>
#include <qpainter.h>

#include "../../options.h"

#include "Static.h"

namespace RiscOS
{

  void
Static::_drawButtonBorder(QPixmap & pix)
{
  _drawBorder(pix, 17, 17);
}

  void
Static::_drawBorder(QPixmap & pix, int w, int h)
{
// 0111111113
// 1022222235
// 12      46
// 12      46
// 12      46
// 1344444476
// 3566666667

  painter_.begin(&pix);
  painter_.translate(transx, transy);

  QColor c0 = down_ ? palette_[7] : palette_[0];
  QColor c1 = down_ ? palette_[6] : palette_[1];
  QColor c2 = down_ ? palette_[5] : palette_[2];
  QColor c3 = down_ ? palette_[4] : palette_[3];
  QColor c4 = down_ ? palette_[3] : palette_[4];
  QColor c5 = down_ ? palette_[2] : palette_[5];
  QColor c6 = down_ ? palette_[2] : palette_[6];
  QColor c7 = down_ ? palette_[1] : palette_[7];

  painter_.setPen(c0);
  painter_.drawPoint(0, 0);
  
  painter_.setPen(c1);
  painter_.drawPoint(1, 1);
  painter_.drawLine(1, 0, w - 1, 0);
  painter_.drawLine(0, 1, 0, h - 1);

  painter_.setPen(c2);
  painter_.drawLine(2, 1, w - 2, 1);
  painter_.drawLine(1, 2, 1, h - 2);

  painter_.setPen(c3);
  painter_.drawPoint(0, h);
  painter_.drawPoint(1, h - 1);
  painter_.drawPoint(w, 0);
  painter_.drawPoint(w - 1, 1);
  painter_.fillRect(2, 2, w - 2, h - 2, c3);

  painter_.setPen(c4);
  painter_.drawLine(2, h - 1, w - 2, h - 1);
  painter_.drawLine(w - 1, 2, w - 1, h - 2);

  painter_.setPen(c5);
  painter_.drawPoint(w, 1);
  painter_.drawPoint(1, h);

  painter_.setPen(c6);
  painter_.drawLine(w, 2, w, h - 1);
  painter_.drawLine(2, h, w - 1, h);

  painter_.setPen(c7);
  painter_.drawPoint(w - 1, h - 1);
  painter_.drawPoint(w, h);
  
  painter_.end();
}

  void
setPalette(Palette & pal, QColor c)
{
  pal[3] = c.rgb();

  int h, s, v;
  c.hsv(&h, &s, &v);

  if (v < 72)
    c.setHsv(h, s, 72);

  pal[0] = c.light(200).rgb();
  pal[1] = c.light(166).rgb();
  pal[2] = c.light(125).rgb();
  pal[4] = c.dark(133).rgb();
  pal[5] = c.dark(166).rgb();
  pal[6] = c.dark(200).rgb();
  pal[7] = c.dark(300).rgb();
}

Static * Static::instance_ = 0L;

  void
Static::_init()
{
  aResize_          .resize(30, 10);
  iResize_          .resize(30, 10);
  aTitleTextLeft_   .resize(3, 20);
  aTitleTextRight_  .resize(3, 20);
  iTitleTextLeft_   .resize(3, 20);
  iTitleTextRight_  .resize(3, 20);
  aTitleTextMid_    .resize(128, 20);
  iTitleTextMid_    .resize(128, 20);
  aResizeMidLeft_   .resize(3, 12);
  aResizeMidRight_  .resize(3, 12);
  iResizeMidLeft_   .resize(3, 12);
  iResizeMidRight_  .resize(3, 12);
  aResizeMid_       .resize(128, 10);
  iResizeMid_       .resize(128, 10);
  aButtonUp_        .resize(19, 19);
  iButtonUp_        .resize(19, 19);
  aButtonDown_      .resize(19, 19);
  iButtonDown_      .resize(19, 19);

  aResize_          .fill(Qt::black);
  iResize_          .fill(Qt::black);
  aTitleTextLeft_   .fill(Qt::black);
  aTitleTextRight_  .fill(Qt::black);
  iTitleTextLeft_   .fill(Qt::black);
  iTitleTextRight_  .fill(Qt::black);
  aTitleTextMid_    .fill(Qt::black);
  iTitleTextMid_    .fill(Qt::black);
  aResizeMidLeft_   .fill(Qt::black);
  aResizeMidRight_  .fill(Qt::black);
  iResizeMidLeft_   .fill(Qt::black);
  iResizeMidRight_  .fill(Qt::black);
  aResizeMid_       .fill(Qt::black);
  iResizeMid_       .fill(Qt::black);
  aButtonUp_        .fill(Qt::black);
  iButtonUp_        .fill(Qt::black);
  aButtonDown_      .fill(Qt::black);
  iButtonDown_      .fill(Qt::black);

  aButtonUp_        .setOptimization(QPixmap::MemoryOptim);
  aButtonDown_      .setOptimization(QPixmap::MemoryOptim);

  iButtonUp_        .setOptimization(QPixmap::MemoryOptim);
  iButtonDown_      .setOptimization(QPixmap::MemoryOptim);

  aTitleTextLeft_   .setOptimization(QPixmap::BestOptim);
  aTitleTextRight_  .setOptimization(QPixmap::BestOptim);
  aTitleTextMid_    .setOptimization(QPixmap::BestOptim);

  iTitleTextLeft_   .setOptimization(QPixmap::BestOptim);
  iTitleTextRight_  .setOptimization(QPixmap::BestOptim);
  iTitleTextMid_    .setOptimization(QPixmap::BestOptim);

  aResizeMidLeft_   .setOptimization(QPixmap::BestOptim);
  aResizeMidRight_  .setOptimization(QPixmap::BestOptim);
  aResizeMid_       .setOptimization(QPixmap::BestOptim);

  iResizeMidLeft_   .setOptimization(QPixmap::BestOptim);
  iResizeMidRight_  .setOptimization(QPixmap::BestOptim);
  iResizeMid_       .setOptimization(QPixmap::BestOptim);

  aResize_          .setOptimization(QPixmap::BestOptim);
  iResize_          .setOptimization(QPixmap::BestOptim);

  update();
}

  void
Static::update()
{
  // -------------------------------------------------------------------------
  // Palettes
  // -------------------------------------------------------------------------

  Palette aBut, iBut;

  if (QPixmap::defaultDepth() > 8) {

    setPalette(aBut, options->color(Options::ButtonBg, true));
    setPalette(iBut, options->color(Options::ButtonBg, false));

    setPalette(aTitlePal_,  options->color(Options::TitleBar, true));
    setPalette(iTitlePal_,  options->color(Options::TitleBar, false));

    setPalette(aResizePal_, options->color(Options::Handle, true));
    setPalette(iResizePal_, options->color(Options::Handle, false));
  }

  // -------------------------------------------------------------------------
  // Bevels
  // -------------------------------------------------------------------------

  transx = transy = 0.0;

  // Create sides of title text area and resize bar middle.

  QPixmap temp(4, 20);
  temp.fill(Qt::black);
  palette_ = aTitlePal_;
  down_ = false;

  _drawBorder(temp, 4, 18);

  painter_.begin(&aTitleTextLeft_);
  painter_.drawPixmap(1, 1, temp, 0, 1);
  painter_.end();

  painter_.begin(&aTitleTextRight_);
  painter_.drawPixmap(0, 1, temp, 2, 1);
  painter_.end();

  palette_ = iTitlePal_;
  _drawBorder(temp, 4, 18);

  painter_.begin(&iTitleTextLeft_);
  painter_.drawPixmap(1, 1, temp, 0, 1);
  painter_.end();

  painter_.begin(&iTitleTextRight_);
  painter_.drawPixmap(0, 1, temp, 2, 1);
  painter_.end();
  
  transy = 1.0;

  palette_ = aResizePal_;
  temp.resize(4, 10);
  temp.fill(Qt::black);
  _drawBorder(temp, 4, 7);

  painter_.begin(&aResizeMidLeft_);
  painter_.drawPixmap(0, 1, temp, 0, 1);
  painter_.end();

  painter_.begin(&aResizeMidRight_);
  painter_.drawPixmap(0, 1, temp, 2, 1);
  painter_.end();

  palette_ = iResizePal_;
  _drawBorder(temp, 4, 7);

  painter_.begin(&iResizeMidLeft_);
  painter_.drawPixmap(0, 1, temp, 0, 1);
  painter_.end();

  painter_.begin(&iResizeMidRight_);
  painter_.drawPixmap(0, 1, temp, 2, 1);
  painter_.end();

  transx = transy = 0.0;

  temp.resize(132, 20);
  
  temp.fill(Qt::black);

  palette_ = aTitlePal_;
  _drawBorder(temp, 132, 17);
  
  painter_.begin(&aTitleTextMid_);
  painter_.drawPixmap(0, 1, temp, 2, 0);
  painter_.end();

  palette_ = iTitlePal_;
  _drawBorder(temp, 132, 17);
  
  painter_.begin(&iTitleTextMid_);
  painter_.drawPixmap(0, 1, temp, 2, 0);
  painter_.end();

  transy = 1.0;

  temp.fill(Qt::black);

  palette_ = aResizePal_;
  _drawBorder(temp, 132, 7);
  
  painter_.begin(&aResizeMid_);
  painter_.drawPixmap(0, 0, temp, 2, 0);
  painter_.end();

  palette_ = iResizePal_;
  _drawBorder(temp, 132, 7);
  
  painter_.begin(&iResizeMid_);
  painter_.drawPixmap(0, 0, temp, 2, 0);
  painter_.end();

  down_ = false;

  transx = 0.0;

  palette_ = aBut;
  _drawButtonBorder(aButtonUp_);
  down_ = true;
  _drawButtonBorder(aButtonDown_);

  palette_ = iBut;
  _drawButtonBorder(iButtonDown_);
  down_ = false;
  _drawButtonBorder(iButtonUp_);

  // -------------------------------------------------------------------------
  // Resize handles
  // -------------------------------------------------------------------------

  transx = transy = 1.0;

  down_ = false;

  palette_ = aResizePal_;
  _drawBorder(aResize_, 28, 7);

  palette_ = iResizePal_;
  _drawBorder(iResize_, 28, 7);
}

  const QPixmap &
Static::buttonBase(bool active, bool down) const
{
  if (active)
    if (down)
      return aButtonDown_;
    else
      return aButtonUp_;
  else
    if (down)
      return iButtonDown_;
    else
      return iButtonUp_;
}

} // End namespace

// vim:ts=2:sw=2:tw=78

