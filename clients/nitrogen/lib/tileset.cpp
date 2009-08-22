/*
 * Copyright 2008 Long Huynh Huu <long.upcase@googlemail.com>
 * Copyright 2007 Matthew Woehlke <mw_triad@users.sourceforge.net>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License version 2 as published by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include <QtGui/QPainter>

#include "tileset.h"

void TileSet::initPixmap(int s, const QPixmap &pix, int w, int h, const QRect &region)
{
    if (w != region.width() || h != region.height()) {
        QPixmap tile = pix.copy(region);
        _pixmap[s] = QPixmap(w, h);
        _pixmap[s].fill(QColor(0,0,0,0));
        QPainter p(&_pixmap[s]);
        p.drawTiledPixmap(0, 0, w, h, tile);
    }
    else {
        _pixmap[s] = pix.copy(region);
    }
}

TileSet::TileSet(const QPixmap &pix, int w1, int h1, int w2, int h2)
    : _w1(w1), _h1(h1)
{
    if (pix.isNull())
        return;

    _w3 = pix.width() - (w1 + w2);
    _h3 = pix.height() - (h1 + h2);
    int w = w2; while (w < 32 && w2 > 0) w += w2;
    int h = h2; while (h < 32 && h2 > 0) h += h2;

    initPixmap(0, pix, _w1, _h1, QRect(0,      0,      _w1, _h1));
    initPixmap(1, pix,  w,  _h1, QRect(_w1,    0,       w2, _h1));
    initPixmap(2, pix, _w3, _h1, QRect(_w1+w2, 0,      _w3, _h1));
    initPixmap(3, pix, _w1,  h,  QRect(0,      _h1,    _w1,  h2));
    initPixmap(4, pix,  w,   h,  QRect(_w1,    _h1,     w2,  h2));
    initPixmap(5, pix, _w3,  h,  QRect(_w1+w2, _h1,    _w3,  h2));
    initPixmap(6, pix, _w1, _h3, QRect(0,      _h1+h2, _w1, _h3));
    initPixmap(7, pix,  w,  _h3, QRect(_w1,    _h1+h2,  w2, _h3));
    initPixmap(8, pix, _w3, _h3, QRect(_w1+w2, _h1+h2, _w3, _h3));
}

TileSet::TileSet(const QPixmap &pix, int w1, int h1, int w3, int h3, int x1, int y1, int w2, int h2)
    : _w1(w1), _h1(h1), _w3(w3), _h3(h3)
{
    if (pix.isNull())
        return;

    int x2 = pix.width() - _w3;
    int y2 = pix.height() - _h3;
    int w = w2; while (w < 32 && w2 > 0) w += w2;
    int h = h2; while (h < 32 && h2 > 0) h += h2;

    initPixmap(0, pix, _w1, _h1, QRect(0,  0,  _w1, _h1));
    initPixmap(1, pix,  w,  _h1, QRect(x1, 0,   w2, _h1));
    initPixmap(2, pix, _w3, _h1, QRect(x2, 0,  _w3, _h1));
    initPixmap(3, pix, _w1,  h,  QRect(0,  y1, _w1,  h2));
    initPixmap(4, pix,  w,   h,  QRect(x1, y1,  w2,  h2));
    initPixmap(5, pix, _w3,  h,  QRect(x2, y1, _w3,  h2));
    initPixmap(6, pix, _w1, _h3, QRect(0,  y2, _w1, _h3));
    initPixmap(7, pix,  w,  _h3, QRect(x1, y2,  w2, _h3));
    initPixmap(8, pix, _w3, _h3, QRect(x2, y2, _w3, _h3));
}

TileSet::TileSet(const TileSet &other)
    : _w1(other._w1), _h1(other._h1), _w3(other._w3), _h3(other._h3)
{
    for (int i=0; i<9; i++) {
        _pixmap[i] = other._pixmap[i];
    }
}

TileSet& TileSet::operator=(const TileSet &other)
{
    _w1 = other._w1;
    _w3 = other._w3;
    _h1 = other._h1;
    _h3 = other._h3;
    for (int i=0; i<9; i++) {
        _pixmap[i] = other._pixmap[i];
    }
    return *this;
}

inline bool bits(TileSet::Tiles flags, TileSet::Tiles testFlags)
{
    return (flags & testFlags) == testFlags;
}

void TileSet::render(const QRect &r, QPainter *p, Tiles t) const
{
    if (_pixmap[0].isNull())
        return;

    int x0, y0, w, h;
    r.getRect(&x0, &y0, &w, &h);
    w -= _w1 + _w3;
    h -= _h1 + _h3;
    int x1 = x0 + _w1;
    int x2 = x1 + w;
    int y1 = y0 + _h1;
    int y2 = y1 + h;

    if (bits(t,    Top|Left))  p->drawPixmap(x0, y0, _pixmap[0]);
    if (bits(t,    Top|Right)) p->drawPixmap(x2, y0, _pixmap[2]);
    if (bits(t, Bottom|Left))  p->drawPixmap(x0, y2, _pixmap[6]);
    if (bits(t, Bottom|Right)) p->drawPixmap(x2, y2, _pixmap[8]);

    if (t & Top)    p->drawTiledPixmap(x1, y0, w, _h1, _pixmap[1]);
    if (t & Bottom) p->drawTiledPixmap(x1, y2, w, _h3, _pixmap[7]);
    if (t & Left)   p->drawTiledPixmap(x0, y1, _w1, h, _pixmap[3]);
    if (t & Right)  p->drawTiledPixmap(x2, y1, _w3, h, _pixmap[5]);

    if (t & Center) p->drawTiledPixmap(x1, y1, w, h, _pixmap[4]);
}
