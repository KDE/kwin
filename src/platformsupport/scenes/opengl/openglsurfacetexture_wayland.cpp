/*
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "openglsurfacetexture_wayland.h"

namespace KWin
{

OpenGLSurfaceTextureWayland::OpenGLSurfaceTextureWayland(OpenGLBackend *backend, SurfacePixmap *pixmap)
    : OpenGLSurfaceTexture(backend)
    , m_pixmap(pixmap)
{
}

} // namespace KWin
