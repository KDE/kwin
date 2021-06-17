/*
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "openglsurfacetextureprovider.h"
#include "krktexture.h"
#include "kwingltexture.h"

namespace KWin
{

OpenGLSurfaceTextureProvider::OpenGLSurfaceTextureProvider(OpenGLBackend *backend)
    : m_backend(backend)
{
}

OpenGLSurfaceTextureProvider::~OpenGLSurfaceTextureProvider()
{
}

bool OpenGLSurfaceTextureProvider::isValid() const
{
    return m_texture;
}

KrkTexture *OpenGLSurfaceTextureProvider::texture() const
{
    return m_sceneTexture.data();
}

OpenGLBackend *OpenGLSurfaceTextureProvider::backend() const
{
    return m_backend;
}

} // namespace KWin
