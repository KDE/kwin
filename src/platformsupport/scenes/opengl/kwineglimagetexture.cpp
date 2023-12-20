/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2020 Aleix Pol Gonzalez <aleixpol@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "kwineglimagetexture.h"
#include "egldisplay.h"
#include "opengl/gltexture_p.h"

#include <QDebug>
#include <epoxy/egl.h>

namespace KWin
{

EGLImageTexture::EGLImageTexture(EglDisplay *display, EGLImage image, uint textureId, int internalFormat, const QSize &size, uint32_t target)
    : GLTexture(target, textureId, internalFormat, size, 1, true, OutputTransform::FlipY)
    , m_image(image)
    , m_display(display)
{
}

EGLImageTexture::~EGLImageTexture()
{
    eglDestroyImageKHR(m_display->handle(), m_image);
}

std::shared_ptr<EGLImageTexture> EGLImageTexture::create(EglDisplay *display, EGLImageKHR image, int internalFormat, const QSize &size, bool externalOnly)
{
    if (image == EGL_NO_IMAGE) {
        return nullptr;
    }
    GLuint texture = 0;
    glGenTextures(1, &texture);
    if (!texture) {
        return nullptr;
    }
    const uint32_t target = externalOnly ? GL_TEXTURE_EXTERNAL_OES : GL_TEXTURE_2D;
    glBindTexture(target, texture);
    glEGLImageTargetTexture2DOES(target, image);
    glBindTexture(target, 0);
    return std::make_shared<EGLImageTexture>(display, image, texture, internalFormat, size, target);
}

} // namespace KWin
