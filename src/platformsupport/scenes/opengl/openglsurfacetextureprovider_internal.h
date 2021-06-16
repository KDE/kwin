/*
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "openglsurfacetextureprovider.h"

namespace KWin
{

class KWIN_EXPORT OpenGLSurfaceTextureProviderInternal : public OpenGLSurfaceTextureProvider
{
public:
    OpenGLSurfaceTextureProviderInternal(OpenGLBackend *backend, SurfacePixmapInternal *pixmap);

protected:
    SurfacePixmapInternal *m_pixmap;
};

} // namespace KWin
