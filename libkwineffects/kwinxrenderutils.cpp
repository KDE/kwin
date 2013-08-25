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
xcb_render_color_t preMultiply(const QColor &c, float opacity)
{
    xcb_render_color_t color;
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

XRenderPicture xRenderFill(const xcb_render_color_t &c)
{
    xcb_pixmap_t pixmap = xcb_generate_id(connection());
    xcb_create_pixmap(connection(), 32, pixmap, rootWindow(), 1, 1);
    XRenderPicture fill(pixmap, 32);
    xcb_free_pixmap(connection(), pixmap);

    uint32_t values[] = {true};
    xcb_render_change_picture(connection(), fill, XCB_RENDER_CP_REPEAT, values);

    xcb_rectangle_t rect = {0, 0, 1, 1};
    xcb_render_fill_rectangles(connection(), XCB_RENDER_PICT_OP_SRC, fill, c, 1, &rect);
    return fill;
}

XRenderPicture xRenderFill(const QColor &c)
{
    return xRenderFill(preMultiply(c));
}

XRenderPicture xRenderBlendPicture(double opacity)
{
    static XRenderPicture s_blendPicture(XCB_RENDER_PICTURE_NONE);
    static xcb_render_color_t s_blendColor = {0, 0, 0, 0};
    s_blendColor.alpha = uint16_t(opacity * 0xffff);
    if (s_blendPicture == XCB_RENDER_PICTURE_NONE) {
        s_blendPicture = xRenderFill(s_blendColor);
    } else {
        xcb_rectangle_t rect = {0, 0, 1, 1};
        xcb_render_fill_rectangles(connection(), XCB_RENDER_PICT_OP_SRC, s_blendPicture, s_blendColor, 1, &rect);
    }
    return s_blendPicture;
}

static xcb_render_picture_t createPicture(xcb_pixmap_t pix, int depth)
{
    if (pix == XCB_PIXMAP_NONE)
        return XCB_RENDER_PICTURE_NONE;
    static QHash<int, xcb_render_pictformat_t> s_renderFormats;
    if (!s_renderFormats.contains(depth)) {
        xcb_render_query_pict_formats_reply_t *formats = xcb_render_query_pict_formats_reply(connection(), xcb_render_query_pict_formats_unchecked(connection()), NULL);
        if (!formats) {
            return XCB_RENDER_PICTURE_NONE;
        }
        for (xcb_render_pictforminfo_iterator_t it = xcb_render_query_pict_formats_formats_iterator(formats);
                it.rem;
                xcb_render_pictforminfo_next(&it)) {
            if (it.data->depth == depth) {
                s_renderFormats.insert(depth, it.data->id);
                break;
            }
        }
        free(formats);
    }
    QHash<int, xcb_render_pictformat_t>::const_iterator it = s_renderFormats.constFind(depth);
    if (it == s_renderFormats.constEnd()) {
        kWarning(1212) << "Could not find XRender format for depth" << depth;
        return XCB_RENDER_PICTURE_NONE;
    }
    xcb_render_picture_t pic = xcb_generate_id(connection());
    xcb_render_create_picture(connection(), pic, pix, it.value(), 0, NULL);
    return pic;
}

XRenderPicture::XRenderPicture(const QPixmap &pix)
{
    if (Extensions::nonNativePixmaps()) {
        fromImage(pix.toImage());
    } else {
        d = new XRenderPictureData(createPicture(pix.handle(), pix.depth()));
    }
}

XRenderPicture::XRenderPicture(const QImage &img)
{
    fromImage(img);
}

void XRenderPicture::fromImage(const QImage &img)
{
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
