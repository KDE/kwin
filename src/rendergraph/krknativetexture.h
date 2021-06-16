/*
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <kwin_export.h>
#include <QImage>

namespace KWin
{
class GLTexture;
class KrkTexture;

/**
 * The KrkNative namespace contains platform-specific details of the scene implementation.
 */
namespace KrkNative
{

/**
 * The KrkNativeTexture class is the base class for platform-specific scene graph textures.
 */
class KWIN_EXPORT KrkNativeTexture
{
public:
    enum CreateTextureOption {
        TextureHasAlpha = 0x1,
        TextureOwnsGLTexture = 0x2,
    };
    Q_DECLARE_FLAGS(CreateTextureOptions, CreateTextureOption)

    virtual ~KrkNativeTexture() {}
};

/**
 * The KrkSoftwareTexture class represents the native underlying scene graph texture with
 * the software renderer backend.
 *
 * KrkSoftwareTexture::fromNative() can be used to construct a scene graph texture for the
 * specified native raster image.
 */
class KWIN_EXPORT KrkSoftwareTexture : public KrkNativeTexture
{
public:
    static KrkTexture *fromNative(const QImage &image, CreateTextureOptions options = {});

    explicit KrkSoftwareTexture(const QImage &image);
    QImage image;
};

/**
 * The KrkOpenGLTexture class represents the native underlying scene graph texture with
 * the opengl renderer backend.
 *
 * KrkSoftwareTexture::fromNative() can be used to construct a scene graph texture for the
 * specified opengl texture.
 */
class KWIN_EXPORT KrkOpenGLTexture : public KrkNativeTexture
{
public:
    static KrkTexture *fromNative(GLTexture *texture, CreateTextureOptions options = {});

    explicit KrkOpenGLTexture(GLTexture *texture);
    GLTexture *texture;
};

}
} // namespace KWin

Q_DECLARE_OPERATORS_FOR_FLAGS(KWin::KrkNative::KrkNativeTexture::CreateTextureOptions)
