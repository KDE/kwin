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

#ifndef DEFAULT_STATIC_H
#define DEFAULT_STATIC_H

#include <qimage.h>
#include <qpixmap.h>
#include <qpainter.h>
#include <qbitmap.h>

#include <kpixmap.h>

#include "Defines.h"

namespace Default
{

enum SymbolType { Sticky, Unsticky, Close, Iconify, Max, Unmax };

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

    const QPixmap & title(bool active) const
    { return active ? aTitle_ : iTitle_; }

    const QPixmap & titleText(bool active) const
    { return active ? aTitleText_ : iTitleText_; }

    const QPixmap  & titleLeft(bool active) const
    { return active ? aTitleLeft_ : iTitleLeft_; }

    const QPixmap & titleRight(bool active) const
    { return active ? aTitleRight_ : iTitleRight_; }

    const QPixmap & resizeMidLeft(bool active) const
    { return active ? aResizeMidLeft_ : iResizeMidLeft_; }

    const QPixmap & resizeMidRight(bool active) const
    { return active ? aResizeMidRight_ : iResizeMidRight_; }

    const QPixmap & resizeMidMid(bool active) const
    { return active ? aResizeMid_ : iResizeMid_; }

    QPixmap button(SymbolType t, bool active, bool down);

    const QPixmap & resize(bool active) const
    { return active ? aResize_ : iResize_; }

    const QBitmap & glyph(SymbolType t) const
    {
      switch (t) {
        case Close:     return glyphClose_;
        case Sticky:    return glyphSticky_;
        case Unsticky:  return glyphUnsticky_;
        case Iconify:   return glyphIconify_;
        case Max:       return glyphMaximise_;
        case Unmax:     return glyphUnmaximise_;
        default:        return glyphClose_;
      }
    }

    unsigned int buttonWidth1() const { return btnWidth1_; }
    unsigned int buttonWidth2() const { return btnWidth2_; }
    unsigned int titleHeight() const { return titleHeight_; }

    const KPixmap & buttonPixmap(int n, bool active, bool down) const
    {
      if (active) {
        if (n == 1)
          if (down)
            return btnDownPix1_;
          else
            return btnPix1_;
        else
          if (down)
            return btnDownPix2_;
          else
            return btnPix2_;
      } else {
        if (n == 1)
          if (down)
            return iBtnDownPix1_;
          else
            return iBtnPix1_;
        else
          if (down)
            return iBtnDownPix2_;
          else
            return iBtnPix2_;
      }
 
    }

  private:

    void _init();
    void _loadGlyphs();

    static Static * instance_;

    QPixmap
      aResize_,         iResize_,
      aResizeDown_,     iResizeDown_,
      aResizeMidLeft_,  aResizeMidRight_,
      iResizeMidLeft_,  iResizeMidRight_,
      aResizeMid_,      iResizeMid_;

    QBitmap
      glyphClose_,
      glyphSticky_,
      glyphUnsticky_,
      glyphIconify_,
      glyphMaximise_,
      glyphUnmaximise_;

    KPixmap aTitle_, iTitle_;
    KPixmap aTitleLeft_, iTitleLeft_;
    KPixmap aTitleRight_, iTitleRight_;
    KPixmap aTitleText_, iTitleText_;

    KPixmap btnPix1_, btnDownPix1_, iBtnPix1_, iBtnDownPix1_;
    KPixmap btnPix2_, btnDownPix2_, iBtnPix2_, iBtnDownPix2_;

    unsigned int btnWidth1_;
    unsigned int btnWidth2_;

    unsigned int titleHeight_;
};

} // End namespace

#endif
 
// vim:ts=2:sw=2:tw=78
