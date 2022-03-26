/*
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "openglsurfacetexture_internal.h"

namespace KWin
{

class KWIN_EXPORT BasicEGLSurfaceTextureInternal : public OpenGLSurfaceTextureInternal
{
public:
    BasicEGLSurfaceTextureInternal(OpenGLBackend *backend, SurfacePixmapInternal *pixmap);

    bool create() override;
    void update(const QRegion &region) override;

private:
    bool updateFromFramebuffer();
    bool updateFromImage(const QRegion &region);
};

} // namespace KWin
