/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2015 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#ifndef KWIN_EGL_X11_BACKEND_H
#define KWIN_EGL_X11_BACKEND_H
#include "eglonxbackend.h"
#include "kwinglutils.h"

#include <QMap>

namespace KWin
{

class X11WindowedBackend;

class EglX11Output
{
public:
    EGLSurface m_eglSurface;
    QScopedPointer<GLRenderTarget> m_renderTarget;
};

/**
 * @brief OpenGL Backend using Egl windowing system over an X overlay window.
 */
class EglX11Backend : public EglOnXBackend
{
    Q_OBJECT

public:
    explicit EglX11Backend(X11WindowedBackend *backend);
    ~EglX11Backend() override;

    SurfaceTexture *createSurfaceTextureInternal(SurfacePixmapInternal *pixmap) override;
    SurfaceTexture *createSurfaceTextureWayland(SurfacePixmapWayland *pixmap) override;
    void init() override;
    QRegion beginFrame(AbstractOutput *output) override;
    void endFrame(AbstractOutput *output, const QRegion &renderedRegion, const QRegion &damagedRegion) override;

protected:
    void cleanupSurfaces() override;
    bool createSurfaces() override;

private:
    void presentSurface(EGLSurface surface, const QRegion &damage, const QRect &screenGeometry);

    QMap<AbstractOutput *, EglX11Output *> m_outputs;
    X11WindowedBackend *m_backend;
};

} // namespace

#endif
