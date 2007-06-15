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

#ifndef OXRENDER_H
#define OXRENDER_H

#include <QPixmap>
#include <QVector>
#include <X11/Xlib.h>
#include <X11/extensions/Xrender.h>
#include <fixx11h.h>

typedef QVector<double> PointArray;
typedef QVector<QColor> ColorArray;
typedef Picture OXPicture;
typedef Pixmap OXPixmap;

class OXRender
{
// public:
//    OXRender(){}
public:
   static void composite(OXPicture src, OXPicture mask, OXPicture dst, int sx, int sy, int mx, int my, int dx, int dy, uint w, uint h, int op = PictOpSrc);
   static void composite(OXPicture src, OXPicture mask, const QPixmap &dst, int sx, int sy, int mx, int my, int dx, int dy, uint w, uint h, int op = PictOpSrc);
   static void composite(const QPixmap &src, OXPicture mask, const QPixmap &dst, int sx, int sy, int mx, int my, int dx, int dy, uint w, uint h, int op = PictOpSrc);
   static bool blend(const QPixmap &upper, QPixmap &lower, double opacity = 0.5);
   static QPixmap fade(const QPixmap &pix, double percent);
   static void setColor(XRenderColor &xc, double r, double g, double b, double a = 1);
   static void setColor(XRenderColor &xc, QColor qc);
   static void setAlpha(QPixmap &pix, const OXPicture &mask);
   static QPixmap applyAlpha(const QPixmap &toThisPix, const QPixmap &fromThisPix,
                             const QRect &rect = QRect(), const QRect &alphaRect = QRect());
   static QPixmap applyAlpha(const QPixmap &toThisPix, const OXPicture &fromThisPict,
                             const QRect &rect = QRect(), const QRect &alphaRect = QRect());
   static void setGradient(XLinearGradient &lg, QPoint p1, QPoint p2);
   static void setGradient(XLinearGradient &lg, XFixed x1, XFixed y1, XFixed x2, XFixed y2);
   static OXPicture gradient(const QPoint start, const QPoint stop, const ColorArray &colors, const PointArray &stops = PointArray());
   static OXPicture gradient(const QPoint c1, int r1, const QPoint c2, int r2, const ColorArray &colors, const PointArray &stops = PointArray());
   static void freePicture(OXPicture pict);
};

#define Q2XRenderColor(_XRC_, _QC_) XRenderColor _XRC_; OXRender::setColor(_XRC_, _QC_)

#endif //OXRENDER_H
