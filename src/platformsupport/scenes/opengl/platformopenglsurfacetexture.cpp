/*
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "platformopenglsurfacetexture.h"
#include "kwingltexture.h"

namespace KWin
{

PlatformOpenGLSurfaceTexture::PlatformOpenGLSurfaceTexture(OpenGLBackend *backend)
    : m_backend(backend)
{
}

PlatformOpenGLSurfaceTexture::~PlatformOpenGLSurfaceTexture()
{
}

bool PlatformOpenGLSurfaceTexture::isValid() const
{
    return m_texture;
}

OpenGLBackend *PlatformOpenGLSurfaceTexture::backend() const
{
    return m_backend;
}

GLTexture *PlatformOpenGLSurfaceTexture::texture() const
{
    return m_texture.data();
}

} // namespace KWin
