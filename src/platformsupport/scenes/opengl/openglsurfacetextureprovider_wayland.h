/*
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "openglsurfacetextureprovider.h"

namespace KWin
{

class KWIN_EXPORT OpenGLSurfaceTextureProviderWayland : public OpenGLSurfaceTextureProvider
{
public:
    OpenGLSurfaceTextureProviderWayland(OpenGLBackend *backend, SurfacePixmapWayland *pixmap);

protected:
    SurfacePixmapWayland *m_pixmap;
};

} // namespace KWin
