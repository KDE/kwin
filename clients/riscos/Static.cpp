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
#include "Utils.h"

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

  QPixmap
recolour(const QImage & inputImage, const Palette & newPalette)
{
  QImage image(inputImage);
  Palette & standard = Static::instance()->standardPalette();

  int ncols = image.numColors();

  QRgb r;
  
  for (int i = 0; i < ncols; i++) {

    r = image.color(i);

    for (int c = 0; c < 8; c++) {

      if (r == standard[c])
      {
        image.setColor(i, newPalette[c]);
        continue;
      }
    }
  }
  
  QPixmap output;
  output.convertFromImage(image);
  return output;
}

  void
Static::_drawCloseSymbol(QPixmap & pixmap)
{
  painter_.begin(&pixmap);
  painter_.translate(transx, transy);

  painter_.setPen(QColor(palette_[1]));
  painter_.drawLine(2, 0, 11, 9);
  painter_.drawLine(0, 2, 9, 11);

  painter_.drawLine(0, 9, 9, 0);
  painter_.drawLine(2, 11, 11, 2);

  painter_.drawPoint(0, 1);
  painter_.drawPoint(1, 0);
  painter_.drawPoint(10, 0);
  painter_.drawPoint(11, 1);
  painter_.drawPoint(0, 10);
  painter_.drawPoint(1, 11);
  painter_.drawPoint(10, 11);
  painter_.drawPoint(11, 10);

  painter_.setPen(QColor(palette_[6]));
  painter_.drawLine(1, 2, 9, 10);
  painter_.drawLine(1, 1, 10, 10);
  painter_.drawLine(2, 1, 10, 9);
  
  painter_.drawLine(1, 9, 9, 1);
  painter_.drawLine(1, 10, 10, 1);
  painter_.drawLine(2, 10, 10, 2);

  painter_.end();
}

  void
Static::_drawIconifySymbol(QPixmap & pixmap)
{
  painter_.begin(&pixmap);
  painter_.translate(transx, transy);

  painter_.setPen(QColor(palette_[1]));
  painter_.drawRect(1, 4, 10, 4);

  painter_.setPen(QColor(palette_[3]));
  painter_.drawPoint(1, 4);
  painter_.drawPoint(1, 7);
  painter_.drawPoint(10, 4);
  painter_.drawPoint(10, 7);

  painter_.setPen(QColor(palette_[6]));
  painter_.drawLine(2, 5, 9, 5);
  painter_.drawLine(2, 6, 9, 6);

  painter_.end();
}

  void
Static::_drawLowerSymbol(QPixmap & pixmap)
{
  painter_.begin(&pixmap);
  painter_.translate(transx, transy);

  painter_.fillRect(1, 1, 6, 6, QColor(palette_[6]));
  painter_.fillRect(5, 5, 6, 6, QColor(palette_[6]));

  painter_.setPen(QColor(palette_[1]));
  painter_.drawRect(0, 0, 8, 8);
  painter_.drawRect(4, 4, 8, 8);

  painter_.setPen(QColor(palette_[3]));
  painter_.drawPoint(0, 0);
  painter_.drawPoint(7, 0);
  painter_.drawPoint(0, 7);
  painter_.drawPoint(3, 7);
  painter_.drawPoint(7, 3);
  painter_.drawPoint(4, 4);
  painter_.drawPoint(11, 4);
  painter_.drawPoint(4, 11);
  painter_.drawPoint(11, 11);

  painter_.setPen(QColor(palette_[5]));
  painter_.drawPoint(5, 7);
  painter_.drawPoint(7, 5);

  painter_.setPen(QColor(palette_[4]));
  painter_.drawPoint(7, 6);
  painter_.drawPoint(7, 7);
  painter_.drawPoint(6, 7);
  
  painter_.end();
}

  void
Static::_drawMaxSymbol(QPixmap & pixmap)
{
  painter_.begin(&pixmap);
  painter_.translate(transx, transy);

  painter_.setPen(QColor(palette_[1]));
  painter_.drawRect(2, 2, 8, 8);

  painter_.setPen(QColor(palette_[3]));
  painter_.drawPoint(2, 2);
  painter_.drawPoint(2, 9);
  painter_.drawPoint(9, 9);
  painter_.drawPoint(9, 2);

  painter_.fillRect(3, 3, 6, 6, QColor(palette_[6]));
  
  painter_.end();
}

  void
Static::_drawUnmaxSymbol(QPixmap & pixmap)
{
  painter_.begin(&pixmap);
  painter_.translate(transx, transy);

  painter_.setPen(QColor(palette_[1]));
  painter_.drawRect(0, 0, 11, 11);

  painter_.setPen(QColor(palette_[3]));
  painter_.drawPoint(0, 0);
  painter_.drawPoint(0, 11);
  painter_.drawPoint(11, 0);
  painter_.drawPoint(11, 11);

  painter_.fillRect(1, 1, 10, 10, QColor(palette_[6]));
  
  painter_.end();
}

  void
setPalette(Palette & pal, QColor c)
{
  pal[0] = c.light(200).rgb();
  pal[1] = c.light(166).rgb();
  pal[2] = c.light(125).rgb();
  pal[3] = c.rgb();
  pal[4] = c.dark(133).rgb();
  pal[5] = c.dark(166).rgb();
  pal[6] = c.dark(200).rgb();
  pal[7] = c.dark(300).rgb();
}

  void
setInversePalette(Palette & pal, QColor c)
{
  pal[7] = c.light(200).rgb();
  pal[6] = c.light(166).rgb();
  pal[5] = c.light(125).rgb();
  pal[4] = c.rgb();
  pal[3] = c.dark(133).rgb();
  pal[2] = c.dark(166).rgb();
  pal[1] = c.dark(200).rgb();
  pal[0] = c.dark(300).rgb();
}

Static * Static::instance_ = 0L;

  void
Static::_init()
{
  buttonPixmaps_.append(&aIconify_);
  buttonPixmaps_.append(&aClose_);
  buttonPixmaps_.append(&aLower_);
  buttonPixmaps_.append(&aMax_);
  buttonPixmaps_.append(&aUnmax_);
  buttonPixmaps_.append(&iIconify_);
  buttonPixmaps_.append(&iClose_);
  buttonPixmaps_.append(&iLower_);
  buttonPixmaps_.append(&iMax_);
  buttonPixmaps_.append(&iUnmax_);
  buttonPixmaps_.append(&aIconifyDown_);
  buttonPixmaps_.append(&aCloseDown_);
  buttonPixmaps_.append(&aLowerDown_);
  buttonPixmaps_.append(&aMaxDown_);
  buttonPixmaps_.append(&aUnmaxDown_);
  buttonPixmaps_.append(&iIconifyDown_);
  buttonPixmaps_.append(&iCloseDown_);
  buttonPixmaps_.append(&iLowerDown_);
  buttonPixmaps_.append(&iMaxDown_);
  buttonPixmaps_.append(&iUnmaxDown_);

  for (QListIterator<QPixmap> it(buttonPixmaps_); it.current(); ++it) {

    it.current()->setOptimization(QPixmap::MemoryOptim);
    it.current()->resize(19, 20);
    it.current()->fill(Qt::black);
  }

  aResize_.setOptimization(QPixmap::MemoryOptim);
  iResize_.setOptimization(QPixmap::MemoryOptim);
  aResize_.resize(30, 10);
  iResize_.resize(30, 10);
  aResize_.fill(Qt::black);
  iResize_.fill(Qt::black);

  aTitleTextLeft_.resize(3, 20);
  aTitleTextRight_.resize(3, 20);
  aTitleTextLeft_.fill(Qt::black);
  aTitleTextRight_.fill(Qt::black);
  
  iTitleTextLeft_.resize(3, 20);
  iTitleTextRight_.resize(3, 20);
  iTitleTextLeft_.fill(Qt::black);
  iTitleTextRight_.fill(Qt::black);
  
  aTitleTextMid_.resize(128, 20);
  iTitleTextMid_.resize(128, 20);
  aTitleTextMid_.fill(Qt::black);
  iTitleTextMid_.fill(Qt::black);
  
  aResizeMidLeft_.resize(3, 12);
  aResizeMidRight_.resize(3, 12);
  aResizeMidLeft_.fill(Qt::black);
  aResizeMidRight_.fill(Qt::black);
  iResizeMidLeft_.resize(3, 12);
  iResizeMidRight_.resize(3, 12);
  iResizeMidLeft_.fill(Qt::black);
  iResizeMidRight_.fill(Qt::black);

  aResizeMid_.resize(128, 10);
  iResizeMid_.resize(128, 10);
  aResizeMid_.fill(Qt::black);
  iResizeMid_.fill(Qt::black);

  update();
}

  void
Static::update()
{
  // -------------------------------------------------------------------------
  // Palettes
  // -------------------------------------------------------------------------

  Palette aBut, iBut;
  Palette aSym, iSym;

  if (QPixmap::defaultDepth() > 8) {

    setPalette(aBut, options->color(Options::ButtonBg, true));
    setPalette(iBut, options->color(Options::ButtonBg, false));

    setInversePalette(aSym,  options->color(Options::ButtonFg, true));
    setInversePalette(iSym,  options->color(Options::ButtonFg, false));
   
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

  palette_ = aBut;
  transx = transy = 1.0;
  _drawButtonBorder(aClose_);
  _drawButtonBorder(aLower_);
  transx = 0.0;
  _drawButtonBorder(aIconify_);
  _drawButtonBorder(aMax_);
  _drawButtonBorder(aUnmax_);

  palette_ = iBut;
  transx = transy = 1.0;
  _drawButtonBorder(iClose_);
  _drawButtonBorder(iLower_);
  transx = 0.0;
  _drawButtonBorder(iIconify_);
  _drawButtonBorder(iMax_);
  _drawButtonBorder(iUnmax_);

  down_ = true;

  palette_ = aBut;
  transx = transy = 1.0;
  _drawButtonBorder(aCloseDown_);
  _drawButtonBorder(aLowerDown_);
  transx = 0.0;
  _drawButtonBorder(aIconifyDown_);
  _drawButtonBorder(aMaxDown_);
  _drawButtonBorder(aUnmaxDown_);

  palette_ = iBut;
  transx = transy = 1.0;
  _drawButtonBorder(iCloseDown_);
  _drawButtonBorder(iLowerDown_);
  transx = 0.0;
  _drawButtonBorder(iIconifyDown_);
  _drawButtonBorder(iMaxDown_);
  _drawButtonBorder(iUnmaxDown_);

  // -------------------------------------------------------------------------
  // Button symbols
  // -------------------------------------------------------------------------
  
  transy = 4.0;

  palette_ = aSym;

  transx = 4.0;
  _drawCloseSymbol   (aClose_);
  _drawLowerSymbol   (aLower_);
  transx = 3.0;
  _drawIconifySymbol (aIconify_);
  _drawMaxSymbol     (aMax_);
  _drawUnmaxSymbol   (aUnmax_);

  transx = 4.0;
  _drawCloseSymbol   (aCloseDown_);
  _drawLowerSymbol   (aLowerDown_);
  transx = 3.0;
  _drawIconifySymbol (aIconifyDown_);
  _drawMaxSymbol     (aMaxDown_);
  _drawUnmaxSymbol   (aUnmaxDown_);

  palette_ = iSym;

  transx = 4.0;
  _drawCloseSymbol   (iClose_);
  _drawLowerSymbol   (iLower_);
  transx = 3.0;
  _drawIconifySymbol (iIconify_);
  _drawMaxSymbol     (iMax_);
  _drawUnmaxSymbol   (iUnmax_);

  transx = 4.0;
  _drawCloseSymbol   (iCloseDown_);
  _drawLowerSymbol   (iLowerDown_);
  transx = 3.0;
  _drawIconifySymbol (iIconifyDown_);
  _drawMaxSymbol     (iMaxDown_);
  _drawUnmaxSymbol   (iUnmaxDown_);

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

  QPixmap
Static::button(SymbolType t, bool active, bool down)
{
  QPixmap p(19, 20);

  if (down) {

    switch (t) {

      case Iconify: p = active ? aIconifyDown_ : iIconifyDown_;  break;
      case Close:   p = active ? aCloseDown_   : iCloseDown_;    break;
      case Lower:   p = active ? aLowerDown_   : iLowerDown_;    break;
      case Max:     p = active ? aMaxDown_     : iMaxDown_;      break;
      case Unmax:   p = active ? aUnmaxDown_   : iUnmaxDown_;    break;
      default:                                                   break;
    }
 
  } else {

    switch (t) {

      case Iconify: p = active ? aIconify_ : iIconify_;  break;
      case Close:   p = active ? aClose_   : iClose_;    break;
      case Lower:   p = active ? aLower_   : iLower_;    break;
      case Max:     p = active ? aMax_     : iMax_;      break;
      case Unmax:   p = active ? aUnmax_   : iUnmax_;    break;
      default:                                           break;
    }

  }

  return p;
}

} // End namespace

// vim:ts=2:sw=2:tw=78

