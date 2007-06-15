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

#include <QPainter>
#include <cmath>
#include "oxrender.h"
#include "tileset.h"

#ifndef MIN
#define MIN(x,y) ((x) < (y) ? (x) : (y))
#endif

#ifndef MAX
#define MAX(x,y) ((x) > (y) ? (x) : (y))
#endif

using namespace Tile;

static QPixmap nullPix;

static bool isEmpty(const QPixmap &pix)
{
   if (!pix.hasAlpha()) return false;
   QImage img =  pix.toImage();
   uint *data = ( uint * ) img.bits();
   int total = img.width() * img.height();
   for ( int current = 0 ; current < total ; ++current )
      if (qAlpha(data[ current ]))
         return false;
   return true;
}

static QPixmap invertAlpha(const QPixmap & pix)
{
   if (pix.isNull()) return pix;
   QImage img =  pix.toImage();
   QImage *dst = new QImage(img);
   uint *data = ( uint * ) img.bits();
   uint *ddata = ( uint * ) dst->bits();
   int total = img.width() * img.height();
   for ( int c = 0 ; c < total ; ++c )
      ddata[c] = qRgba( qRed(data[c]), qGreen(data[c]), qBlue(data[c]), 255-qAlpha(data[c]) );
   QPixmap ret = QPixmap::fromImage(*dst, 0);
   delete dst;
   return ret;
}

Set::Set(const QPixmap &pix, int xOff, int yOff, int width, int height, int rx, int ry)
{
   if (pix.isNull())
   {
      _isBitmap = false;
      return;
   }
   _isBitmap = pix.isQBitmap();
   rxf = pix.width()*rx;
   ryf = pix.height()*ry;
   
   int rOff = pix.width() - xOff - width;
   int bOff = pix.height() - yOff - height;
   int amount = 32/width+1;
   int amount2 = 32/height+1;
   int i;
   
   QPainter p;
   
#define initPixmap(_SECTION_,_WIDTH_,_HEIGHT_)\
   pixmap[_SECTION_] = QPixmap(_WIDTH_, _HEIGHT_);\
   pixmap[_SECTION_].fill(Qt::transparent); p.begin(&pixmap[_SECTION_])
      
#define finishPixmap(_SECTION_)\
   p.end();\
   if (isEmpty(pixmap[_SECTION_]))\
      pixmap[_SECTION_] = QPixmap()
   
   initPixmap(TopLeft, xOff, yOff);
   p.drawPixmap(0, 0, pix, 0, 0, xOff, yOff);
   finishPixmap(TopLeft);
   
   initPixmap(TopMid, amount*width, yOff);
   for (i = 0; i < amount; i++)
      p.drawPixmap(i*width, 0, pix, xOff, 0, width, yOff);
   finishPixmap(TopMid);
   
   initPixmap(TopRight, rOff, yOff);
   p.drawPixmap(0, 0, pix, xOff+width, 0, rOff, yOff);
   finishPixmap(TopRight);
   
   //----------------------------------
   initPixmap(MidLeft, xOff, amount2*height);
   for (i = 0; i < amount2; i++)
      p.drawPixmap(0, i*height, pix, 0, yOff, xOff, height);
   finishPixmap(MidLeft);
   
   initPixmap(MidMid, amount*width, amount2*height);
   for (i = 0; i < amount; i++)
      for (int j = 0; j < amount2; j++)
         p.drawPixmap(i*width, j*height, pix, xOff, yOff, width, height);
   finishPixmap(MidMid);
   
   initPixmap(MidRight, rOff, amount2*height);
   for (i = 0; i < amount2; i++)
      p.drawPixmap(0, i*height, pix, xOff+width, yOff, rOff, height);
   finishPixmap(MidRight);
   
   //----------------------------------
   
   initPixmap(BtmLeft, xOff, bOff);
   p.drawPixmap(0, 0, pix, 0, yOff+height, xOff, bOff);
   finishPixmap(BtmLeft);
   
   initPixmap(BtmMid, amount*width, bOff);
   for (i = 0; i < amount; i++)
      p.drawPixmap(i*width, 0, pix, xOff, yOff+height, width, bOff);
   finishPixmap(BtmMid);
   
   initPixmap(BtmRight, rOff, bOff);
   p.drawPixmap(0, 0, pix, xOff+width, yOff+height, rOff, bOff);
   finishPixmap(BtmRight);
   
#undef initPixmap
#undef finishPixmap
}

QRect Set::rect(const QRect &rect, PosFlags pf) const
{
   QRect ret = rect;
   if (pf == Center)
      ret.adjust(width(MidLeft),height(TopMid),-width(TopMid),-height(BtmMid));
   else if (pf == Left)
      ret.setRight(ret.left()+width(MidLeft));
   else if (pf == Top)
      ret.setBottom(ret.top()+height(TopMid));
   else if (pf == Right)
      ret.setLeft(ret.right()-width(MidRight));
   else if (pf == Bottom)
      ret.setTop(ret.bottom()-height(BtmMid));
   return ret;
}

void Set::render(const QRect &r, QPainter *p, PosFlags pf) const
{
#define PIXMAP(_TILE_) pixmap[_TILE_]
#define DRAW_PIXMAP p->drawPixmap
#define DRAW_TILED_PIXMAP p->drawTiledPixmap

#include "t_render1.cpp"
#undef PIXMAP
#undef DRAW_PIXMAP
#undef DRAW_TILED_PIXMAP
}

static Window root = RootWindow (QX11Info::display(), DefaultScreen (QX11Info::display()));

Picture Set::render(const QSize &s, PosFlags pf) const
{
   return render(s.width(), s.height(), pf);
}

Picture Set::render(int W, int H, PosFlags pf) const
{
   Display *dpy = QX11Info::display();
   QRect r(0,0,W,H);
   Pixmap xpix = XCreatePixmap (dpy, root, W, H, 32);
   if (!pixmap)
      return X::None;
   Picture pict =
      XRenderCreatePicture (dpy, xpix,
                            XRenderFindStandardFormat(dpy, PictStandardARGB32),
                            0, 0);
   if (!pict) {
      XFreePixmap (dpy, xpix); return X::None;
   }
   
#define PIXMAP(_TILE_) pixmap[_TILE_]
#define DRAW_PIXMAP(_DX_, _DY_, _PIX_, _SX_, _SY_, _W_, _H_) \
   XRenderComposite(dpy, PictOpSrc, _PIX_.x11PictureHandle(), X::None, pict,\
      _SX_, _SY_, 0, 0, _DX_, _DY_, _W_, _H_);
//    XCopyArea( dpy, _PIX_.handle(), pix->handle(), gc, _SX_, _SY_, _W_, _H_, _DX_, _DY_ )
#define DRAW_TILED_PIXMAP(_DX_, _DY_, _W_, _H_, _PIX_)\
      for (int x = 0; x < _W_; x += _PIX_.width())\
         for (int y = 0; y < _H_; y += _PIX_.height())\
            XRenderComposite(dpy, PictOpSrc, _PIX_.x11PictureHandle(), X::None,\
                             pict, 0, 0, 0, 0, x+_DX_, y+_DY_,\
                             MIN(_PIX_.width(), _W_-x),\
                             MIN(_PIX_.height(), _H_-y));

#include "t_render1.cpp"
   XFreePixmap (dpy, xpix);
   return pict;
#undef PIXMAP
#undef DRAW_PIXMAP
#undef DRAW_TILED_PIXMAP
}

void Set::outline(const QRect &r, QPainter *p, QColor c, bool strong, PosFlags pf) const
{
   p->save();
   p->setRenderHint(QPainter::Antialiasing, false);
   QPen pen = p->pen();
   pen.setColor(c); pen.setWidth(1);
   p->setPen(pen);
   p->setBrush(Qt::NoBrush);
   QRect rect = r/*.adjusted(0,0,0,-1)*/;
   if (! (pf & Top))
      rect.setTop(rect.top()-100);
   else if (strong)
      p->drawLine(r.left()+width(TopLeft), r.top(), r.right()-width(TopRight), r.top());
   if (! (pf & Left))
      rect.setLeft(rect.left()-100);
   else if (strong)
      p->drawLine(r.left(), r.top()+height(TopRight), r.left(), r.bottom()-height(BtmRight));
   if (! (pf & Bottom))
      rect.setBottom(rect.bottom()+100);
   else if (strong)
      p->drawLine(r.left()+width(BtmLeft), r.bottom(), r.right()-width(BtmRight), r.bottom());
   if (! (pf & Right))
      rect.setRight(rect.right()+100);
   else if (strong)
      p->drawLine(r.right(), r.top()+height(TopRight), r.right(), r.bottom()-height(BtmRight));
   p->setClipRect(r);
   p->setRenderHint(QPainter::Antialiasing);
   p->drawRoundRect(rect, ceil((float)rxf/rect.width()), ceil((float)ryf/rect.height()));
   p->restore();
}

Tile::Mask::Mask(const QPixmap &pix, int xOff, int yOff, int width, int height,
          int dx1, int dy1, int dx2, int dy2, int rx, int ry):
Set(pix, xOff, yOff, width, height, rx, ry) {
   _dx[0] = dx1; _dx[1] = dx2; _dy[0] = dy1; _dy[1] = dy2;
   
   pixmap[MidMid] = QPixmap();
   if (dx1 < 1) pixmap[MidLeft] = QPixmap();
   if (dx2 > -1) pixmap[MidRight] = QPixmap();
   if (dy1 < 1) pixmap[TopMid] = QPixmap();
   if (dy2 > -1) pixmap[BtmMid] = QPixmap();
   
   for (int i = 0; i < 9; ++i)
      if (i != MidMid)
      inversePixmap[i] = invertAlpha(pixmap[i]);
      
   _hasCorners = !pix.isNull();
}

QRect Tile::Mask::bounds(const QRect &rect, PosFlags pf) const
{
   QRect ret = rect;
   if (pf & Left)
      ret.setLeft(ret.left()+_dx[0]);
   if (pf & Top)
      ret.setTop(ret.top()+_dy[0]);
   if (pf & Right)
      ret.setRight(ret.right()+_dx[1]);
   if (pf & Bottom)
      ret.setBottom(ret.bottom()+_dy[1]);
   return ret;
}

const QPixmap &Tile::Mask::corner(PosFlags pf, bool inverse) const
{
   const QPixmap (*pics)[9] = inverse ? &inversePixmap : &pixmap;
   if (pf == (Top | Left))
      return (*pics)[TopLeft];
   if (pf == (Top | Right))
      return (*pics)[TopRight];
   if (pf == (Bottom | Right))
      return (*pics)[BtmRight];
   if (pf == (Bottom | Left))
      return (*pics)[BtmLeft];

   qWarning("requested impossible corner %d",pf);
   return nullPix;
}

QRegion Tile::Mask::clipRegion(const QRect &rect, PosFlags pf) const
{
   QRegion ret(rect.adjusted(_dx[0], _dy[0], _dx[1], _dy[1]));
   int w,h;
   if (matches(Top | Left, pf)) {
      ret -= QRect(rect.x(), rect.y(), width(TopLeft), height(TopLeft));
   }
   if (matches(Top | Right, pf)) {
      w = width(TopRight);
      ret -= QRect(rect.right()-w+1, rect.y(), w, height(TopRight));
   }
   if (matches(Bottom | Left, pf)) {
      h = height(BtmLeft);
      ret -= QRect(rect.x(), rect.bottom()-h+1, width(BtmLeft), h);
   }
   if (matches(Bottom | Right, pf)) {
      w = width(BtmRight); h = height(BtmRight);
      ret -= QRect(rect.right()-w+1, rect.bottom()-h+1, w, h);
   }
   return ret;
}

void Tile::Mask::render(const QRect &r, QPainter *p, const QBrush &fill,
                  PosFlags oPf, bool justClip, QPoint offset, bool inverse,
                  const QRect *outerRect) const
{
   bool pixmode = !fill.texture().isNull();

   if (justClip) {
      p->save();
      p->setClipRegion(clipRegion(r, oPf));
      if (pixmode)
         p->drawTiledPixmap(r, fill.texture(), offset);
      else
         p->fillRect(r, fill.color());
      p->restore();
      return;
   }
   // first the inner region
   //NOTE: using full alphablend can become enourmously slow due to VRAM size -
   // even on HW that has Render acceleration!
   if (!inverse || outerRect) {
      p->save();
      if (inverse) {
         QRegion cr = *outerRect; cr -= r;
         p->setClipRegion(cr, Qt::IntersectClip);
      }
      else
         p->setClipRegion(clipRegion(r, oPf), Qt::IntersectClip);
      if (pixmode)
         p->drawTiledPixmap(outerRect ? *outerRect : r, fill.texture(), offset);
      else
         p->fillRect(outerRect ? *outerRect : r, fill.color());
      p->restore();
   }
   if (!_hasCorners)
      return;
   
   QPixmap filledPix;
   QPainter pixPainter;
   PosFlags pf = oPf & ~Center;
   const QPixmap (*pics)[9] = inverse ? &inversePixmap : &pixmap;
   QPoint off = r.topLeft()-offset;
   
#define PIXMAP(_TILE_) (*pics)[_TILE_]
   
#define MAKE_FILL(_PIX_, _OFF_)\
   filledPix = QPixmap(_PIX_.size());\
   if (pixmode) {\
      filledPix.fill(Qt::transparent);\
      pixPainter.begin(&filledPix);\
      pixPainter.drawTiledPixmap(filledPix.rect(), fill.texture(), _OFF_-off);\
      pixPainter.end();\
   }\
   else\
      filledPix.fill(fill.color());\
   filledPix = OXRender::applyAlpha(filledPix, _PIX_)
   
#define DRAW_PIXMAP(_DX_, _DY_, _PIX_, _SX_, _SY_, _W_, _H_) {\
   MAKE_FILL(_PIX_, QPoint(_DX_, _DY_));\
   p->drawPixmap(_DX_, _DY_, filledPix, _SX_, _SY_, _W_, _H_);\
}// --skip semicolon
   
#define DRAW_TILED_PIXMAP(_DX_, _DY_, _W_, _H_, _PIX_) {\
   MAKE_FILL(_PIX_, QPoint(_DX_, _DY_));\
   p->drawTiledPixmap(_DX_, _DY_, _W_, _H_, filledPix);\
}// --skip semicolon

#include "t_render1.cpp"
#undef PIXMAP
#undef MAKE_FILL
#undef DRAW_PIXMAP
#undef DRAW_TILED_PIXMAP

}

Line::Line(const QPixmap &pix, Qt::Orientation o, int d1, int d2) {
   _o = o;
   QPainter p;
   if (o == Qt::Horizontal) {
      _thickness = pix.height();
      pixmap[0] = QPixmap(d1,pix.height());
      pixmap[0].fill(Qt::transparent);
      p.begin(&pixmap[0]);
      p.drawPixmap(0,0,pix,0,0,d1,pix.height());
      p.end();
      
      int d = pix.width()-d1+d2;
      pixmap[1] = QPixmap(MAX(32,d),pix.height());
      pixmap[1].fill(Qt::transparent);
      p.begin(&pixmap[1]);
      for (int i = 0; i+d <= width(1); i+=d)
         p.drawPixmap(i,0,pix,d1,0,MIN(d,width(1)-i),pix.height());
      p.end();
      
      pixmap[2] = QPixmap(-d2,pix.height());
      pixmap[2].fill(Qt::transparent);
      p.begin(&pixmap[2]);
      p.drawPixmap(0,0,pix,pix.width()+d2,0,-d2,pix.height());
      p.end();
   }
   else {
      _thickness = pix.width();
      pixmap[0] = QPixmap(pix.width(),d1);
      pixmap[0].fill(Qt::transparent);
      p.begin(&pixmap[0]);
      p.drawPixmap(0,0,pix,0,0,pix.width(),d1);
      p.end();
      
      int d = pix.height()-d1+d2;
      pixmap[1] = QPixmap(pix.width(), MAX(32,d));
      pixmap[1].fill(Qt::transparent);
      p.begin(&pixmap[1]);
      for (int i = 0; i+d < height(1); i+=d)
         p.drawPixmap(0,i,pix,0,d1,pix.width(),MIN(d,height(1)-i));
      p.end();
      
      pixmap[2] = QPixmap(pix.width(),-d2);
      pixmap[2].fill(Qt::transparent);
      p.begin(&pixmap[2]);
      p.drawPixmap(0,0,pix,0,pix.height()+d2,pix.width(),-d2);
      p.end();
   }
}

void Line::render(const QRect &rect, QPainter *p, PosFlags pf, bool btmRight) const {
   int d0,d2;
   if (_o == Qt::Horizontal) {
      int y = btmRight?rect.bottom()-_thickness-1:rect.y();
      d0 = (pf & Left) ? width(0) : 0;
      d2 = (pf & Right) ? width(2) : 0;
      if ((pf & Center) && rect.width() >= d0+d2)
         p->drawTiledPixmap(rect.x()+d0,y, rect.width()-d0-d2, height(1), pixmap[1]);
      else if (d0 || d2) {
         d0 = qMin(d0,d0*rect.width()/(d0+d2));
         d2 = qMin(d2,rect.width()-d0);
      }
      if (pf & Left)
         p->drawPixmap(rect.x(),y, pixmap[0],0,0,d0,height(0));
      if (pf & Right)
         p->drawPixmap(rect.right()+1-d2,y, pixmap[2], width(2)-d2,0,d2,height(2));
   }
   else {
      int x = btmRight?rect.right()-_thickness-1:rect.x();
      d0 = (pf & Top) ? height(0) : 0;
      d2 = (pf & Bottom) ? height(2) : 0;
      if ((pf & Center) && rect.height() >= d0+d2) {
         p->drawTiledPixmap(x,rect.y()+d0, width(1), rect.height()-d0-d2, pixmap[1]);
      }
      else if (d0 || d2) {
         d0 = qMin(d0,d0*rect.height()/(d0+d2));
         d2 = qMin(d2,rect.height()-d0);
      }
      if (pf & Top)
         p->drawPixmap(x,rect.y(),pixmap[0],0,0,width(0),d0);
      if (pf & Bottom)
         p->drawPixmap(x, rect.bottom()+1-d2,pixmap[2],0,height(2)-d2,width(2),d2);
   }
}

#define FILL_PIX(_NUM_)\
   pixmap[_NUM_].fill(Qt::transparent);\
   p.begin(&pixmap[_NUM_]);\
   p.fillRect(pixmap[_NUM_].rect(), lg);\
   p.end()

#define MAKE_ARC(_NUM_,_CLR_,_X_,_Y_,_ANG_)\
      pixmap[_NUM_] = QPixmap(4,4);\
      pixmap[_NUM_].fill(Qt::transparent);\
      p.begin(&pixmap[_NUM_]);\
      p.setRenderHint(QPainter::Antialiasing);\
      p.setPen(_CLR_);\
      p.drawArc(_X_,_Y_,8,8,_ANG_<<4,90<<4);\
      p.setRenderHint(QPainter::Antialiasing, false);\
      p.end()

#define SET_GRAD(_C1_,_C2_)\
   stops.clear();\
   stops << QGradientStop(0, QColor(_C1_,_C1_,_C1_,_alpha))\
         << QGradientStop(1, QColor(_C2_,_C2_,_C2_,_alpha));\
   lg.setStops(stops)

#ifndef CLAMP
#define CLAMP(x,l,u) (x) < (l) ? (l) :\
(x) > (u) ? (u) :\
(x)
#endif

Nuno::Nuno(int alpha)
{
   QGradientStops stops;
   _alpha = alpha;
   QPainter p; QPen pen = p.pen();
   MAKE_ARC(0,QColor(255,255,255,_alpha),0,0,90); //tl
   QLinearGradient lg( 0, 0, 4, 4 );
   SET_GRAD(255,170); pen.setBrush(lg);
   MAKE_ARC(1,pen,-4,0,0); //tr
   lg.setStart(4,0); lg.setFinalStop(0,4);
   SET_GRAD(107,100); pen.setBrush(lg);
   MAKE_ARC(2,pen,-4,-4,270); //br
   lg.setStart(0,0); lg.setFinalStop(4,4);
   SET_GRAD(255,175); pen.setBrush(lg);
   MAKE_ARC(3,pen,0,-4,180); //bl

   _alpha = alpha;
   
   lg.setStart(0,0); lg.setFinalStop(37,0);
   
   //bottom 1
   SET_GRAD(175,138);
   pixmap[4] = QPixmap(37,1);
   FILL_PIX(4);

   //bottom 2
   SET_GRAD(136,100);
   pixmap[5] = QPixmap(37,1);
   FILL_PIX(5);

   //right 1
   lg.setStart(1,0); lg.setFinalStop(1,31);
   SET_GRAD(170,139);
   pixmap[6] = QPixmap(1,31);
   FILL_PIX(6);

   //right 2
   SET_GRAD(137,107);
   pixmap[7] = QPixmap(1,31);
   FILL_PIX(7);
}

#undef FILL_PIX
#undef MAKE_ARC

void Nuno::render(const QRect &r, QPainter *p, PosFlags pf) const
{
   int xOff = 0, yOff = 0, rOff = 0, bOff = 0;
   
   if (matches(Top|Left, pf))
   {
      p->drawPixmap(r.x(),r.y(),pixmap[0]);
      xOff = r.x()+width(0); yOff = r.y()+height(0);
   }
   
   if (matches(Top|Right, pf))
   {
      p->drawPixmap(r.right()-width(1)+1,r.y(),pixmap[1]);
      rOff = r.right()-width(1)+1; yOff = r.y()+height(1);
   }
   
   if (matches(Bottom|Left, pf))
   {
      p->drawPixmap(r.x(),r.bottom()-height(3)+1,pixmap[3]);
      xOff = r.x()+width(3); bOff = r.bottom()-height(3)+1;
   }
   
   if (matches(Bottom|Right, pf))
   {
      p->drawPixmap(r.right()-width(2)+1,r.bottom()-height(2)+1,pixmap[2]);
      rOff = r.right()-width(2)+1; bOff = r.bottom()-height(2)+1;
   }
   p->save();
   if (xOff < rOff)
   {
      if (pf & Top)
      {
         p->setPen(QColor(255,255,255,_alpha));
         p->drawLine(xOff, r.y(), rOff, r.y());
      }
      if (pf & Bottom)
      {
         int w = rOff-xOff;
         if (w < width(4)+width(5))
         {
            QHash<int, QPixmap> *cache = &(const_cast<Nuno*>( this )->_lines[0]);
            QHash<int, QPixmap>::const_iterator it;
            it = cache->find(w);
            if (it == cache->end())
            {
               QPixmap pix(w,1); pix.fill(Qt::transparent);
               QLinearGradient lg( 0, 1, w, 1 );
               lg.setColorAt ( 0, QColor(175,175,175,_alpha) );
               lg.setColorAt ( 1, QColor(100,100,100,_alpha) );
               QPainter p(&pix); p.fillRect(0,0,w,1,lg); p.end();
               it = cache->insert(w, pix);
            }
            p->drawPixmap(xOff,r.bottom(),it.value());
         }
         else
         {
            p->drawPixmap(xOff,r.bottom(),pixmap[4]);
            p->drawPixmap(rOff-width(5),r.bottom(),pixmap[5]);
            if (w > width(4)+width(5))
            {
               p->setPen(QColor(137,137,137,_alpha));
               p->drawLine(xOff+width(4),r.bottom(),rOff-width(5)-1,r.bottom());
            }
         }
      }
   }
   if (yOff < bOff)
   {
      if (pf & Left)
      {
         p->setPen(QColor(255,255,255,_alpha));
         p->drawLine(r.x(), yOff, r.x(), bOff);
      }
      if (pf & Right)
      {
         int h = bOff-yOff;
         if (h < height(6)+height(7))
         {
            QHash<int, QPixmap> *cache = &(const_cast<Nuno*>( this )->_lines[1]);
            QHash<int, QPixmap>::const_iterator it;
            it = cache->find(h);
            if (it == cache->end())
            {
               QPixmap pix(1,h);
               QLinearGradient lg( 1, 0, 1, h ); pix.fill(Qt::transparent);
               lg.setColorAt ( 0, QColor(170,170,170,_alpha) );
               lg.setColorAt ( 1, QColor(107,107,107,_alpha) );
               QPainter p(&pix); p.fillRect(0,0,1,h,lg); p.end();
               it = cache->insert(h, pix);
            }
            p->drawPixmap(r.right(),yOff,it.value());
         }
         else
         {
            p->drawPixmap(r.right(),yOff,pixmap[6]);
            p->drawPixmap(r.right(),bOff-height(7),pixmap[7]);
            if (h > height(6)+height(7))
            {
               p->setPen(QColor(138,138,138,_alpha));
               p->drawLine(r.right(),yOff+height(6),r.right(),bOff-height(7)-1);
            }
         }
      }
   }
   p->restore();
}
