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

#include <qimage.h>
#include <qpixmap.h>
#include <qpainter.h>
#include <kpixmapeffect.h>

#include "../../options.h"

#include "Static.h"

namespace Default
{

static void drawButtonFrame(KPixmap & pix, const QColorGroup &g, bool sunken)
{
    QPainter p;

    int w = pix.width();
    int h = pix.height();

    p.begin(&pix);

    p.setPen(g.dark());

    p.drawLine(0, 0, w-1, 0);
    p.drawLine(0, 0, 0, h-1);

    p.setPen(sunken ? g.dark() : g.light());
    p.drawLine(1, 1, w-2, 1);
    p.drawLine(1, 1, 1, h-2);

    p.setPen(sunken ? g.light() : g.dark());
    p.drawLine(w-2, 1, w-2, h-2);
    p.drawLine(1, h-2, w-2, h-2);

    p.setPen(g.light());
    p.drawLine(w-1, 0, w-1, h-1);
    p.drawLine(0, h-1, w-1, h-1);

    p.end();
}


Static * Static::instance_ = 0L;

  void
Static::_init()
{
  aResize_.setOptimization(QPixmap::MemoryOptim);
  iResize_.setOptimization(QPixmap::MemoryOptim);
  aResize_.resize(30, RESIZE_BAR_HEIGHT);
  iResize_.resize(30, RESIZE_BAR_HEIGHT);
  aResize_.fill(Qt::black);
  iResize_.fill(Qt::black);

  aTitle_           .setOptimization(QPixmap::BestOptim);
  iTitle_           .setOptimization(QPixmap::BestOptim);

  aResizeMidLeft_   .setOptimization(QPixmap::BestOptim);
  aResizeMidRight_  .setOptimization(QPixmap::BestOptim);
  aResizeMid_       .setOptimization(QPixmap::BestOptim);

  iResizeMidLeft_   .setOptimization(QPixmap::BestOptim);
  iResizeMidRight_  .setOptimization(QPixmap::BestOptim);
  iResizeMid_       .setOptimization(QPixmap::BestOptim);

  aResizeMidLeft_   .resize(3, 12);
  aResizeMidRight_  .resize(3, 12);
  aResizeMidLeft_   .fill(Qt::black);
  aResizeMidRight_  .fill(Qt::black);

  iResizeMidLeft_   .resize(3, 12);
  iResizeMidRight_  .resize(3, 12);
  iResizeMidLeft_   .fill(Qt::black);
  iResizeMidRight_  .fill(Qt::black);

  aResizeMid_       .resize(128, RESIZE_BAR_HEIGHT);
  iResizeMid_       .resize(128, RESIZE_BAR_HEIGHT);
  aResizeMid_       .fill(Qt::black);
  iResizeMid_       .fill(Qt::black);

  _loadGlyphs();

  update();
}

  void
Static::update()
{
  QPainter p;

  titleHeight_ = 24;
  btnWidth1_ = 36;
  btnWidth2_ = 24;

  unsigned int availableTitleHeight = titleHeight_ - 6;

  aTitle_.resize(36, availableTitleHeight);
  iTitle_.resize(36, availableTitleHeight);

  aTitleText_.resize(36, availableTitleHeight);
  iTitleText_.resize(36, availableTitleHeight);

  aTitleLeft_.resize(2, availableTitleHeight);
  iTitleLeft_.resize(2, availableTitleHeight);

  aTitleRight_.resize(2, availableTitleHeight);
  iTitleRight_.resize(2, availableTitleHeight);

  QColor bgColor, light, dark;
  QColorGroup buttonBgColorGroup = options->colorGroup(Options::ButtonBg, true);
  KPixmapEffect::GradientType vertGrad = KPixmapEffect::VerticalGradient;
  KPixmapEffect::GradientType diagGrad = KPixmapEffect::DiagonalGradient;
  QSize buttonSize1(btnWidth1_, availableTitleHeight);
  QSize buttonSize2(btnWidth2_, availableTitleHeight);

  // Titlebar

  bgColor = options->color(Options::TitleBar, true);

  light   = bgColor.light(120);
  dark    = bgColor.dark(120);

  KPixmapEffect::gradient(aTitle_, light, dark, vertGrad);
  KPixmapEffect::gradient(aTitleText_, light, dark, vertGrad);
  KPixmapEffect::gradient(aTitleLeft_, light, dark, vertGrad);
  KPixmapEffect::gradient(aTitleRight_, light, dark, vertGrad);

  p.begin(&aTitle_);
  p.setPen(dark);
  p.drawLine(0, 0, 36, 0);
  p.setPen(light);
  p.drawLine(0, availableTitleHeight - 1, 36, availableTitleHeight - 1);
  p.end();

  p.begin(&aTitleText_);
  p.setPen(dark);
  p.drawLine(0, 0, 36, 0);
  p.setPen(light);
  p.drawLine(0, availableTitleHeight - 1, 36, availableTitleHeight - 1);
  p.end();

  p.begin(&aTitleLeft_);
  p.setPen(dark);
  p.drawPoint(1, 0);
  p.drawLine(0, 0, 0, availableTitleHeight - 1);
  p.setPen(light);
  p.drawPoint(1, availableTitleHeight - 1);
  p.end();

  p.begin(&aTitleRight_);
  p.setPen(dark);
  p.drawPoint(0, 1);
  p.setPen(light);
  p.drawLine(1, 0, 1, availableTitleHeight - 1);
  p.drawPoint(0, availableTitleHeight - 1);
  p.end();

  bgColor = options->color(Options::TitleBar, false);
  light   = bgColor.light(120);
  dark    = bgColor.dark(120);

  KPixmapEffect::gradient(iTitle_, light, dark, vertGrad);
  KPixmapEffect::gradient(iTitleText_, light, dark, vertGrad);
  KPixmapEffect::gradient(iTitleLeft_, light, dark, vertGrad);
  KPixmapEffect::gradient(iTitleRight_, light, dark, vertGrad);

  p.begin(&iTitle_);
  p.setPen(dark);
  p.drawLine(0, 0, 36, 0);
  p.setPen(light);
  p.drawLine(0, availableTitleHeight - 1, 36, availableTitleHeight - 1);
  p.end();

  p.begin(&iTitleText_);
  p.setPen(dark);
  p.drawLine(0, 0, 36, 0);
  p.setPen(light);
  p.drawLine(0, availableTitleHeight - 1, 36, availableTitleHeight - 1);
  p.end();

  p.begin(&iTitleLeft_);
  p.setPen(dark);
  p.drawPoint(1, 0);
  p.drawLine(0, 0, 0, availableTitleHeight - 1);
  p.setPen(light);
  p.drawPoint(1, availableTitleHeight - 1);
  p.end();

  p.begin(&iTitleRight_);
  p.setPen(dark);
  p.drawPoint(0, 1);
  p.setPen(light);
  p.drawLine(1, 0, 1, availableTitleHeight - 1);
  p.drawPoint(0, availableTitleHeight - 1);
  p.end();

  p.begin(&aTitle_);
  p.setPen(options->color(Options::TitleBar, true).light(150));

  for (unsigned int y = 3; y < availableTitleHeight - 3; y += 3)
    for (unsigned int x = 1; x < 36; x += 3)
      p.drawPoint(x, y);

  p.setPen(options->color(Options::TitleBar, true).dark(150));

  for (unsigned int y = 3; y < availableTitleHeight - 3; y += 3)
    for (unsigned int x = 1; x < 36; x += 3)
      p.drawPoint(x + 1, y + 1);
  p.end();

  // Buttons

  bgColor = buttonBgColorGroup.background();
  light   = bgColor.light(120);
  dark    = bgColor.dark(120);

  btnPix1_      .resize(buttonSize1);
  btnDownPix1_  .resize(buttonSize1);
  btnPix2_      .resize(buttonSize2);
  btnDownPix2_  .resize(buttonSize2);

  iBtnPix1_     .resize(buttonSize1);
  iBtnDownPix1_ .resize(buttonSize1);
  iBtnPix2_     .resize(buttonSize2);
  iBtnDownPix2_ .resize(buttonSize2);

  KPixmapEffect::gradient(btnPix1_,     light, dark, diagGrad);
  KPixmapEffect::gradient(btnDownPix1_, dark, light, diagGrad);
  KPixmapEffect::gradient(btnPix2_,     light, dark, diagGrad);
  KPixmapEffect::gradient(btnDownPix2_, dark, light, diagGrad);

  KPixmapEffect::gradient(iBtnPix1_,     light, dark, diagGrad);
  KPixmapEffect::gradient(iBtnDownPix1_, dark, light, diagGrad);
  KPixmapEffect::gradient(iBtnPix2_,     light, dark, diagGrad);
  KPixmapEffect::gradient(iBtnDownPix2_, dark, light, diagGrad);

  drawButtonFrame(btnPix1_,       buttonBgColorGroup, false);
  drawButtonFrame(btnDownPix1_,   buttonBgColorGroup, true);
  drawButtonFrame(btnPix2_,       buttonBgColorGroup, false);
  drawButtonFrame(btnDownPix2_,   buttonBgColorGroup, true);

  drawButtonFrame(iBtnPix1_,      buttonBgColorGroup, false);
  drawButtonFrame(iBtnDownPix1_,  buttonBgColorGroup, true);
  drawButtonFrame(iBtnPix2_,      buttonBgColorGroup, false);
  drawButtonFrame(iBtnDownPix2_,  buttonBgColorGroup, true);

  p.flush();
}

  QPixmap
Static::button(SymbolType t, bool active, bool down)
{
  bool buttonSize2 = (t == Sticky || t == Unsticky);
  QPixmap px = buttonPixmap(!buttonSize2, active, down);

  QBitmap b = glyph(t);
  b.setMask(b);

  QPainter p(&px);
  QPoint offset((buttonSize2 ? btnWidth2_ : btnWidth1_) / 2 - 4, (titleHeight_ - 6) / 2 - 4);
  p.drawPixmap(offset, b);

  return px;
}

static unsigned char iconify_bits[] = {
    0xff, 0xff, 0x00, 0xff, 0xff, 0x7e, 0x3c, 0x18};

static unsigned char close_bits[] = {
    0xc3, 0x66, 0x3c, 0x18, 0x3c, 0x66, 0xc3, 0x00 };

static unsigned char maximize_bits[] = {
    0x18, 0x3c, 0x7e, 0xff, 0xff, 0x00, 0xff, 0xff };

static unsigned char unmax_bits[] = {
   0x30, 0x18, 0xcc, 0xe6, 0xf3, 0xf9, 0xfc, 0xfc};

static unsigned char unsticky_bits[] = {
   0x3c, 0x42, 0x99, 0xbd, 0xbd, 0x99, 0x42, 0x3c};

static unsigned char sticky_bits[] = {
   0x3c, 0x42, 0x81, 0x81, 0x81, 0x81, 0x42, 0x3c};

static unsigned char question_bits[] = {
    0x3c, 0x66, 0x60, 0x30, 0x18, 0x00, 0x18, 0x18};


  void
Static::_loadGlyphs()
{
  glyphClose_       = QBitmap(8, 8, close_bits);
  glyphMaximise_    = QBitmap(8, 8, maximize_bits);
  glyphUnmaximise_  = QBitmap(8, 8, unmax_bits);
  glyphSticky_      = QBitmap(8, 8, sticky_bits);
  glyphUnsticky_    = QBitmap(8, 8, unsticky_bits);
  glyphIconify_     = QBitmap(8, 8, iconify_bits);
}

} // End namespace

// vim:ts=2:sw=2:tw=78

