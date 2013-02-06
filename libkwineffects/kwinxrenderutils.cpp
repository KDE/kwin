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

#include <QStack>
#include <QPixmap>
#include <QPainter>
#include <kdebug.h>

namespace KWin
{

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

static XRenderPicture _blendPicture(X::None);
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
