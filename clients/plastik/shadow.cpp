/* Copyright (C) 2003-2005 by Sandro Giessl
 *
 * based on the text shadow code from KHTML. Original copyright notes:
 *
 * This file is part of the DOM implementation for KDE.
 *
 * Copyright (C) 1999-2003 Lars Knoll (knoll@kde.org)
 *           (C) 2000-2003 Dirk Mueller (mueller@kde.org)
 *           (C) 2003 Apple Computer, Inc.
 *           (C) 2004-2005 Allan Sandfeld Jensen (kde@carewolf.com)
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include "shadow.h"
#include <qcolor.h>
#include <qimage.h>
#include <qpainter.h>
#include <qpixmap.h>

#ifdef HAVE_ALLOCA_H
// explicitly included for systems that don't provide it in stdlib.h
#include <alloca.h>
#else
#include <stdlib.h>
#endif

void paintShadow(QPainter *pt, const QPixmap& textPixmap, int x, int y, int thickness, const QColor &c)
{
    const QRgb color = c.rgb();
    const int fgGray = qGray(qRgb(255,255,255) );
    const QRgb bgColor = qRgb(0,0,0);

    const int w = textPixmap.width();
    const int h = textPixmap.height();

    QImage img = textPixmap.convertToImage().convertDepth(32);

    int md = thickness*thickness; // max-dist²

    // blur map (division cache)
    float *bmap = (float*)alloca(sizeof(float)*(md+1));
    for(int n=0; n<=md; n++) {
        float f;
        f = n/(float)(md+1);
        f = 1.0 - f*f;
        bmap[n] = f;
    }

    float factor = 0.0; // maximal opacity-sum
    for(int n=-thickness; n<=thickness; n++) {
        for(int m=-thickness; m<=thickness; m++) {
            int d = n*n+m*m;
            if (d<=md) {
                factor += bmap[d];
            }
        }
    }

    factor = 1.0/factor;

    // alpha map
    float* amap = (float*)alloca(sizeof(float)*(h*w));
    memset(amap, 0, h*w*(sizeof(float)));
    for(int j=thickness; j<h-thickness; j++) {
        for(int i=thickness; i<w-thickness; i++) {
            QRgb col= img.pixel(i,j);
            if (col == bgColor) continue;
            float g = qGray(col);
            g = g/fgGray;
            for(int n=-thickness; n<=thickness; n++) {
                for(int m=-thickness; m<=thickness; m++) {
                    int d = n*n+m*m;
                    if (d>md) continue;
                    float f = bmap[d];
                    amap[(i+m)+(j+n)*w] += (g*f);
                }
            }
        }
    }

    QImage res(w,h,32);
    res.setAlphaBuffer(true);
    int r = qRed(color);
    int g = qGreen(color);
    int b = qBlue(color);

    for(int j=0; j<h; j++) {
        for(int i=0; i<w; i++) {
            res.setPixel(i,j, qRgba(r,g,b,(int)(amap[i+j*w]*factor*255.0)));
        }
    }

    pt->drawImage(x,y, res);
}

