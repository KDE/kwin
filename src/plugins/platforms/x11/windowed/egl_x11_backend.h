/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2015 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#ifndef KWIN_EGL_X11_BACKEND_H
#define KWIN_EGL_X11_BACKEND_H
#include "eglonxbackend.h"

namespace KWin
{

class X11WindowedBackend;

/**
 * @brief OpenGL Backend using Egl windowing system over an X overlay window.
 */
class EglX11Backend : public EglOnXBackend
{
public:
    explicit EglX11Backend(X11WindowedBackend *backend);
    ~EglX11Backend() override;

    PlatformSurfaceTexture *createPlatformSurfaceTextureInternal(SurfacePixmapInternal *pixmap) override;
    PlatformSurfaceTexture *createPlatformSurfaceTextureWayland(SurfacePixmapWayland *pixmap) override;
    void init() override;
    QRegion beginFrame(int screenId) override;
    void endFrame(int screenId, const QRegion &damage, const QRegion &damagedRegion) override;
    void screenGeometryChanged(const QSize &size) override;

protected:
    void cleanupSurfaces() override;
    bool createSurfaces() override;

private:
    void setupViewport(int screenId);
    void presentSurface(EGLSurface surface, const QRegion &damage, const QRect &screenGeometry);

    QVector<EGLSurface> m_surfaces;
    X11WindowedBackend *m_backend;
};

} // namespace

#endif
