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

#ifndef KWIN_XRENDERUTILS_H
#define KWIN_XRENDERUTILS_H

// KWin
#include <kwinglobals.h>
// KDE
#include <KDE/KSharedPtr>
// Qt
#include <QRegion>
#include <QVector>
// XCB
#include <xcb/xfixes.h>

class QColor;
class QPixmap;

/** @addtogroup kwineffects */
/** @{ */

namespace KWin
{
/**
 * dumps a QColor into a xcb_render_color_t
 */
KWIN_EXPORT xcb_render_color_t preMultiply(const QColor &c, float opacity = 1.0);

/** @internal */
class KWIN_EXPORT XRenderPictureData
    : public QSharedData
{
public:
    explicit XRenderPictureData(xcb_render_picture_t pic = XCB_RENDER_PICTURE_NONE);
    ~XRenderPictureData();
    xcb_render_picture_t value();
private:
    xcb_render_picture_t picture;
    Q_DISABLE_COPY(XRenderPictureData)
};

/**
 * @short Wrapper around XRender Picture.
 *
 * This class wraps XRender's Picture, providing proper initialization,
 * convenience constructors and freeing of resources.
 * It should otherwise act exactly like the Picture type.
 */
class KWIN_EXPORT XRenderPicture
{
public:
    explicit XRenderPicture(xcb_render_picture_t pic = XCB_RENDER_PICTURE_NONE);
    // TODO: Qt5 - replace QPixmap by QImage to make it more obvious that it uses PutImage
    explicit XRenderPicture(const QPixmap &pix);
    explicit XRenderPicture(const QImage &img);
    XRenderPicture(xcb_pixmap_t pix, int depth);
    operator xcb_render_picture_t();
private:
    void fromImage(const QImage &img);
    KSharedPtr< XRenderPictureData > d;
};

class KWIN_EXPORT XFixesRegion
{
public:
    explicit XFixesRegion(const QRegion &region);
    virtual ~XFixesRegion();

    operator xcb_xfixes_region_t();
private:
    xcb_xfixes_region_t m_region;
};

inline
XRenderPictureData::XRenderPictureData(xcb_render_picture_t pic)
    : picture(pic)
{
}

inline
XRenderPictureData::~XRenderPictureData()
{
    if (picture != XCB_RENDER_PICTURE_NONE)
        xcb_render_free_picture(connection(), picture);
}

inline
xcb_render_picture_t XRenderPictureData::value()
{
    return picture;
}

inline
XRenderPicture::XRenderPicture(xcb_render_picture_t pic)
    : d(new XRenderPictureData(pic))
{
}

inline
XRenderPicture::operator xcb_render_picture_t()
{
    return d->value();
}

inline
XFixesRegion::XFixesRegion(const QRegion &region)
{
    m_region = xcb_generate_id(connection());
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
    xcb_xfixes_create_region(connection(), m_region, xrects.count(), xrects.constData());
}

inline
XFixesRegion::~XFixesRegion()
{
    xcb_xfixes_destroy_region(connection(), m_region);
}

inline
XFixesRegion::operator xcb_xfixes_region_t()
{
    return m_region;
}

/**
 * Static 1x1 picture used to deliver a black pixel with given opacity (for blending performance)
 * Call and Use, the PixelPicture will stay, but may change it's opacity meanwhile. It's NOT threadsafe either
 */
KWIN_EXPORT XRenderPicture xRenderBlendPicture(double opacity);
/**
 * Creates a 1x1 Picture filled with c
 */
KWIN_EXPORT XRenderPicture xRenderFill(const xcb_render_color_t &c);
KWIN_EXPORT XRenderPicture xRenderFill(const QColor &c);

/**
 * Allows to render a window into a (transparent) pixmap
 * NOTICE: the result can be queried as xRenderWindowOffscreenTarget()
 * NOTICE: it may be 0
 * NOTICE: when done call setXRenderWindowOffscreen(false) to continue normal render process
 */
KWIN_EXPORT void setXRenderOffscreen(bool b);

/**
 * Allows to define a persistent effect member as render target
 * The window (including shadows) is rendered into the top left corner
 * NOTICE: do NOT call setXRenderOffscreen(true) in addition!
 * NOTICE: do not forget to xRenderPopTarget once you're done to continue the normal render process
 */
KWIN_EXPORT void xRenderPushTarget(XRenderPicture *pic);
KWIN_EXPORT void xRenderPopTarget();

/**
 * Whether windows are currently rendered into an offscreen target buffer
 */
KWIN_EXPORT bool xRenderOffscreen();
/**
 * The offscreen buffer as set by the renderer because of setXRenderWindowOffscreen(true)
 */
KWIN_EXPORT xcb_render_picture_t xRenderOffscreenTarget();

/**
 * NOTICE: HANDS OFF!!!
 * scene_setXRenderWindowOffscreenTarget() is ONLY to be used by the renderer - DO NOT TOUCH!
 */
KWIN_EXPORT void scene_setXRenderOffscreenTarget(xcb_render_picture_t pix);
/**
 * scene_xRenderWindowOffscreenTarget() is used by the scene to figure the target set by an effect
 */
KWIN_EXPORT XRenderPicture *scene_xRenderOffscreenTarget();

} // namespace

/** @} */

#endif
