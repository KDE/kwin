/*
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "krkplaintexture_p.h"

#include <kwinglutils.h>

namespace KWin
{

KrkPlainTextureOpenGL::KrkPlainTextureOpenGL(GLTexture *texture)
    : m_nativeTexture(texture)
{
}

KrkPlainTextureOpenGL::~KrkPlainTextureOpenGL()
{
    if (m_ownsTexture) {
        delete m_nativeTexture.texture;
        m_nativeTexture.texture = nullptr;
    }
}

bool KrkPlainTextureOpenGL::ownsTexture() const
{
    return m_ownsTexture;
}

void KrkPlainTextureOpenGL::setOwnsTexture(bool owns)
{
    m_ownsTexture = owns;
}

bool KrkPlainTextureOpenGL::hasAlphaChannel() const
{
    return m_hasAlphaChannel;
}

void KrkPlainTextureOpenGL::setHasAlphaChannel(bool has)
{
    m_hasAlphaChannel = has;
}

KrkNative::KrkNativeTexture *KrkPlainTextureOpenGL::nativeTexture() const
{
    return const_cast<KrkNative::KrkOpenGLTexture *>(&m_nativeTexture);
}

KrkPlainTextureSoftware::KrkPlainTextureSoftware(const QImage &image)
    : m_nativeTexture(image)
{
}

bool KrkPlainTextureSoftware::hasAlphaChannel() const
{
    return m_nativeTexture.image.hasAlphaChannel();
}

KrkNative::KrkNativeTexture *KrkPlainTextureSoftware::nativeTexture() const
{
    return const_cast<KrkNative::KrkSoftwareTexture *>(&m_nativeTexture);
}

} // namespace KWin
