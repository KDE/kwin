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
#include <qstringlist.h>

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

    void reset();
    void updatePixmaps();

    const QPixmap & titleTextLeft(bool active) const
    { return active ? aTitleTextLeft_ : iTitleTextLeft_; }

    const QPixmap & titleTextRight(bool active) const
    { return active ? aTitleTextRight_ : iTitleTextRight_; }

    const QPixmap & resizeMidLeft(bool active) const
    { return active ? aResizeMidLeft_ : iResizeMidLeft_; }

    const QPixmap & resizeMidRight(bool active) const
    { return active ? aResizeMidRight_ : iResizeMidRight_; }

    const QPixmap & titleTextMid(bool active) const
    { return active ? aTitleTextMid_ : iTitleTextMid_; }

    const QPixmap & resizeMidMid(bool active) const
    { return active ? aResizeMid_ : iResizeMid_; }

    const QPixmap & buttonBase(bool active, bool down) const;

    const QPixmap & resize(bool active) const
    { return active ? aResize_ : iResize_; }

    int animationStyle() const
    { return animationStyle_; }

    int titleHeight() const
    { return titleHeight_; }

    int resizeHeight() const
    { return resizeHeight_; }

  private:

    void _init();

    void _drawBorder(QPixmap &, int, int);
    void _createTexture(QPixmap &, int, bool);

    void _initSizes();

    void _resizeAllPixmaps();
    void _blankAllPixmaps();

    void _initPalettes();
    void _initTextures();

    void _drawTitleTextAreaSides();
    void _drawResizeCentralAreaSides();
    void _drawTitleTextAreaBackground();
    void _drawResizeCentralAreaBackground();
    void _drawResizeHandles();
    void _drawButtonBackgrounds();

    static Static * instance_;

    Palette
      aTitlePal_,
      iTitlePal_,
      aResizePal_,
      iResizePal_,
      aButPal_,
      iButPal_;

    QPixmap
      aButtonUp_,         iButtonUp_,
      aButtonDown_,       iButtonDown_,
      aResize_,           iResize_,
      aResizeDown_,       iResizeDown_,
      aTitleTextLeft_,    iTitleTextLeft_,
      aTitleTextRight_,   iTitleTextRight_,
      aTitleTextMid_,     iTitleTextMid_,
      aResizeMidLeft_,    iResizeMidLeft_,
      aResizeMidRight_,   iResizeMidRight_,
      aResizeMid_,        iResizeMid_;

    QPixmap aTexture_, iTexture_, abTexture_, ibTexture_;

    QPainter painter_;
    bool down_;
    Palette palette_;
    double transx, transy;

    int animationStyle_;

    int titleHeight_;
    int buttonSize_;
    int resizeHeight_;

    bool hicolour_;
};

} // End namespace

#endif
 
// vim:ts=2:sw=2:tw=78
