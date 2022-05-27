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
class GLFramebuffer;
class GLTexture;
class EglGbmBackend;

class VirtualOutputLayer : public OutputLayer
{
public:
    VirtualOutputLayer(EglGbmBackend *backend);

    OutputLayerBeginFrameInfo beginFrame() override;
    bool endFrame(const QRegion &renderedRegion, const QRegion &damagedRegion) override;

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
    OutputLayer *primaryLayer(Output *output) override;
    void present(Output *output) override;
    void init() override;

    OutputLayerBeginFrameInfo beginFrame();

private:
    bool initializeEgl();
    bool initBufferConfigs();
    bool initRenderingContext();
    VirtualBackend *m_backend;
    GLTexture *m_backBuffer = nullptr;
    GLFramebuffer *m_fbo = nullptr;
    int m_frameCounter = 0;
    QScopedPointer<VirtualOutputLayer> m_layer;
};

} // namespace

#endif
