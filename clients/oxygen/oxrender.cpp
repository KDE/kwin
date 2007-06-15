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

#include <QX11Info>
#include <QPainter>
#include <QRegion>
#include <QWidget>
#include <QtCore/QVarLengthArray>
#include <cmath>
#include "oxrender.h"

static Display *dpy = QX11Info::display();
static Window root = RootWindow (dpy, DefaultScreen (dpy));

/* "qt_getClipRects" is friend enough to qregion... ;)*/
inline void *qt_getClipRects( const QRegion &r, int &num )
{
   return r.clipRectangles( num );
}

static OXPicture createFill(Display *dpy, const XRenderColor *c)
{
   XRenderPictureAttributes pa;
   OXPixmap pixmap = XCreatePixmap (dpy, root, 1, 1, 32);
   if (!pixmap)
      return X::None;
   pa.repeat = True;
   OXPicture fill = XRenderCreatePicture (dpy, pixmap, XRenderFindStandardFormat (dpy, PictStandardARGB32), CPRepeat, &pa);
   if (!fill)
   {
      XFreePixmap (dpy, pixmap);
      return X::None;
   }
   XRenderFillRectangle (dpy, PictOpSrc, fill, c, 0, 0, 1, 1);
   XFreePixmap (dpy, pixmap);
   return fill;
}

void OXRender::composite(OXPicture src, OXPicture mask, OXPicture dst,
                         int sx, int sy, int mx, int my, int dx, int dy,
                         uint w, uint h, int op)
{
   XRenderComposite (dpy, op, src, mask, dst, sx, sy, mx, my, dx, dy, w, h);
}
void OXRender::composite(OXPicture src, OXPicture mask, const QPixmap &dst,
                         int sx, int sy, int mx, int my, int dx, int dy,
                         uint w, uint h, int op)
{
   XRenderComposite (dpy, op, src, mask, dst.x11PictureHandle(), sx, sy, mx, my, dx, dy, w, h);
}
void OXRender::composite(const QPixmap &src, OXPicture mask, const QPixmap &dst,
                         int sx, int sy, int mx, int my, int dx, int dy,
                         uint w, uint h, int op)
{
   XRenderComposite (dpy, op, src.x11PictureHandle(), mask, dst.x11PictureHandle(), sx, sy, mx, my, dx, dy, w, h);
}

bool OXRender::blend(const QPixmap &upper, QPixmap &lower, double opacity)
{
   XRenderColor c = {0,0,0, (ushort)(opacity * 0xffff) };
   OXPicture alpha = createFill (dpy, &c);
   if (alpha == X::None)
      return false;
   XRenderComposite (dpy, PictOpOver, upper.x11PictureHandle(), alpha, lower.x11PictureHandle(), 0, 0, 0, 0, 0, 0, upper.width(), upper.height());
   XRenderFreePicture (dpy, alpha);
   return true;
}

QPixmap OXRender::applyAlpha(const QPixmap &toThisPix, const QPixmap &fromThisPix, const QRect &rect, const QRect &alphaRect) {
   return applyAlpha(toThisPix, fromThisPix.x11PictureHandle(), rect, alphaRect);
}

QPixmap OXRender::applyAlpha(const QPixmap &toThisPix, const OXPicture &fromThisPict, const QRect &rect, const QRect &alphaRect) {
   int sx,sy,ax,ay,w,h;
   if (rect.isNull()) {
      sx = sy = 0; w = toThisPix.width(); h = toThisPix.height();
   }
   else
      rect.getRect(&sx,&sy,&w,&h);
   if (alphaRect.isNull()) {
      ax = ay = 0;
   }
   else {
      ax = alphaRect.x(); ay = alphaRect.y();
      w = qMin(alphaRect.width(),w); h = qMin(alphaRect.height(),h);
   }

   QPixmap pix(w,h);
   pix.fill(Qt::transparent);
   XRenderComposite (dpy, PictOpOver, toThisPix.x11PictureHandle(),
                     fromThisPict, pix.x11PictureHandle(),
                     sx, sy, ax, ay, 0, 0, w, h);
   return pix;
}

void OXRender::setAlpha(QPixmap &pix, const OXPicture &alpha)
{
   XRenderPictureAttributes pa;
   pa.alpha_map = alpha;
   pa.alpha_x_origin = pa.alpha_y_origin = 0;
   XRenderChangePicture(dpy, pix.x11PictureHandle(), CPAlphaMap|CPAlphaXOrigin|CPAlphaYOrigin, &pa);
}

QPixmap OXRender::fade(const QPixmap &pix, double percent)
{
   QPixmap newPix(pix.size());
   newPix.fill(Qt::transparent);
   blend(pix, newPix, percent);
   return newPix;
}

void OXRender::setColor(XRenderColor &xc, double r, double g, double b, double a)
{
   xc.red = ushort(r * 0xffff);
   xc.green = ushort(g * 0xffff);
   xc.blue = ushort(b * 0xffff);
   xc.alpha = ushort(a * 0xffff);
}

void OXRender::setColor(XRenderColor &xc, QColor qc)
{
   xc.red = qc.red()*0x101; xc.green = qc.green()*0x101;
   xc.blue = qc.blue()*0x101; xc.alpha = qc.alpha()*0x101;
}

void OXRender::setGradient(XLinearGradient &lg, QPoint p1, QPoint p2)
{
   lg.p1.x = p1.x(); lg.p1.y = p1.y();
   lg.p2.x = p2.x(); lg.p2.y = p2.y();
}

void OXRender::setGradient(XLinearGradient &lg, XFixed x1, XFixed y1, XFixed x2, XFixed y2)
{
   lg.p1.x = x1; lg.p1.y = y1;
   lg.p2.x = x2; lg.p2.y = y2;
}

OXPicture OXRender::gradient(const QPoint start, const QPoint stop, const ColorArray &colors, const PointArray &stops)
{
   XLinearGradient lg = {
      { start.x() << 16, start.y() << 16 },
      { stop.x() << 16, stop.y() << 16} };
   QVarLengthArray<XRenderColor> cs(colors.size());
   for (int i = 0; i < colors.size(); ++i)
      setColor(cs[i], colors.at(i));
   XFixed *stps;
   if (stops.size() < 2)
   {
      stps = new XFixed[2];
      stps[0] = 0; stps[1] = (1<<16);
   }
   else
   {
      int d = (1<<16);
      stps = new XFixed[stops.size()];
      for (int i = 0; i < stops.size(); ++i)
      {
         if (stops.at(i) < 0) continue;
         if (stops.at(i) > 1) break;
         stps[i] = stops.at(i)*d;
      }
   }
   XFlush (dpy);
   OXPicture lgp = XRenderCreateLinearGradient(dpy, &lg, stps, &cs[0], qMin(qMax(stops.size(),2),colors.size()));
   delete[] stps;
   return lgp;
}

OXPicture OXRender::gradient(const QPoint c1, int r1, const QPoint c2, int r2, const ColorArray &colors, const PointArray &stops)
{
   XRadialGradient rg = {
      { c1.x() << 16, c1.y() << 16, r1 << 16 },
      { c2.x() << 16, c2.y() << 16, r2 << 16 } };
   QVarLengthArray<XRenderColor> cs(colors.size());
   for (int i = 0; i < colors.size(); ++i)
      setColor(cs[i], colors.at(i));
   XFixed *stps;
   if (stops.size() < 2)
   {
      stps = new XFixed[2];
      stps[0] = 0; stps[1] = (1<<16);
   }
   else
   {
      int d = ((int)(sqrt(pow(c2.x()-c1.x(),2)+pow(c2.y()-c1.y(),2)))) << 16;
      stps = new XFixed[stops.size()];
      for (int i = 0; i < stops.size(); ++i)
      {
         if (stops.at(i) < 0) continue;
         if (stops.at(i) > 1) break;
         stps[i] = stops.at(i)*d;
      }
   }
   XFlush (dpy);
   OXPicture lgp = XRenderCreateRadialGradient(dpy, &rg, stps, &cs[0], qMin(qMax(stops.size(),2),colors.size()));
   delete[] stps;
   return lgp;
}

void OXRender::freePicture(OXPicture pict)
{
   XRenderFreePicture (dpy, pict);
}
