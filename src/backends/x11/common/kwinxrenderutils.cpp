/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2008 Lubos Lunak <l.lunak@kde.org>
    SPDX-FileCopyrightText: 2022 MBition GmbH
    SPDX-FileContributor: Kai Uwe Broulik <kai_uwe.broulik@mbition.io>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "kwinxrenderutils.h"
#include "x11_common_logging_p.h"

#include <QCoreApplication>
#include <QImage>

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

void cleanup()
{
    s_connection = nullptr;
    s_rootWindow = XCB_WINDOW_NONE;
}

} // namespace

static xcb_render_picture_t createPicture(xcb_pixmap_t pix, int depth)
{
    if (pix == XCB_PIXMAP_NONE) {
        return XCB_RENDER_PICTURE_NONE;
    }
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

    d = std::make_unique<XRenderPictureData>(createPicture(xpix, depth));
    xcb_free_pixmap(c, xpix);
}

XRenderPicture::XRenderPicture(xcb_pixmap_t pix, int depth)
    : d(std::make_unique<XRenderPictureData>(createPicture(pix, depth)))
{
}

XRenderPictureData::~XRenderPictureData()
{
    if (picture != XCB_RENDER_PICTURE_NONE) {
        Q_ASSERT(qApp);
        xcb_render_free_picture(XRenderUtils::s_connection, picture);
    }
}

namespace XRenderUtils
{

struct PictFormatData
{
    PictFormatData()
    {
        // Fetch the render pict formats
        reply = xcb_render_query_pict_formats_reply(s_connection,
                                                    xcb_render_query_pict_formats_unchecked(s_connection), nullptr);

        // Init the visual ID -> format ID hash table
        for (auto screens = xcb_render_query_pict_formats_screens_iterator(reply); screens.rem; xcb_render_pictscreen_next(&screens)) {
            for (auto depths = xcb_render_pictscreen_depths_iterator(screens.data); depths.rem; xcb_render_pictdepth_next(&depths)) {
                const xcb_render_pictvisual_t *visuals = xcb_render_pictdepth_visuals(depths.data);
                const int len = xcb_render_pictdepth_visuals_length(depths.data);

                for (int i = 0; i < len; i++) {
                    visualHash.insert(visuals[i].visual, visuals[i].format);
                }
            }
        }

        // Init the format ID -> xcb_render_directformat_t* hash table
        const xcb_render_pictforminfo_t *formats = xcb_render_query_pict_formats_formats(reply);
        const int len = xcb_render_query_pict_formats_formats_length(reply);

        for (int i = 0; i < len; i++) {
            if (formats[i].type == XCB_RENDER_PICT_TYPE_DIRECT) {
                formatInfoHash.insert(formats[i].id, &formats[i].direct);
            }
        }
    }

    ~PictFormatData()
    {
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
