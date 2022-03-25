/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2015 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#ifndef KWIN_EGL_GBM_BACKEND_H
#define KWIN_EGL_GBM_BACKEND_H
#include "abstract_egl_backend.h"
#include "outputlayer.h"

namespace KWin
{
class VirtualBackend;
class GLTexture;
class GLRenderTarget;
class EglGbmBackend;

class VirtualEglOutputLayer : public OutputLayer
{
public:
    VirtualEglOutputLayer(EglGbmBackend *backend);

    std::optional<QRegion> beginFrame(const QRect &geometry) override;
    void endFrame(const QRegion &renderedRegion, const QRegion &damagedRegion) override;
    QRect geometry() const override;

private:
    EglGbmBackend *const m_backend;
};

/**
 * @brief OpenGL Backend using Egl on a GBM surface.
 */
class EglGbmBackend : public AbstractEglBackend
{
    Q_OBJECT

public:
    EglGbmBackend(VirtualBackend *b);
    ~EglGbmBackend() override;
    SurfaceTexture *createSurfaceTextureInternal(SurfacePixmapInternal *pixmap) override;
    SurfaceTexture *createSurfaceTextureWayland(SurfacePixmapWayland *pixmap) override;

    QRegion beginFrame();
    void present(AbstractOutput *output) override;
    QVector<OutputLayer *> getLayers(RenderOutput *output) override;

    void init() override;

private:
    bool initializeEgl();
    bool initBufferConfigs();
    bool initRenderingContext();
    VirtualBackend *m_backend;
    GLTexture *m_backBuffer = nullptr;
    GLRenderTarget *m_fbo = nullptr;
    QScopedPointer<VirtualEglOutputLayer> m_layer;
    int m_frameCounter = 0;
};

} // namespace

#endif
