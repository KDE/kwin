/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2008 Lubos Lunak <l.lunak@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef KWIN_XRENDERUTILS_H
#define KWIN_XRENDERUTILS_H

// KWin
#include <kwinxrenderutils_export.h>
// Qt
#include <QExplicitlySharedDataPointer>
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
KWINXRENDERUTILS_EXPORT xcb_render_color_t preMultiply(const QColor &c, float opacity = 1.0);

/** @internal */
class KWINXRENDERUTILS_EXPORT XRenderPictureData
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
class KWINXRENDERUTILS_EXPORT XRenderPicture
{
public:
    explicit XRenderPicture(xcb_render_picture_t pic = XCB_RENDER_PICTURE_NONE);
    explicit XRenderPicture(const QImage &img);
    XRenderPicture(xcb_pixmap_t pix, int depth);
    operator xcb_render_picture_t();
private:
    void fromImage(const QImage &img);
    QExplicitlySharedDataPointer< XRenderPictureData > d;
};

class KWINXRENDERUTILS_EXPORT XFixesRegion
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
XFixesRegion::operator xcb_xfixes_region_t()
{
    return m_region;
}

/**
 * Static 1x1 picture used to deliver a black pixel with given opacity (for blending performance)
 * Call and Use, the PixelPicture will stay, but may change it's opacity meanwhile. It's NOT threadsafe either
 */
KWINXRENDERUTILS_EXPORT XRenderPicture xRenderBlendPicture(double opacity);
/**
 * Creates a 1x1 Picture filled with c
 */
KWINXRENDERUTILS_EXPORT XRenderPicture xRenderFill(const xcb_render_color_t &c);
KWINXRENDERUTILS_EXPORT XRenderPicture xRenderFill(const QColor &c);

/**
 * Allows to render a window into a (transparent) pixmap
 * NOTICE: the result can be queried as xRenderWindowOffscreenTarget()
 * NOTICE: it may be 0
 * NOTICE: when done call setXRenderWindowOffscreen(false) to continue normal render process
 */
KWINXRENDERUTILS_EXPORT void setXRenderOffscreen(bool b);

/**
 * Allows to define a persistent effect member as render target
 * The window (including shadows) is rendered into the top left corner
 * NOTICE: do NOT call setXRenderOffscreen(true) in addition!
 * NOTICE: do not forget to xRenderPopTarget once you're done to continue the normal render process
 */
KWINXRENDERUTILS_EXPORT void xRenderPushTarget(XRenderPicture *pic);
KWINXRENDERUTILS_EXPORT void xRenderPopTarget();

/**
 * Whether windows are currently rendered into an offscreen target buffer
 */
KWINXRENDERUTILS_EXPORT bool xRenderOffscreen();
/**
 * The offscreen buffer as set by the renderer because of setXRenderWindowOffscreen(true)
 */
KWINXRENDERUTILS_EXPORT xcb_render_picture_t xRenderOffscreenTarget();

/**
 * NOTICE: HANDS OFF!!!
 * scene_setXRenderWindowOffscreenTarget() is ONLY to be used by the renderer - DO NOT TOUCH!
 */
KWINXRENDERUTILS_EXPORT void scene_setXRenderOffscreenTarget(xcb_render_picture_t pix);
/**
 * scene_xRenderWindowOffscreenTarget() is used by the scene to figure the target set by an effect
 */
KWINXRENDERUTILS_EXPORT XRenderPicture *scene_xRenderOffscreenTarget();

namespace XRenderUtils
{
/**
 * @internal
 */
KWINXRENDERUTILS_EXPORT void init(xcb_connection_t *connection, xcb_window_t rootWindow);

/**
 * Returns the Xrender format that corresponds to the given visual ID.
 */
KWINXRENDERUTILS_EXPORT xcb_render_pictformat_t findPictFormat(xcb_visualid_t visual);

/**
 * Returns the xcb_render_directformat_t for the given Xrender format.
 */
KWINXRENDERUTILS_EXPORT const xcb_render_directformat_t *findPictFormatInfo(xcb_render_pictformat_t format);

/**
 * @internal
 */
KWINXRENDERUTILS_EXPORT void cleanup();

} // namespace XRenderUtils

} // namespace KWin

/** @} */

#endif
