/*
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "openglsurfacetextureprovider.h"

namespace KWin
{

class KWIN_EXPORT OpenGLSurfaceTextureProviderX11 : public OpenGLSurfaceTextureProvider
{
public:
    OpenGLSurfaceTextureProviderX11(OpenGLBackend *backend, SurfacePixmapX11 *pixmap);

protected:
    SurfacePixmapX11 *m_pixmap;
};

} // namespace KWin
