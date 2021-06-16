/*
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "krknativetexture.h"
#include "composite.h"
#include "scene.h"

namespace KWin::KrkNative
{

KrkTexture *KrkSoftwareTexture::fromNative(const QImage &image, CreateTextureOptions options)
{
    return Compositor::self()->scene()->createSceneTexture(image, options);
}

KrkSoftwareTexture::KrkSoftwareTexture(const QImage &image)
    : image(image)
{
}

KrkTexture *KrkOpenGLTexture::fromNative(GLTexture *texture, CreateTextureOptions options)
{
    return Compositor::self()->scene()->createSceneTexture(texture, options);
}

KrkOpenGLTexture::KrkOpenGLTexture(GLTexture *texture)
    : texture(texture)
{
}

} // namespace KWin::KrkNative
