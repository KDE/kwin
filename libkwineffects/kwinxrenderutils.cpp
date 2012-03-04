/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2008 Lubos Lunak <l.lunak@kde.org>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*********************************************************************/

#include "kwinxrenderutils.h"
#include "kwinglobals.h"

#ifdef KWIN_HAVE_XRENDER_COMPOSITING

#include <QStack>
#include <QVector>
#include <QPixmap>
#include <QPainter>
#include <kdebug.h>

namespace KWin
{

// Convert QRegion to XserverRegion. All code uses XserverRegion
// only when really necessary as the shared implementation uses
// QRegion.
XserverRegion toXserverRegion(QRegion region)
{
    QVector< QRect > rects = region.rects();
    XRectangle* xr = new XRectangle[ rects.count()];
    for (int i = 0;
            i < rects.count();
            ++i) {
        xr[ i ].x = rects[ i ].x();
        xr[ i ].y = rects[ i ].y();
        xr[ i ].width = rects[ i ].width();
        xr[ i ].height = rects[ i ].height();
    }
    XserverRegion ret = XFixesCreateRegion(display(), xr, rects.count());
    delete[] xr;
    return ret;
}

// adapted from Qt, because this really sucks ;)
XRenderColor preMultiply(const QColor &c, float opacity)
{
    XRenderColor color;
    const uint A = c.alpha() * opacity,
               R = c.red(),
               G = c.green(),
               B = c.blue();
    color.alpha = (A | A << 8);
    color.red   = (R | R << 8) * color.alpha / 0x10000;
    color.green = (G | G << 8) * color.alpha / 0x10000;
    color.blue  = (B | B << 8) * color.alpha / 0x10000;
    return color;
}

XRenderPicture xRenderFill(const XRenderColor *xc)
{
    Pixmap pixmap = XCreatePixmap(display(), rootWindow(), 1, 1, 32);
    XRenderPictureAttributes pa; pa.repeat = True;
    XRenderPicture fill(pixmap, 32);
    XFreePixmap(display(), pixmap);
    XRenderChangePicture(display(), fill, CPRepeat, &pa);
    XRenderFillRectangle(display(), PictOpSrc, fill, xc, 0, 0, 1, 1);
    return fill;
}

XRenderPicture xRenderFill(const QColor &c)
{
    XRenderColor xc = preMultiply(c);
    return xRenderFill(&xc);
}

static XRenderPicture _blendPicture = X::None;
static XRenderColor _blendColor;
XRenderPicture xRenderBlendPicture(double opacity)
{
    _blendColor.alpha = ushort(opacity * 0xffff);
    if (_blendPicture == X::None)
        _blendPicture = xRenderFill(&_blendColor);
    else
        XRenderFillRectangle(display(), PictOpSrc, _blendPicture, &_blendColor, 0, 0, 1, 1);
    return _blendPicture;
}


static XRenderPicture *_circle[4] = {NULL, NULL, NULL, NULL};

#define DUMP_CNR(_SECT_, _W_, _H_, _XOFF_, _YOFF_)\
    dump = QPixmap(_W_, _H_);\
    dump.fill(Qt::transparent);\
    p.begin(&dump);\
    p.drawPixmap( 0, 0, tmp, _XOFF_, _YOFF_, _W_, _H_ );\
    p.end();\
    _circle[_SECT_] = new XRenderPicture(dump);

#define CS 8

static XRenderPicture *circle(int i)
{
    if (!_circle[0]) {
        QPixmap tmp(2 * CS, 2 * CS);
        tmp.fill(Qt::transparent);
        QPainter p(&tmp);
        p.setRenderHint(QPainter::Antialiasing);
        p.setPen(Qt::NoPen); p.setBrush(Qt::black);
        p.drawEllipse(tmp.rect());
        p.end();
        QPixmap dump;
        DUMP_CNR(0, CS, CS, 0, 0);
        DUMP_CNR(1, CS, CS, CS, 0);
        DUMP_CNR(2, CS, CS, CS, CS);
        DUMP_CNR(3, CS, CS, 0, CS);
    }
    return _circle[i];
}

void xRenderRoundBox(Picture pict, const QRect &rect, int , const QColor &c)
{
    XRenderPicture fill = xRenderFill(c);
    int op = c.alpha() == 255 ? PictOpSrc : PictOpOver;
    // TODO: implement second paramenter "roundness"
    // so rather use ?? XRenderCompositeTriFan (dpy, op, src, dst, maskFormat, xSrc, ySrc,
    //XPointFixed *points, npoint);
    // this will require "points on a circle" calculation, however...

    int s = qMin(CS, qMin(rect.height() / 2, rect.width() / 2));
    int x, y, b, r;
    rect.getCoords(&x, &y, &r, &b);
    r -= (s - 1);
    b -= (s - 1);
    XRenderComposite(display(), PictOpOver, fill, *circle(0), pict, 0, 0, 0, 0, x, y, CS, CS);
    XRenderComposite(display(), PictOpOver, fill, *circle(1), pict, 0, 0, CS - s, 0, r, y, s, s);
    XRenderComposite(display(), PictOpOver, fill, *circle(2), pict, 0, 0, CS - s, CS - s, r, b, s, s);
    XRenderComposite(display(), PictOpOver, fill, *circle(3), pict, 0, 0, 0, CS - s, x, b, s, s);
    XRenderComposite(display(), op, fill, 0, pict, 0, 0, 0, 0, x + s, y, rect.width() - 2 * s, s);
    XRenderComposite(display(), op, fill, 0, pict, 0, 0, 0, 0, x, y + s, rect.width(), rect.height() - 2 * s);
    XRenderComposite(display(), op, fill, 0, pict, 0, 0, 0, 0, x + s, b, rect.width() - 2 * s, s);
}

#undef CS
#undef DUMP_CNR

// XRenderFind(Standard)Format() is a roundtrip, so cache the results
static XRenderPictFormat* renderformats[ 33 ];

static Picture createPicture(Pixmap pix, int depth)
{
    if (pix == None)
        return None;
    if (renderformats[ depth ] == NULL) {
        switch(depth) {
        case 1:
            renderformats[ 1 ] = XRenderFindStandardFormat(display(), PictStandardA1);
            break;
        case 8:
            renderformats[ 8 ] = XRenderFindStandardFormat(display(), PictStandardA8);
            break;
        case 24:
            renderformats[ 24 ] = XRenderFindStandardFormat(display(), PictStandardRGB24);
            break;
        case 32:
            renderformats[ 32 ] = XRenderFindStandardFormat(display(), PictStandardARGB32);
            break;
        default: {
            XRenderPictFormat req;
            long mask = PictFormatType | PictFormatDepth;
            req.type = PictTypeDirect;
            req.depth = depth;
            renderformats[ depth ] = XRenderFindFormat(display(), mask, &req, 0);
            break;
        }
        }
        if (renderformats[ depth ] == NULL) {
            kWarning(1212) << "Could not find XRender format for depth" << depth;
            return None;
        }
    }
    return XRenderCreatePicture(display(), pix, renderformats[ depth ], 0, NULL);
}

XRenderPicture::XRenderPicture(QPixmap pix)
{
    if (Extensions::nonNativePixmaps()) {
        Pixmap xPix = XCreatePixmap(display(), rootWindow(), pix.width(), pix.height(), pix.depth());
        QPixmap tempPix = QPixmap::fromX11Pixmap(xPix, QPixmap::ExplicitlyShared);
        tempPix.fill(Qt::transparent);
        QPainter p(&tempPix);
        p.drawPixmap(QPoint(0, 0), pix);
        p.end();
        d = new XRenderPictureData(createPicture(tempPix.handle(), tempPix.depth()));
        XFreePixmap(display(), xPix);
    } else {
        d = new XRenderPictureData(createPicture(pix.handle(), pix.depth()));
    }
}

XRenderPicture::XRenderPicture(Pixmap pix, int depth)
    : d(new XRenderPictureData(createPicture(pix, depth)))
{
}

static QPixmap *s_offscreenTarget = 0;
static QStack<XRenderPicture*> s_scene_offscreenTargetStack;
static int s_renderOffscreen = 0;

void scene_setXRenderOffscreenTarget(QPixmap *pix)
{
    s_offscreenTarget = pix;
}

XRenderPicture *scene_xRenderOffscreenTarget()
{
    return s_scene_offscreenTargetStack.isEmpty() ? 0 : s_scene_offscreenTargetStack.top();
}

void setXRenderOffscreen(bool b)
{
    b ? ++s_renderOffscreen : --s_renderOffscreen;
    if (s_renderOffscreen < 0) {
        s_renderOffscreen = 0;
        qWarning("*** SOMETHING IS MESSED UP WITH YOUR setXRenderOffscreen() USAGE ***");
    }
}

void xRenderPushTarget(XRenderPicture *pic)
{
    s_scene_offscreenTargetStack.push(pic);
    ++s_renderOffscreen;
}

void xRenderPopTarget()
{
    s_scene_offscreenTargetStack.pop();
    --s_renderOffscreen;
    if (s_renderOffscreen < 0) {
        s_renderOffscreen = 0;
        qWarning("*** SOMETHING IS MESSED UP WITH YOUR xRenderPopTarget() USAGE ***");
    }
}

bool xRenderOffscreen()
{
    return s_renderOffscreen;
}

QPixmap *xRenderOffscreenTarget()
{
    return s_offscreenTarget;
}

} // namespace

#endif
