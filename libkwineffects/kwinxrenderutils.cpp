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

static xcb_render_picture_t createPicture(xcb_pixmap_t pix, int depth)
{
    if (pix == XCB_PIXMAP_NONE)
        return XCB_RENDER_PICTURE_NONE;
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
    if (!Extensions::nonNativePixmaps()) {
        d = new XRenderPictureData(createPicture(pix.handle(), pix.depth()));
        return;
    }
    QImage img(pix.toImage());
    const int depth = img.depth();
    xcb_pixmap_t xpix = xcb_generate_id(connection());
    xcb_create_pixmap(connection(), depth, xpix, rootWindow(), img.width(), img.height());

    xcb_gcontext_t cid = xcb_generate_id(connection());
    xcb_create_gc(connection(), cid, xpix, 0, NULL);
    xcb_put_image(connection(), XCB_IMAGE_FORMAT_Z_PIXMAP, xpix, cid, img.width(), img.height(),
                  0, 0, 0, depth, img.byteCount(), img.constBits());
    xcb_free_gc(connection(), cid);

    d = new XRenderPictureData(createPicture(xpix, depth));
    xcb_free_pixmap(connection(), xpix);
}

XRenderPicture::XRenderPicture(xcb_pixmap_t pix, int depth)
    : d(new XRenderPictureData(createPicture(pix, depth)))
{
}

static xcb_render_picture_t s_offscreenTarget = XCB_RENDER_PICTURE_NONE;
static QStack<XRenderPicture*> s_scene_offscreenTargetStack;
static int s_renderOffscreen = 0;

void scene_setXRenderOffscreenTarget(xcb_render_picture_t pix)
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

xcb_render_picture_t xRenderOffscreenTarget()
{
    return s_offscreenTarget;
}

} // namespace
