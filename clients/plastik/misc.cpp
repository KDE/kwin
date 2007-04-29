/* Plastik KWin window decoration
  Copyright (C) 2003 Sandro Giessl <ceebx@users.sourceforge.net>

  based on the window decoration "Web":
  Copyright (C) 2001 Rik Hemsley (rikkus) <rik@kde.org>

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
  the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
  Boston, MA 02110-1301, USA.
 */

#include <kpixmapeffect.h>

#include <QColor>
#include <QImage>
#include <QPainter>

#include "misc.h"

QColor hsvRelative(const QColor& baseColor, int relativeH, int relativeS, int relativeV)
{
    int h, s, v;
    baseColor.getHsv(&h, &s, &v);

    h += relativeH;
    s += relativeS;
    v += relativeV;

    if(h < 0) { h = 0; }
    else if(h > 359) { h = 359; }
    if(s < 0) { s = 0; }
    else if(s > 255) { s = 255; }
    if(v < 0) { v = 0; }
    else if(v > 255) { v = 255; }

    QColor c;
    c.setHsv( h, s, v );
    return c;
}

QColor alphaBlendColors(const QColor &bgColor, const QColor &fgColor, const int a)
{

    // normal button...
    QRgb rgb = bgColor.rgb();
    QRgb rgb_b = fgColor.rgb();
    int alpha = a;
    if(alpha>255) alpha = 255;
    if(alpha<0) alpha = 0;
    int inv_alpha = 255 - alpha;

    QColor result  = QColor( qRgb(qRed(rgb_b)*inv_alpha/255 + qRed(rgb)*alpha/255,
                                  qGreen(rgb_b)*inv_alpha/255 + qGreen(rgb)*alpha/255,
                                  qBlue(rgb_b)*inv_alpha/255 + qBlue(rgb)*alpha/255) );

    return result;
}

QImage recolorImage(QImage *img, QColor color) {
    QImage destImg(img->width(),img->height(), QImage::Format_ARGB32);
    for (int x = 0; x < img->width(); x++) {
        for (int y = 0; y < img->height(); y++) {
            if(img->pixel(x,y) == qRgb(0,0,255) ) {
                destImg.setPixel(x,y,color.rgb() ); // set to the new color
            } else {
                destImg.setPixel(x,y,qRgba(0,0,0,0) ); // set transparent...
            }
        }
    }

    return destImg;
}
