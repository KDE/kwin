/*
    SPDX-FileCopyrightText: 2020 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "eglonxbackend.h"
#include "openglsurfacetextureprovider.h"

#include "krknativetexture.h"
#include "krktexture.h"

#include <kwingltexture.h>
#include <kwingltexture_p.h>

namespace KWin
{

class EglPixmapTexturePrivate;
class SoftwareVsyncMonitor;
class X11StandalonePlatform;

class EglBackend : public EglOnXBackend
{
    Q_OBJECT

public:
    EglBackend(Display *display, X11StandalonePlatform *platform);
    ~EglBackend() override;

    void init() override;

    SurfaceTextureProvider *createSurfaceTextureProviderX11(SurfacePixmapX11 *texture) override;
    QRegion beginFrame(int screenId) override;
    void endFrame(int screenId, const QRegion &damage, const QRegion &damagedRegion) override;
    void screenGeometryChanged(const QSize &size) override;

private:
    void presentSurface(EGLSurface surface, const QRegion &damage, const QRect &screenGeometry);
    void vblank(std::chrono::nanoseconds timestamp);

    X11StandalonePlatform *m_backend;
    SoftwareVsyncMonitor *m_vsyncMonitor;
    int m_bufferAge = 0;
};

class EglSurfaceTextureX11 final : public KrkTexture
{
    Q_OBJECT

public:
    EglSurfaceTextureX11(EglBackend *eglBackend, EGLImageKHR eglImage, GLTexture *texture,
                         bool hasAlphaChannel);
    ~EglSurfaceTextureX11() override;

    void bind() override;
    void reattach();

    bool hasAlphaChannel() const override;
    KrkNative::KrkNativeTexture *nativeTexture() const override;

private:
    EglBackend *m_backend;
    EGLImageKHR m_image;
    KrkNative::KrkOpenGLTexture m_nativeTexture;
    bool m_hasAlphaChannel;
};

class EglSurfaceTextureProviderX11 final : public OpenGLSurfaceTextureProvider
{
    Q_OBJECT

public:
    EglSurfaceTextureProviderX11(EglBackend *backend, SurfacePixmapX11 *texture);

    EglBackend *backend() const;

    bool create() override;
    void update(const QRegion &region) override;

private:
    SurfacePixmapX11 *m_pixmap;
};

} // namespace KWin
