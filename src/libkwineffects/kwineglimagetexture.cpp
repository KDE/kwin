/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2020 Aleix Pol Gonzalez <aleixpol@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "libkwineffects/kwineglimagetexture.h"
#include "libkwineffects/kwingltexture_p.h"

#include <QDebug>
#include <epoxy/egl.h>

namespace KWin
{

EGLImageTexture::EGLImageTexture(::EGLDisplay display, EGLImage image, uint textureId, int internalFormat, const QSize &size)
    : GLTexture(textureId, internalFormat, size, 1, true)
    , m_image(image)
    , m_display(display)
{
    d_ptr->m_foreign = false;
    setContentTransform(TextureTransform::MirrorY);
}

EGLImageTexture::~EGLImageTexture()
{
    eglDestroyImageKHR(m_display, m_image);
}

std::shared_ptr<EGLImageTexture> EGLImageTexture::create(::EGLDisplay display, EGLImageKHR image, int internalFormat, const QSize &size)
{
    if (image == EGL_NO_IMAGE) {
        return nullptr;
    }
    GLuint texture = 0;
    glGenTextures(1, &texture);
    if (!texture) {
        return nullptr;
    }
    glBindTexture(GL_TEXTURE_2D, texture);
    glEGLImageTargetTexture2DOES(GL_TEXTURE_2D, image);
    glBindTexture(GL_TEXTURE_2D, 0);
    return std::make_shared<EGLImageTexture>(display, image, texture, internalFormat, size);
}

} // namespace KWin
