/*
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "platformsupport/scenes/opengl/openglsurfacetexture_x11.h"

namespace KWin
{

OpenGLSurfaceTextureX11::OpenGLSurfaceTextureX11(OpenGLBackend *backend, SurfacePixmapX11 *pixmap)
    : OpenGLSurfaceTexture(backend)
    , m_pixmap(pixmap)
{
}

} // namespace KWin
