/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2008 Lubos Lunak <l.lunak@kde.org>
    SPDX-FileCopyrightText: 2022 MBition GmbH
    SPDX-FileContributor: Kai Uwe Broulik <kai_uwe.broulik@mbition.io>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

// KWin
#include <kwin_export.h>
// Qt
#include <QExplicitlySharedDataPointer>
// XCB
#include <xcb/render.h>

#include <memory>

class QImage;

/** @addtogroup kwineffects */
/** @{ */

namespace KWin
{

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
    explicit XRenderPicture(const QImage &img);
    XRenderPicture(xcb_pixmap_t pix, int depth);
    operator xcb_render_picture_t();

private:
    void fromImage(const QImage &img);
    std::unique_ptr<XRenderPictureData> d;
};

inline XRenderPictureData::XRenderPictureData(xcb_render_picture_t pic)
    : picture(pic)
{
}

inline xcb_render_picture_t XRenderPictureData::value()
{
    return picture;
}

inline XRenderPicture::XRenderPicture(xcb_render_picture_t pic)
    : d(std::make_unique<XRenderPictureData>(pic))
{
}

inline XRenderPicture::operator xcb_render_picture_t()
{
    return d->value();
}

namespace XRenderUtils
{
/**
 * @internal
 */
KWIN_EXPORT void init(xcb_connection_t *connection, xcb_window_t rootWindow);

/**
 * Returns the Xrender format that corresponds to the given visual ID.
 */
KWIN_EXPORT xcb_render_pictformat_t findPictFormat(xcb_visualid_t visual);

/**
 * Returns the xcb_render_directformat_t for the given Xrender format.
 */
KWIN_EXPORT const xcb_render_directformat_t *findPictFormatInfo(xcb_render_pictformat_t format);

/**
 * @internal
 */
KWIN_EXPORT void cleanup();

} // namespace XRenderUtils

} // namespace KWin

/** @} */
