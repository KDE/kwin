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

#include <ksimpleconfig.h>

#include "../../options.h"

#include "Static.h"

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
  animationStyle_ = 0;

  aButtonUp_        .setOptimization(QPixmap::BestOptim);
  aButtonDown_      .setOptimization(QPixmap::BestOptim);

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
  QFont f(options->font(true)); // XXX false doesn't work right at the moment

  QFontMetrics fm(f);

  int h = fm.height();

  titleHeight_ = h + 6;

  if (titleHeight_ < 20)
    titleHeight_ = 20;

  int buttonSize = titleHeight_ - 1;

  resizeHeight_ = 10;

  // Sizes.

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
  aButtonUp_        .resize(buttonSize, buttonSize);
  iButtonUp_        .resize(buttonSize, buttonSize);
  aButtonDown_      .resize(buttonSize, buttonSize);
  iButtonDown_      .resize(buttonSize, buttonSize);

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

  // -------------------------------------------------------------------------
  // Palettes
  // -------------------------------------------------------------------------

  Palette aBut, iBut;
  QPixmap aTexture;
  QPixmap iTexture;

  bool hicolour = QPixmap::defaultDepth() > 8;

  if (hicolour) {

    setPalette(aBut, options->color(Options::ButtonBg, true));
    setPalette(iBut, options->color(Options::ButtonBg, false));

    setPalette(aTitlePal_,  options->color(Options::TitleBar, true));
    setPalette(iTitlePal_,  options->color(Options::TitleBar, false));

    setPalette(aResizePal_, options->color(Options::TitleBar, true));
    setPalette(iResizePal_, options->color(Options::TitleBar, false));

    // -----------------------------------------------------------------------
    // Convert colours in texture to match colour scheme.
    // -----------------------------------------------------------------------

    QRgb light, dark;
    QRgb * data;
    QRgb w = qRgb(255,255,255);
    QRgb b = qRgb(0,0,0);

    QPixmap tx = QPixmap((const char **)texture_xpm);
    QImage aTx(tx.convertToImage());
    QImage iTx(aTx.copy());

    light  = options->color(Options::TitleBar, true).light(110).rgb();
    dark   = options->color(Options::TitleBar, true).dark(110).rgb();

    data = (QRgb *)aTx.bits();

    for (int x = 0; x < 64*12; x++)
      if (data[x] == w)
        data[x] = light;
      else if (data[x] == b)
        data[x] = dark;

    light  = options->color(Options::TitleBar, false).light(110).rgb();
    dark   = options->color(Options::TitleBar, false).dark(110).rgb();

    data = (QRgb *)iTx.bits();

    for (int x = 0; x < 64*12; x++)
      if (data[x] == w)
        data[x] = light;
      else if (data[x] == b)
        data[x] = dark;


    aTexture.convertFromImage(aTx);
    iTexture.convertFromImage(iTx);
  }

  // -------------------------------------------------------------------------
  // Title text area sides
  // -------------------------------------------------------------------------

  QPixmap temp(4, titleHeight_);

  transx = transy = 0.0;

  temp.fill(Qt::black);
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
  

  // -------------------------------------------------------------------------
  // Resize central area sides.
  // -------------------------------------------------------------------------

  transy = 1.0;

  palette_ = aResizePal_;
  temp.resize(4, resizeHeight_);
  temp.fill(Qt::black);

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


  // -------------------------------------------------------------------------
  // Title text area background
  // -------------------------------------------------------------------------

  transx = transy = 0.0;
  temp.resize(70, titleHeight_);
  temp.fill(Qt::black);

  palette_ = aTitlePal_;
  _drawBorder(temp, 70, titleHeight_ - 3);
  
  painter_.begin(&aTitleTextMid_);
  painter_.drawPixmap(0, 1, temp, 2, 0);
  if (hicolour)
    painter_.drawTiledPixmap(0, 4, 64, titleHeight_ - 8, aTexture);
  painter_.end();

  palette_ = iTitlePal_;
  _drawBorder(temp, 70, titleHeight_ - 3);
  
  painter_.begin(&iTitleTextMid_);
  painter_.drawPixmap(0, 1, temp, 2, 0);
  if (hicolour)
    painter_.drawTiledPixmap(0, 4, 64, titleHeight_ - 8, iTexture);
  painter_.end();


  // -------------------------------------------------------------------------
  // Resize central area background
  // -------------------------------------------------------------------------

  transy = 1.0;
  temp.fill(Qt::black);

  palette_ = aResizePal_;
  _drawBorder(temp, 70, resizeHeight_ - 3);
  
  painter_.begin(&aResizeMid_);
  painter_.drawPixmap(0, 0, temp, 2, 0);
  if (hicolour)
    painter_.drawTiledPixmap(0, 4, 64, resizeHeight_ - 8, aTexture);
  painter_.end();

  palette_ = iResizePal_;
  _drawBorder(temp, 70, 7);
  
  painter_.begin(&iResizeMid_);
  painter_.drawPixmap(0, 0, temp, 2, 0);
  if (hicolour)
    painter_.drawTiledPixmap(0, 4, 64, resizeHeight_ - 8, iTexture);
  painter_.end();

  // -------------------------------------------------------------------------
  // Resize handles
  // -------------------------------------------------------------------------

  transx = transy = 1.0;

  down_ = false;

  palette_ = aResizePal_;
  _drawBorder(aResize_, 28, resizeHeight_ - 3);

  if (hicolour) {
    painter_.begin(&aResize_);
    painter_.drawTiledPixmap(4, 4, 20, resizeHeight_ - 8, aTexture);
    painter_.end();
  }

  palette_ = iResizePal_;
  _drawBorder(iResize_, 28, resizeHeight_ - 3);

  if (hicolour) {
    painter_.begin(&iResize_);
    painter_.drawTiledPixmap(4, 4, 20, resizeHeight_ - 8, iTexture);
    painter_.end();
  }

  // -------------------------------------------------------------------------
  // Button backgrounds
  // -------------------------------------------------------------------------
  qDebug("buttonSize == %d", buttonSize);

  buttonSize -=2;

  down_ = false;
  transx = 0.0;
  transy = 1.0;
  palette_ = aBut;
  _drawBorder(aButtonUp_, buttonSize, buttonSize);
  down_ = true;
  _drawBorder(aButtonDown_, buttonSize, buttonSize);

  palette_ = iBut;
  _drawBorder(iButtonDown_, buttonSize, buttonSize);
  down_ = false;
  _drawBorder(iButtonUp_, buttonSize, buttonSize);

  painter_.begin(&aButtonUp_);

  if (hicolour)
    painter_.drawTiledPixmap(4, 4, buttonSize - 8, buttonSize - 8, aTexture);

  painter_.end();

  painter_.begin(&iButtonUp_);
  if (hicolour)
    painter_.drawTiledPixmap(4, 4, buttonSize - 8, buttonSize - 8, iTexture);

  painter_.end();

  // ------------------------------------------------------------------------
  // Button order
  // ------------------------------------------------------------------------

  KConfig * c = new KConfig("kwinriscosrc", true, true);
  c->setGroup("WM");

  leftButtons_.clear();
  rightButtons_.clear();

  if (c->hasKey("LeftButtons"))
    leftButtons_ = c->readListEntry("LeftButtons");
  else
    leftButtons_ << "Lower" << "Close" << "Sticky";

  if (c->hasKey("RightButtons"))
    rightButtons_ = c->readListEntry("RightButtons");
  else
    rightButtons_ << "Help" << "Iconify" << "Maximize";

  animationStyle_ = c->readNumEntry("IconifyAnimationStyle", 0);

  delete c;
  c = 0;
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

