/*
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "openglsurfacetexture_internal.h"

namespace KWin
{

OpenGLSurfaceTextureInternal::OpenGLSurfaceTextureInternal(OpenGLBackend *backend, SurfacePixmapInternal *pixmap)
    : OpenGLSurfaceTexture(backend)
    , m_pixmap(pixmap)
{
}

} // namespace KWin
