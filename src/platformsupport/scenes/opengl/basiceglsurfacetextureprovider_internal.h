/*
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "openglsurfacetextureprovider.h"

namespace KWin
{

class KWIN_EXPORT BasicEGLSurfaceTextureProviderInternal : public OpenGLSurfaceTextureProvider
{
public:
    BasicEGLSurfaceTextureProviderInternal(OpenGLBackend *backend, SurfacePixmapInternal *pixmap);

    bool create() override;
    void update(const QRegion &region) override;

private:
    bool updateFromFramebuffer();
    bool updateFromImage(const QRegion &region);

    SurfacePixmapInternal *m_pixmap;
};

} // namespace KWin
