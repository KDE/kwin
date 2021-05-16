/*
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "platformopenglsurfacetexture_internal.h"

namespace KWin
{

class ClientBufferInternal;

class KWIN_EXPORT BasicEGLSurfaceTextureInternal : public PlatformOpenGLSurfaceTextureInternal
{
public:
    BasicEGLSurfaceTextureInternal(OpenGLBackend *backend, SurfacePixmapInternal *pixmap);

    bool create() override;
    void update(const QRegion &region) override;

private:
    bool updateFromFramebuffer(const ClientBufferInternal *buffer);
    bool updateFromImage(const ClientBufferInternal *buffer, const QRegion &region);
};

} // namespace KWin
