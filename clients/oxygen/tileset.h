/***************************************************************************
 *   Copyright (C) 2006-2007 by Thomas Lübking                             *
 *   thomas.luebking@web.de                                                *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.             *
 ***************************************************************************/

#ifndef TILESET_H
#define TILESET_H

#include <QBitmap>
#include <QRect>
#include <QHash>
#include <QX11Info>
#include <X11/Xlib.h>
#include <X11/extensions/Xrender.h>
#include <fixx11h.h>

namespace Tile
{

   enum Section
   { // DON'T CHANGE THE ORDER FOR NO REASON, i misuse them on the masks...
      TopLeft = 0, TopRight, BtmLeft, BtmRight,
      TopMid, BtmMid, MidLeft, MidMid, MidRight
   };
   enum Position
   {
      Top = 0x1, Left=0x2, Bottom=0x4, Right=0x8,
      Ring=0xf, Center=0x10, Full=0x1f
   };

   typedef uint PosFlags;

inline static bool matches(PosFlags This, PosFlags That){return (This & That) == This;}

class Set
{
public:
   Set(const QPixmap &pix, int xOff, int yOff, int width, int height, int rx = 0, int ry = 0);
   Set(){}
   void render(const QRect &rect, QPainter *p, PosFlags pf = Ring) const;
   void outline(const QRect &rect, QPainter *p, QColor c, bool strong = false,
                PosFlags pf = Ring) const;
   Picture render(int width, int height, PosFlags pf = Ring) const;
   Picture render(const QSize &size, PosFlags pf = Ring) const;
   QRect rect(const QRect &rect, PosFlags pf) const;
   inline int width(Section sect) const {return pixmap[sect].width();}
   inline int height(Section sect) const {return pixmap[sect].height();}
   inline bool isQBitmap() const {return _isBitmap;}
protected:
   QPixmap pixmap[9];
private:
   int rxf, ryf;
   bool _isBitmap;
};

class Mask : public Set
{
public:
   Mask(const QPixmap &pix, int xOff, int yOff, int width, int height,
               int dx1, int dy1, int dx2, int dy2, int rx = 0, int ry = 0);
   Mask(){_dx[0] = _dx[1] = _dy[0] = _dy[1] = 0; _hasCorners = false;}
   void render(const QRect &rect, QPainter *p, const QBrush &fill,
               PosFlags pf = Full, bool justClip = false,
               QPoint offset = QPoint(), bool inverse = false,
               const QRect *outerRect = 0L) const;
   QRect bounds(const QRect &rect, PosFlags pf = Full) const;
   const QPixmap &corner(PosFlags pf, bool inverse = false) const;
   QRegion clipRegion(const QRect &rect, PosFlags pf = Ring) const;
   inline bool hasCorners() const {return _hasCorners;}
private:
   int _dx[2], _dy[2];
   QPixmap inversePixmap[9];
   bool _hasCorners;
};

class Line
{
public:
   Line(const QPixmap &pix, Qt::Orientation o, int d1, int d2);
   Line(){}
   void render(const QRect &rect, QPainter *p, PosFlags pf = Full, bool btmRight = false) const;
   inline int thickness() const { return _thickness; }
private:
   inline int width(int i) const {return pixmap[i].width();}
   inline int height(int i) const {return pixmap[i].height();}
   Qt::Orientation _o;
   QPixmap pixmap[3];
   int _thickness;
};

class Nuno
{
public:
   Nuno(int alpha = 255);
   void render(const QRect &rect, QPainter *p, PosFlags pf = Ring) const;
private:
   inline int width(int i) const {return pixmap[i].width();}
   inline int height(int i) const {return pixmap[i].height();}
   QPixmap pixmap[8];
   int _alpha;
   QHash<int, QPixmap> _lines[2];
};

} // namespace Tile

#endif //TILESET_H
