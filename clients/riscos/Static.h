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

#ifndef RISC_OS_STATIC_H
#define RISC_OS_STATIC_H

#include <qimage.h>
#include <qpixmap.h>
#include <qpainter.h>

#include "Palette.h"

namespace RiscOS
{

enum SymbolType { Lower, Close, Iconify, Max, Unmax };

class Static
{

  public:

    Static()
    {
      instance_ = this;
      _init();
    }

    ~Static()
    {
      instance_ = 0L;
    }

    static Static * instance()
    {
      if (0 == instance_)
        new Static;

      return instance_;
    }
    
    void update();

    QPixmap titleTextLeft(bool active)
    { return active ? aTitleTextLeft_ : iTitleTextLeft_; }

    QPixmap titleTextRight(bool active)
    { return active ? aTitleTextRight_ : iTitleTextRight_; }

    QPixmap resizeMidLeft(bool active)
    { return active ? aResizeMidLeft_ : iResizeMidLeft_; }

    QPixmap resizeMidRight(bool active)
    { return active ? aResizeMidRight_ : iResizeMidRight_; }

    QPixmap titleTextMid(bool active)
    { return active ? aTitleTextMid_ : iTitleTextMid_; }
    
    QPixmap resizeMidMid(bool active)
    { return active ? aResizeMid_ : iResizeMid_; }

    QPixmap button(SymbolType t, bool active, bool down);

    QPixmap resize(bool active)
    { return active ? aResize_ : iResize_; }

    Palette & standardPalette()
    { return standardPal_; }

  private:

    void _drawButtonBorder(QPixmap &);
    void _drawBorder(QPixmap &, int w, int h);
    void _init();
    void _drawCloseSymbol(QPixmap & pixmap);
    void _drawIconifySymbol(QPixmap & pixmap);
    void _drawLowerSymbol(QPixmap & pixmap);
    void _drawMaxSymbol(QPixmap & pixmap);
    void _drawUnmaxSymbol(QPixmap & pixmap);

    static Static * instance_;

    Palette standardPal_, aTitlePal_, iTitlePal_, aResizePal_, iResizePal_;

    QPixmap aIconify_, aClose_, aLower_, aMax_, aUnmax_,
            iIconify_, iClose_, iLower_, iMax_, iUnmax_,
            aResize_, iResize_,
            aIconifyDown_, aCloseDown_, aLowerDown_, aMaxDown_, aUnmaxDown_,
            iIconifyDown_, iCloseDown_, iLowerDown_, iMaxDown_, iUnmaxDown_,
            aResizeDown_, iResizeDown_,
            aTitleTextLeft_, aTitleTextRight_,
            aResizeMidLeft_, aResizeMidRight_,
            iTitleTextLeft_, iTitleTextRight_,
            iResizeMidLeft_, iResizeMidRight_,
            aTitleTextMid_, iTitleTextMid_,
            aResizeMid_, iResizeMid_;

    QList<QPixmap> buttonPixmaps_;

    QPainter painter_;
    bool down_;
    Palette palette_;
    double transx, transy;
};

} // End namespace

#endif

// vim:ts=2:sw=2:tw=78
