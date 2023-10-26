/*
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "openglsurfacetexture.h"
#include "opengl/gltexture.h"

namespace KWin
{

OpenGLSurfaceTexture::OpenGLSurfaceTexture(OpenGLBackend *backend)
    : m_backend(backend)
{
}

OpenGLSurfaceTexture::~OpenGLSurfaceTexture()
{
}

bool OpenGLSurfaceTexture::isValid() const
{
    return m_texture.isValid();
}

OpenGLBackend *OpenGLSurfaceTexture::backend() const
{
    return m_backend;
}

OpenGLSurfaceContents OpenGLSurfaceTexture::texture() const
{
    return m_texture;
}

void OpenGLSurfaceContents::setDirty()
{
    for (auto &plane : planes) {
        plane->setDirty();
    }
}

} // namespace KWin
