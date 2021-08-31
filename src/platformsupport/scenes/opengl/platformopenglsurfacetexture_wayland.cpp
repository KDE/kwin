/*
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "platformopenglsurfacetexture_wayland.h"

namespace KWin
{
PlatformOpenGLSurfaceTextureWayland::PlatformOpenGLSurfaceTextureWayland(OpenGLBackend *backend, SurfacePixmapWayland *pixmap)
    : PlatformOpenGLSurfaceTexture(backend)
    , m_pixmap(pixmap)
{
}

} // namespace KWin
