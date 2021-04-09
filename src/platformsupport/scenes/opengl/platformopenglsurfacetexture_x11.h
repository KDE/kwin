/*
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "platformopenglsurfacetexture.h"

namespace KWin
{

class KWIN_EXPORT PlatformOpenGLSurfaceTextureX11 : public PlatformOpenGLSurfaceTexture
{
public:
    PlatformOpenGLSurfaceTextureX11(OpenGLBackend *backend, SurfacePixmapX11 *pixmap);

protected:
    SurfacePixmapX11 *m_pixmap;
};

} // namespace KWin
