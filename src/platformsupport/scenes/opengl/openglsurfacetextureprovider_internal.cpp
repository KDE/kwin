/*
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "openglsurfacetextureprovider_internal.h"

namespace KWin
{

OpenGLSurfaceTextureProviderInternal::OpenGLSurfaceTextureProviderInternal(OpenGLBackend *backend,
                                                                           SurfacePixmapInternal *pixmap)
    : OpenGLSurfaceTextureProvider(backend)
    , m_pixmap(pixmap)
{
}

} // namespace KWin
