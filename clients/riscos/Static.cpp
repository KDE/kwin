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

// for enable-final
#ifdef Bool
#undef Bool 
#endif

#include <qimage.h>
#include <qpixmap.h>
#include <qpainter.h>
#include <qlist.h>

#include <ksimpleconfig.h>

#include "../../options.h"
#include "../../workspace.h"

#include "Static.h"

using namespace KWinInternal;

namespace RiscOS
{

/* XPM */
static const char * const texture_xpm[] = {
"64 12 3 1",
" 	c None",
"a	c #000000",
"b	c #FFFFFF",
"   b           b         b     b   b b       b        a       b ",
" b       aa   b  aaa aaa     b   a   b   aab     bb           a ",
" b     b    a b    b   b             b       aa      b b   aa   ",
" b b     b     a         a b   b b b  aa  ab             ab     ",
"         b     b   b b       b                 b   b  ba        ",
" aaa aaa         aa       b          b b b   b           b      ",
"   b       b      bb a b     aa  b         a  a  aaa a     b    ",
" b         b b b b     a b       b       b         b           b",
"   b b       b              b  a a b  b  aa      b       baa    ",
"       a     b    a  bb  aa    bb aa aaa  b     aa a            ",
"   a a       aab       b  b   aa aab          a  b              ",
" b        a        b   b aaa  a      b      bb     b   b     b b"};

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

  painter_.resetXForm();
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
  hicolour_ = QPixmap::defaultDepth() > 8;
  animationStyle_ = 0;
  updatePixmaps();
}

  void
Static::reset()
{
  updatePixmaps();
  Workspace::self()->slotResetAllClientsDelayed();
}

  void
Static::updatePixmaps()
{
  _initSizes();

  _resizeAllPixmaps();

  _blankAllPixmaps();

  if (hicolour_)
  {
    _initPalettes();
    _initTextures();
  }

  _drawTitleTextAreaSides();

  _drawResizeCentralAreaSides();

  _drawTitleTextAreaBackground();

  _drawResizeCentralAreaBackground();

  _drawResizeHandles();

  _drawButtonBackgrounds();
}

  void
Static::_createTexture(QPixmap & px, int t, bool active)
{
  const QImage texture(QPixmap((const char **)texture_xpm).convertToImage());
  const QRgb w(qRgb(255, 255, 255));
  const QRgb b(qRgb(0, 0, 0));

  QColor c(options->color(Options::ColorType(t), active));

  QRgb light(c.light(110).rgb());
  QRgb dark (c.dark(110).rgb());

  QRgb * data(reinterpret_cast<QRgb *>(texture.bits()));

  for (int x = 0; x < 64*12; x++)
    if (data[x] == w)
      data[x] = light;
    else if (data[x] == b)
      data[x] = dark;

  px.convertFromImage(texture);
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

  void
Static::_initSizes()
{
  QFont f(options->font(true)); // XXX false doesn't work right at the moment

  QFontMetrics fm(f);

  int h = fm.height();

  titleHeight_ = h + 6;

  if (titleHeight_ < 20)
    titleHeight_ = 20;

  buttonSize_ = titleHeight_ - 1;

  resizeHeight_ = 10;
}

  void
Static::_resizeAllPixmaps()
{
  aResize_          .resize(30, resizeHeight_);
  iResize_          .resize(30, resizeHeight_);
  aTitleTextLeft_   .resize(3, titleHeight_);
  aTitleTextRight_  .resize(3, titleHeight_);
  iTitleTextLeft_   .resize(3, titleHeight_);
  iTitleTextRight_  .resize(3, titleHeight_);
  aTitleTextMid_    .resize(64, titleHeight_);
  iTitleTextMid_    .resize(64, titleHeight_);
  aResizeMidLeft_   .resize(3, resizeHeight_);
  aResizeMidRight_  .resize(3, resizeHeight_);
  iResizeMidLeft_   .resize(3, resizeHeight_);
  iResizeMidRight_  .resize(3, resizeHeight_);
  aResizeMid_       .resize(64, resizeHeight_);
  iResizeMid_       .resize(64, resizeHeight_);

  aButtonUp_        .resize(buttonSize_, buttonSize_);
  iButtonUp_        .resize(buttonSize_, buttonSize_);
  aButtonDown_      .resize(buttonSize_, buttonSize_);
  iButtonDown_      .resize(buttonSize_, buttonSize_);
}

  void
Static::_blankAllPixmaps()
{
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
}

  void
Static::_initPalettes()
{
  setPalette(aButPal_, options->color(Options::ButtonBg, true));
  setPalette(iButPal_, options->color(Options::ButtonBg, false));

  setPalette(aTitlePal_,  options->color(Options::TitleBar, true));
  setPalette(iTitlePal_,  options->color(Options::TitleBar, false));

  setPalette(aResizePal_, options->color(Options::TitleBar, true));
  setPalette(iResizePal_, options->color(Options::TitleBar, false));
}

  void
Static::_initTextures()
{
  _createTexture(aTexture_,  Options::TitleBar, true);
  _createTexture(iTexture_,  Options::TitleBar, false);
  _createTexture(abTexture_, Options::ButtonBg, true);
  _createTexture(ibTexture_, Options::ButtonBg, false);
}

  void
Static::_drawTitleTextAreaSides()
{
  QPixmap temp(4, titleHeight_);
  temp.fill(Qt::black);

  transx = transy = 0.0;

  palette_ = aTitlePal_;
  down_ = false;

  _drawBorder(temp, 4, titleHeight_ - 2);

  painter_.begin(&aTitleTextLeft_);
  painter_.drawPixmap(1, 1, temp, 0, 1);
  painter_.end();

  painter_.begin(&aTitleTextRight_);
  painter_.drawPixmap(0, 1, temp, 2, 1);
  painter_.end();

  palette_ = iTitlePal_;
  _drawBorder(temp, 4, titleHeight_ - 2);

  painter_.begin(&iTitleTextLeft_);
  painter_.drawPixmap(1, 1, temp, 0, 1);
  painter_.end();

  painter_.begin(&iTitleTextRight_);
  painter_.drawPixmap(0, 1, temp, 2, 1);
  painter_.end();
}

  void
Static::_drawResizeCentralAreaSides()
{
  QPixmap temp(4, resizeHeight_);
  temp.fill(Qt::black);

  transy = 1.0;

  palette_ = aResizePal_;

  _drawBorder(temp, 4, resizeHeight_ - 3);

  painter_.begin(&aResizeMidLeft_);
  painter_.drawPixmap(0, 1, temp, 0, 1);
  painter_.end();

  painter_.begin(&aResizeMidRight_);
  painter_.drawPixmap(0, 1, temp, 2, 1);
  painter_.end();

  palette_ = iResizePal_;
  _drawBorder(temp, 4, resizeHeight_ - 3);

  painter_.begin(&iResizeMidLeft_);
  painter_.drawPixmap(0, 1, temp, 0, 1);
  painter_.end();

  painter_.begin(&iResizeMidRight_);
  painter_.drawPixmap(0, 1, temp, 2, 1);
  painter_.end();
}

  void
Static::_drawTitleTextAreaBackground()
{
  QPixmap temp(70, titleHeight_);
  temp.fill(Qt::black);

  transx = transy = 0.0;

  palette_ = aTitlePal_;
  _drawBorder(temp, 70, titleHeight_ - 3);

  painter_.begin(&aTitleTextMid_);
  painter_.drawPixmap(0, 1, temp, 2, 0);
  if (hicolour_)
    painter_.drawTiledPixmap(0, 4, 64, titleHeight_ - 8, aTexture_);
  painter_.end();

  palette_ = iTitlePal_;
  _drawBorder(temp, 70, titleHeight_ - 3);

  painter_.begin(&iTitleTextMid_);
  painter_.drawPixmap(0, 1, temp, 2, 0);
  if (hicolour_)
    painter_.drawTiledPixmap(0, 4, 64, titleHeight_ - 8, iTexture_);
  painter_.end();
}

  void
Static::_drawResizeCentralAreaBackground()
{
  QPixmap temp(70, titleHeight_);
  temp.fill(Qt::black);

  transy = 1.0;

  palette_ = aResizePal_;
  _drawBorder(temp, 70, resizeHeight_ - 3);

  painter_.begin(&aResizeMid_);
  painter_.drawPixmap(0, 0, temp, 2, 0);
  if (hicolour_)
    painter_.drawTiledPixmap(0, 4, 64, resizeHeight_ - 8, aTexture_);
  painter_.end();

  palette_ = iResizePal_;
  _drawBorder(temp, 70, 7);

  painter_.begin(&iResizeMid_);
  painter_.drawPixmap(0, 0, temp, 2, 0);
  if (hicolour_)
    painter_.drawTiledPixmap(0, 4, 64, resizeHeight_ - 8, iTexture_);
  painter_.end();
}

  void
Static::_drawResizeHandles()
{
  transx = transy = 1.0;

  down_ = false;

  palette_ = aResizePal_;
  _drawBorder(aResize_, 28, resizeHeight_ - 3);

  if (hicolour_)
  {
    painter_.begin(&aResize_);
    painter_.drawTiledPixmap(4, 4, 20, resizeHeight_ - 8, aTexture_);
    painter_.end();
  }

  palette_ = iResizePal_;
  _drawBorder(iResize_, 28, resizeHeight_ - 3);

  if (hicolour_)
  {
    painter_.begin(&iResize_);
    painter_.drawTiledPixmap(4, 4, 20, resizeHeight_ - 8, iTexture_);
    painter_.end();
  }
}

  void
Static::_drawButtonBackgrounds()
{
  buttonSize_ -= 2;

  down_ = false;
  transx = 0.0;
  transy = 1.0;
  palette_ = aButPal_;
  _drawBorder(aButtonUp_, buttonSize_, buttonSize_);
  down_ = true;
  _drawBorder(aButtonDown_, buttonSize_, buttonSize_);

  palette_ = iButPal_;
  _drawBorder(iButtonDown_, buttonSize_, buttonSize_);
  down_ = false;
  _drawBorder(iButtonUp_, buttonSize_, buttonSize_);

  painter_.begin(&aButtonUp_);

  if (hicolour_)
    painter_.drawTiledPixmap
      (2, 4, buttonSize_ - 4, buttonSize_ - 5, abTexture_);

  painter_.end();

  painter_.begin(&iButtonUp_);
  if (hicolour_)
    painter_.drawTiledPixmap
      (2, 4, buttonSize_ - 4, buttonSize_ - 5, ibTexture_);

  painter_.end();
}

} // End namespace

// vim:ts=2:sw=2:tw=78

