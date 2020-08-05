/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2020 Aleix Pol Gonzalez <aleixpol@kde.org>

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

#include "kwineglimagetexture.h"

using namespace KWin;

#include <QDebug>
#include <epoxy/egl.h>

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
