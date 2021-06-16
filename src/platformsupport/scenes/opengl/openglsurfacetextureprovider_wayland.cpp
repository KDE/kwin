/*
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "openglsurfacetextureprovider_wayland.h"

namespace KWin
{

OpenGLSurfaceTextureProviderWayland::OpenGLSurfaceTextureProviderWayland(OpenGLBackend *backend,
                                                                         SurfacePixmapWayland *pixmap)
    : OpenGLSurfaceTextureProvider(backend)
    , m_pixmap(pixmap)
{
}

} // namespace KWin
