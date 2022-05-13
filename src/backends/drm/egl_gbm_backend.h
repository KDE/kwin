/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2015 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once
#include "abstract_egl_backend.h"
#include "drm_render_backend.h"

#include <kwinglutils.h>

#include <QHash>
#include <QPointer>
#include <QSharedPointer>
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

struct GbmFormat
{
    uint32_t drmFormat = 0;
    uint32_t bpp;
    EGLint alphaSize = -1;
};
bool operator==(const GbmFormat &lhs, const GbmFormat &rhs);

/**
 * @brief OpenGL Backend using Egl on a GBM surface.
 */
class EglGbmBackend : public AbstractEglBackend, public DrmRenderBackend
{
    Q_OBJECT
public:
    EglGbmBackend(DrmBackend *drmBackend);
    ~EglGbmBackend() override;

    SurfaceTexture *createSurfaceTextureInternal(SurfacePixmapInternal *pixmap) override;
    SurfaceTexture *createSurfaceTextureWayland(SurfacePixmapWayland *pixmap) override;

    void present(Output *output) override;
    OutputLayer *primaryLayer(Output *output) override;

    void init() override;
    bool prefer10bpc() const override;
    QSharedPointer<DrmPipelineLayer> createPrimaryLayer(DrmPipeline *pipeline) override;
    QSharedPointer<DrmOverlayLayer> createCursorLayer(DrmPipeline *pipeline) override;
    QSharedPointer<DrmOutputLayer> createLayer(DrmVirtualOutput *output) override;

    QSharedPointer<GLTexture> textureForOutput(Output *requestedOutput) const override;

    QSharedPointer<DrmBuffer> testBuffer(DrmAbstractOutput *output);
    EGLConfig config(uint32_t format) const;
    std::optional<GbmFormat> gbmFormatForDrmFormat(uint32_t format) const;
    DrmGpu *gpu() const;

    EGLImageKHR importDmaBufAsImage(const DmaBufAttributes &attributes);
    EGLImageKHR importDmaBufAsImage(gbm_bo *bo);
    QSharedPointer<GLTexture> importDmaBufAsTexture(const DmaBufAttributes &attributes);
    QSharedPointer<GLTexture> importDmaBufAsTexture(gbm_bo *bo);

private:
    bool initializeEgl();
    bool initBufferConfigs();
    bool initRenderingContext();

    DrmBackend *m_backend;
    QHash<uint32_t, GbmFormat> m_formats;
    QHash<uint32_t, EGLConfig> m_configs;

    friend class EglGbmTexture;
};

} // namespace
