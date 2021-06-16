/*
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "openglsurfacetextureprovider_x11.h"

namespace KWin
{

OpenGLSurfaceTextureProviderX11::OpenGLSurfaceTextureProviderX11(OpenGLBackend *backend,
                                                                 SurfacePixmapX11 *pixmap)
    : OpenGLSurfaceTextureProvider(backend)
    , m_pixmap(pixmap)
{
}

} // namespace KWin
