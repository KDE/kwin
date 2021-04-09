/*
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "platformxrendersurfacetexture_x11.h"
#include "main.h"
#include "surfaceitem_x11.h"

#include <kwinxrenderutils.h>

namespace KWin
{

PlatformXrenderSurfaceTextureX11::PlatformXrenderSurfaceTextureX11(SurfacePixmapX11 *pixmap)
    : m_pixmap(pixmap)
{
}

PlatformXrenderSurfaceTextureX11::~PlatformXrenderSurfaceTextureX11()
{
    if (m_picture != XCB_RENDER_PICTURE_NONE) {
        xcb_render_free_picture(kwinApp()->x11Connection(), m_picture);
    }
}

bool PlatformXrenderSurfaceTextureX11::isValid() const
{
    return m_picture != XCB_RENDER_PICTURE_NONE;
}

xcb_render_picture_t PlatformXrenderSurfaceTextureX11::picture() const
{
    return m_picture;
}

bool PlatformXrenderSurfaceTextureX11::create()
{
    if (m_picture != XCB_RENDER_PICTURE_NONE) {
        return true;
    }

    const xcb_pixmap_t pixmap = m_pixmap->pixmap();
    if (pixmap == XCB_PIXMAP_NONE) {
        return false;
    }

    xcb_render_pictformat_t format = XRenderUtils::findPictFormat(m_pixmap->visual());
    if (format == XCB_NONE) {
        return false;
    }

    m_picture = xcb_generate_id(kwinApp()->x11Connection());
    xcb_render_create_picture(kwinApp()->x11Connection(), m_picture, pixmap, format, 0, nullptr);
    return true;
}

} // namespace KWin
