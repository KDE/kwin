/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2015 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once
#include "drm_render_backend.h"
#include "platformsupport/scenes/opengl/abstract_egl_backend.h"

#include "libkwineffects/kwinglutils.h"

#include <QHash>
#include <QPointer>
#include <optional>

namespace KWaylandServer
{
class SurfaceInterface;
}

namespace KWin
{

struct DmaBufAttributes;
class Output;
class DrmAbstractOutput;
class DrmOutput;
class DumbSwapchain;
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

    std::unique_ptr<SurfaceTexture> createSurfaceTextureInternal(SurfacePixmapInternal *pixmap) override;
    std::unique_ptr<SurfaceTexture> createSurfaceTextureWayland(SurfacePixmapWayland *pixmap) override;

    void present(Output *output) override;
    OutputLayer *primaryLayer(Output *output) override;
    OutputLayer *cursorLayer(Output *output) override;

    void init() override;
    bool prefer10bpc() const override;
    std::shared_ptr<DrmPipelineLayer> createPrimaryLayer(DrmPipeline *pipeline) override;
    std::shared_ptr<DrmOverlayLayer> createCursorLayer(DrmPipeline *pipeline) override;
    std::shared_ptr<DrmOutputLayer> createLayer(DrmVirtualOutput *output) override;

    std::pair<std::shared_ptr<KWin::GLTexture>, ColorDescription> textureForOutput(Output *requestedOutput) const override;

    std::optional<GbmFormat> gbmFormatForDrmFormat(uint32_t format) const;
    DrmGpu *gpu() const;

    EglContext *contextForGpu(DrmGpu *gpu);

private:
    bool initializeEgl();
    bool initBufferConfigs(EglDisplay *display);
    bool initRenderingContext();
    EglDisplay *createEglDisplay(DrmGpu *gpu) const;

    DrmBackend *m_backend;
    std::map<EglDisplay *, std::unique_ptr<EglContext>> m_contexts;
    QHash<EglDisplay *, QHash<uint32_t, GbmFormat>> m_formats;

    friend class EglGbmTexture;
};

} // namespace
