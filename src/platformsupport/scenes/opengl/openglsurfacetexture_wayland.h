/*
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "openglsurfacetexture.h"

namespace KWin
{

class SurfacePixmap;

class KWIN_EXPORT OpenGLSurfaceTextureWayland : public OpenGLSurfaceTexture
{
public:
    OpenGLSurfaceTextureWayland(OpenGLBackend *backend, SurfacePixmap *pixmap);

protected:
    SurfacePixmap *m_pixmap;
};

} // namespace KWin
