/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2008 Lubos Lunak <l.lunak@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "kwinxrenderutils.h"
#include "logging_p.h"

#include <QCoreApplication>
#include <QStack>
#include <QPixmap>
#include <QGlobalStatic>

namespace KWin
{

namespace XRenderUtils
{
static xcb_connection_t *s_connection = nullptr;
static xcb_window_t s_rootWindow = XCB_WINDOW_NONE;
static XRenderPicture s_blendPicture(XCB_RENDER_PICTURE_NONE);

void init(xcb_connection_t *connection, xcb_window_t rootWindow)
{
    s_connection = connection;
    s_rootWindow = rootWindow;
}

void cleanup()
{
    s_blendPicture = XRenderPicture(XCB_RENDER_PICTURE_NONE);
    s_connection = nullptr;
    s_rootWindow = XCB_WINDOW_NONE;
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
    static xcb_render_color_t s_blendColor = {0, 0, 0, 0};
    s_blendColor.alpha = uint16_t(opacity * 0xffff);
    if (XRenderUtils::s_blendPicture == XCB_RENDER_PICTURE_NONE) {
        XRenderUtils::s_blendPicture = xRenderFill(s_blendColor);
    } else {
        xcb_rectangle_t rect = {0, 0, 1, 1};
        xcb_render_fill_rectangles(XRenderUtils::s_connection, XCB_RENDER_PICT_OP_SRC, XRenderUtils::s_blendPicture, s_blendColor, 1, &rect);
    }
    return XRenderUtils::s_blendPicture;
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
        qCWarning(LIBKWINXRENDERUTILS) << "Could not find XRender format for depth" << depth;
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
                  0, 0, 0, depth, img.sizeInBytes(), img.constBits());
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
    if (picture != XCB_RENDER_PICTURE_NONE) {
        Q_ASSERT(qApp);
        xcb_render_free_picture(XRenderUtils::s_connection, picture);
    }
}

XFixesRegion::XFixesRegion(const QRegion &region)
{
    m_region = xcb_generate_id(XRenderUtils::s_connection);
    QVector<xcb_rectangle_t> xrects;
    xrects.reserve(region.rectCount());
    for (const QRect &rect : region) {
        xcb_rectangle_t xrect;
        xrect.x = rect.x();
        xrect.y = rect.y();
        xrect.width = rect.width();
        xrect.height = rect.height();
        xrects.append(xrect);
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
        qCWarning(LIBKWINXRENDERUTILS) << "*** SOMETHING IS MESSED UP WITH YOUR setXRenderOffscreen() USAGE ***";
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
        qCWarning(LIBKWINXRENDERUTILS) << "*** SOMETHING IS MESSED UP WITH YOUR xRenderPopTarget() USAGE ***";
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

namespace XRenderUtils
{

struct PictFormatData
{
    PictFormatData() {
        // Fetch the render pict formats
        reply = xcb_render_query_pict_formats_reply(s_connection,
                        xcb_render_query_pict_formats_unchecked(s_connection), nullptr);

        // Init the visual ID -> format ID hash table
        for (auto screens = xcb_render_query_pict_formats_screens_iterator(reply); screens.rem; xcb_render_pictscreen_next(&screens)) {
            for (auto depths = xcb_render_pictscreen_depths_iterator(screens.data); depths.rem; xcb_render_pictdepth_next(&depths)) {
                const xcb_render_pictvisual_t *visuals = xcb_render_pictdepth_visuals(depths.data);
                const int len = xcb_render_pictdepth_visuals_length(depths.data);

                for (int i = 0; i < len; i++)
                    visualHash.insert(visuals[i].visual, visuals[i].format);
            }
        }

        // Init the format ID -> xcb_render_directformat_t* hash table
        const xcb_render_pictforminfo_t *formats = xcb_render_query_pict_formats_formats(reply);
        const int len = xcb_render_query_pict_formats_formats_length(reply);

        for (int i = 0; i < len; i++) {
            if (formats[i].type == XCB_RENDER_PICT_TYPE_DIRECT)
                formatInfoHash.insert(formats[i].id, &formats[i].direct);
        }
    }

    ~PictFormatData() {
        free(reply);
    }

    xcb_render_query_pict_formats_reply_t *reply;
    QHash<xcb_visualid_t, xcb_render_pictformat_t> visualHash;
    QHash<xcb_render_pictformat_t, const xcb_render_directformat_t *> formatInfoHash;
};

Q_GLOBAL_STATIC(PictFormatData, g_pictFormatData)

xcb_render_pictformat_t findPictFormat(xcb_visualid_t visual)
{
    PictFormatData *d = g_pictFormatData;
    return d->visualHash.value(visual);
}

const xcb_render_directformat_t *findPictFormatInfo(xcb_render_pictformat_t format)
{
    PictFormatData *d = g_pictFormatData;
    return d->formatInfoHash.value(format);
}

} // namespace XRenderUtils

} // namespace KWin
