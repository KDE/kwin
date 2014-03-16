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

#include <QDebug>
#include <QStack>
#include <QPixmap>

namespace KWin
{

namespace XRenderUtils
{
static xcb_connection_t *s_connection = nullptr;
static xcb_window_t s_rootWindow = XCB_WINDOW_NONE;

void init(xcb_connection_t *connection, xcb_window_t rootWindow)
{
    s_connection = connection;
    s_rootWindow = rootWindow;
}

} // namespace

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
    xcb_pixmap_t pixmap = xcb_generate_id(XRenderUtils::s_connection);
    xcb_create_pixmap(XRenderUtils::s_connection, 32, pixmap, XRenderUtils::s_rootWindow, 1, 1);
    XRenderPicture fill(pixmap, 32);
    xcb_free_pixmap(XRenderUtils::s_connection, pixmap);

    uint32_t values[] = {true};
    xcb_render_change_picture(XRenderUtils::s_connection, fill, XCB_RENDER_CP_REPEAT, values);

    xcb_rectangle_t rect = {0, 0, 1, 1};
    xcb_render_fill_rectangles(XRenderUtils::s_connection, XCB_RENDER_PICT_OP_SRC, fill, c, 1, &rect);
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
        xcb_render_fill_rectangles(XRenderUtils::s_connection, XCB_RENDER_PICT_OP_SRC, s_blendPicture, s_blendColor, 1, &rect);
    }
    return s_blendPicture;
}

static xcb_render_picture_t createPicture(xcb_pixmap_t pix, int depth)
{
    if (pix == XCB_PIXMAP_NONE)
        return XCB_RENDER_PICTURE_NONE;
    xcb_connection_t *c = XRenderUtils::s_connection;
    static QHash<int, xcb_render_pictformat_t> s_renderFormats;
    if (!s_renderFormats.contains(depth)) {
        xcb_render_query_pict_formats_reply_t *formats = xcb_render_query_pict_formats_reply(c, xcb_render_query_pict_formats_unchecked(c), nullptr);
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
        qWarning() << "Could not find XRender format for depth" << depth;
        return XCB_RENDER_PICTURE_NONE;
    }
    xcb_render_picture_t pic = xcb_generate_id(c);
    xcb_render_create_picture(c, pic, pix, it.value(), 0, nullptr);
    return pic;
}

XRenderPicture::XRenderPicture(const QImage &img)
{
    fromImage(img);
}

void XRenderPicture::fromImage(const QImage &img)
{
    xcb_connection_t *c = XRenderUtils::s_connection;
    const int depth = img.depth();
    xcb_pixmap_t xpix = xcb_generate_id(c);
    xcb_create_pixmap(c, depth, xpix, XRenderUtils::s_rootWindow, img.width(), img.height());

    xcb_gcontext_t cid = xcb_generate_id(c);
    xcb_create_gc(c, cid, xpix, 0, nullptr);
    xcb_put_image(c, XCB_IMAGE_FORMAT_Z_PIXMAP, xpix, cid, img.width(), img.height(),
                  0, 0, 0, depth, img.byteCount(), img.constBits());
    xcb_free_gc(c, cid);

    d = new XRenderPictureData(createPicture(xpix, depth));
    xcb_free_pixmap(c, xpix);
}

XRenderPicture::XRenderPicture(xcb_pixmap_t pix, int depth)
    : d(new XRenderPictureData(createPicture(pix, depth)))
{
}

XRenderPictureData::~XRenderPictureData()
{
    if (picture != XCB_RENDER_PICTURE_NONE)
        xcb_render_free_picture(XRenderUtils::s_connection, picture);
}

XFixesRegion::XFixesRegion(const QRegion &region)
{
    m_region = xcb_generate_id(XRenderUtils::s_connection);
    QVector< QRect > rects = region.rects();
    QVector< xcb_rectangle_t > xrects(rects.count());
    for (int i = 0;
            i < rects.count();
            ++i) {
        const QRect &rect = rects.at(i);
        xcb_rectangle_t xrect;
        xrect.x = rect.x();
        xrect.y = rect.y();
        xrect.width = rect.width();
        xrect.height = rect.height();
        xrects[i] = xrect;
    }
    xcb_xfixes_create_region(XRenderUtils::s_connection, m_region, xrects.count(), xrects.constData());
}

XFixesRegion::~XFixesRegion()
{
    xcb_xfixes_destroy_region(XRenderUtils::s_connection, m_region);
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
    return s_scene_offscreenTargetStack.isEmpty() ? nullptr : s_scene_offscreenTargetStack.top();
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
