/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2015 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once
#include "abstract_egl_backend.h"
#include "drm_render_backend.h"
#include "eglcontext.h"
#include "egldisplay.h"

#include <kwinglutils.h>

#include <QHash>
#include <QPointer>
#include <optional>

struct gbm_surface;
struct gbm_bo;

namespace KWaylandServer
{
class SurfaceInterface;
}

namespace KWin
{

struct DmaBufAttributes;
class Output;
class DrmAbstractOutput;
class DrmBuffer;
class DrmGbmBuffer;
class DrmOutput;
class GbmSurface;
class GbmBuffer;
class DumbSwapchain;
class ShadowBuffer;
class DrmBackend;
class DrmGpu;
class EglGbmLayer;
class DrmOutputLayer;
class DrmPipeline;

/**
 * @brief OpenGL Backend using Egl on a GBM surface.
 */
class EglGbmBackend : public AbstractEglBackend, public DrmRenderBackend
{
    Q_OBJECT
public:
    EglGbmBackend(DrmBackend *drmBackend);
    ~EglGbmBackend() override;

    std::unique_ptr<SurfaceTexture> createSurfaceTextureInternal(SurfacePixmapInternal *pixmap) override;
    std::unique_ptr<SurfaceTexture> createSurfaceTextureWayland(SurfacePixmapWayland *pixmap) override;

    void present(Output *output) override;
    OutputLayer *primaryLayer(Output *output) override;

    void init() override;
    bool prefer10bpc() const override;
    std::shared_ptr<DrmPipelineLayer> createPrimaryLayer(DrmPipeline *pipeline) override;
    std::shared_ptr<DrmOverlayLayer> createCursorLayer(DrmPipeline *pipeline) override;
    std::shared_ptr<DrmOutputLayer> createLayer(DrmVirtualOutput *output) override;

    std::shared_ptr<GLTexture> textureForOutput(Output *requestedOutput) const override;

    std::shared_ptr<DrmBuffer> testBuffer(DrmAbstractOutput *output);
    EGLConfig config(DrmGpu *gpu, uint32_t format) const;
    DrmGpu *gpu() const;

    EGLImageKHR importBufferObjectAsImage(gbm_bo *bo);
    std::shared_ptr<GLTexture> importBufferObjectAsTexture(gbm_bo *bo);

    KWinEglContext *contextObject(DrmGpu *gpu);

private:
    bool initializeEgl();
    QHash<uint32_t, EGLConfig> queryBufferConfigs(EGLDisplay display) const;
    bool initRenderingContext();
    EGLDisplay createDisplay(DrmGpu *gpu) const;
    void addGpu(DrmGpu *gpu);
    void removeGpu(DrmGpu *gpu);

    DrmBackend *m_backend;
    QHash<DrmGpu *, QHash<uint32_t, EGLConfig>> m_configs;
    std::map<DrmGpu *, std::unique_ptr<KWinEglDisplay>> m_displays;
    std::map<DrmGpu *, std::unique_ptr<KWinEglContext>> m_contexts;

    friend class EglGbmTexture;
};

} // namespace
