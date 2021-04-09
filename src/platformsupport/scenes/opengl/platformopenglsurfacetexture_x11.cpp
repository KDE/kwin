/*
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "platformopenglsurfacetexture_x11.h"

namespace KWin
{

PlatformOpenGLSurfaceTextureX11::PlatformOpenGLSurfaceTextureX11(OpenGLBackend *backend,
                                                                 SurfacePixmapX11 *pixmap)
    : PlatformOpenGLSurfaceTexture(backend)
    , m_pixmap(pixmap)
{
}

} // namespace KWin
