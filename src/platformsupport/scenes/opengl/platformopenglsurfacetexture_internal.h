/*
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "platformopenglsurfacetexture.h"

namespace KWin
{

class SurfacePixmapInternal;

class KWIN_EXPORT PlatformOpenGLSurfaceTextureInternal : public PlatformOpenGLSurfaceTexture
{
public:
    PlatformOpenGLSurfaceTextureInternal(OpenGLBackend *backend, SurfacePixmapInternal *pixmap);

protected:
    SurfacePixmapInternal *m_pixmap;
};

} // namespace KWin
