/*
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "openglsurfacetexture.h"

namespace KWin
{

class SurfacePixmapX11;

class KWIN_EXPORT OpenGLSurfaceTextureX11 : public OpenGLSurfaceTexture
{
public:
    OpenGLSurfaceTextureX11(OpenGLBackend *backend, SurfacePixmapX11 *pixmap);

protected:
    SurfacePixmapX11 *m_pixmap;
};

} // namespace KWin
