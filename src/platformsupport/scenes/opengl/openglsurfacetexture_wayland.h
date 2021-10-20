/*
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "openglsurfacetexture.h"

namespace KWin
{

class SurfacePixmapWayland;

class KWIN_EXPORT OpenGLSurfaceTextureWayland : public OpenGLSurfaceTexture
{
public:
    OpenGLSurfaceTextureWayland(OpenGLBackend *backend, SurfacePixmapWayland *pixmap);

protected:
    SurfacePixmapWayland *m_pixmap;
};

} // namespace KWin
