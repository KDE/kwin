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
#include "outputlayer.h"

#include <QMap>

namespace KWin
{

class X11WindowedBackend;
class EglX11Backend;

class EglX11Output : public OutputLayer
{
public:
    EglX11Output(AbstractOutput *output, EglX11Backend *backend, EGLSurface surface);
    ~EglX11Output();

    std::optional<QRegion> beginFrame(const QRect &geometry) override;
    void endFrame(const QRegion &renderedRegion, const QRegion &damagedRegion) override;
    QRect geometry() const override;

private:
    EglX11Backend *const m_backend;
    AbstractOutput *const m_output;

    EGLSurface m_eglSurface;
    QScopedPointer<GLRenderTarget> m_renderTarget;
    QRegion m_lastRenderedRegion;

    friend class EglX11Backend;
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
    void present(AbstractOutput *output) override;
    QVector<OutputLayer *> getLayers(RenderOutput *output) override;

protected:
    void cleanupSurfaces() override;
    bool createSurfaces() override;

private:
    void presentSurface(EGLSurface surface, const QRegion &damage, const QRect &screenGeometry);

    QMap<AbstractOutput *, QSharedPointer<EglX11Output>> m_outputs;
    X11WindowedBackend *m_backend;
};

} // namespace

#endif
