/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2020 Aleix Pol Gonzalez <aleixpol@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "kwineglimagetexture.h"

#include <QDebug>
#include <epoxy/egl.h>

namespace KWin
{

EGLImageTexture::EGLImageTexture(EGLDisplay display, EGLImage image, int internalFormat, const QSize &size)
    : GLTexture(internalFormat, size, 1, true)
    , m_image(image)
    , m_display(display)
{
    if (m_image == EGL_NO_IMAGE_KHR) {
        return;
    }

    bind();
    glEGLImageTargetTexture2DOES(GL_TEXTURE_2D, m_image);
}

EGLImageTexture::~EGLImageTexture()
{
    eglDestroyImageKHR(m_display, m_image);
}

} // namespace KWin
