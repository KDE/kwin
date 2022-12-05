/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2015 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include "../common/x11_common_egl_backend.h"
#include "core/outputlayer.h"
#include "kwinglutils.h"

#include <QMap>

namespace KWin
{

class X11WindowedBackend;
class X11WindowedOutput;
class X11WindowedEglBackend;

class X11WindowedEglOutput : public OutputLayer
{
public:
    X11WindowedEglOutput(X11WindowedEglBackend *backend, X11WindowedOutput *output, EGLSurface surface);
    ~X11WindowedEglOutput();

    std::optional<OutputLayerBeginFrameInfo> beginFrame() override;
    bool endFrame(const QRegion &renderedRegion, const QRegion &damagedRegion) override;
    EGLSurface surface() const;
    QRegion lastDamage() const;

private:
    void ensureFbo();

    EGLSurface m_eglSurface;
    std::unique_ptr<GLFramebuffer> m_fbo;
    QRegion m_lastDamage;

    X11WindowedOutput *const m_output;
    X11WindowedEglBackend *const m_backend;
};

/**
 * @brief OpenGL Backend using Egl windowing system over an X overlay window.
 */
class X11WindowedEglBackend : public EglOnXBackend
{
    Q_OBJECT

public:
    explicit X11WindowedEglBackend(X11WindowedBackend *backend);
    ~X11WindowedEglBackend() override;

    std::unique_ptr<SurfaceTexture> createSurfaceTextureInternal(SurfacePixmapInternal *pixmap) override;
    std::unique_ptr<SurfaceTexture> createSurfaceTextureWayland(SurfacePixmapWayland *pixmap) override;
    void init() override;
    void endFrame(Output *output, const QRegion &renderedRegion, const QRegion &damagedRegion);
    void present(Output *output) override;
    OutputLayer *primaryLayer(Output *output) override;
    OutputLayer *cursorLayer(Output *output) override;

protected:
    void cleanupSurfaces() override;
    bool createSurfaces() override;

private:
    void presentSurface(EGLSurface surface, const QRegion &damage, const QRect &screenGeometry);

    QMap<Output *, std::shared_ptr<X11WindowedEglOutput>> m_outputs;
    X11WindowedBackend *m_backend;
};

} // namespace KWin
