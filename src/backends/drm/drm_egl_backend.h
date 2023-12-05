/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2015 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once
#include "drm_render_backend.h"
#include "platformsupport/scenes/opengl/abstract_egl_backend.h"

#include "opengl/glutils.h"

#include <QHash>
#include <QPointer>
#include <optional>

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
class EglContext;
class EglDisplay;

/**
 * @brief OpenGL Backend using Egl on a GBM surface.
 */
class EglGbmBackend : public AbstractEglBackend, public DrmRenderBackend
{
    Q_OBJECT
public:
    EglGbmBackend(DrmBackend *drmBackend);
    ~EglGbmBackend() override;

    std::unique_ptr<SurfaceTexture> createSurfaceTextureWayland(SurfacePixmap *pixmap) override;

    GraphicsBufferAllocator *graphicsBufferAllocator() const override;

    void present(Output *output, const std::shared_ptr<OutputFrame> &frame) override;
    OutputLayer *primaryLayer(Output *output) override;
    OutputLayer *cursorLayer(Output *output) override;

    void init() override;
    std::shared_ptr<DrmPipelineLayer> createPrimaryLayer(DrmPipeline *pipeline) override;
    std::shared_ptr<DrmPipelineLayer> createCursorLayer(DrmPipeline *pipeline) override;
    std::shared_ptr<DrmOutputLayer> createLayer(DrmVirtualOutput *output) override;

    std::pair<std::shared_ptr<KWin::GLTexture>, ColorDescription> textureForOutput(Output *requestedOutput) const override;

    DrmGpu *gpu() const;

    EglDisplay *displayForGpu(DrmGpu *gpu);
    std::shared_ptr<EglContext> contextForGpu(DrmGpu *gpu);

private:
    bool initializeEgl();
    bool initRenderingContext();
    EglDisplay *createEglDisplay(DrmGpu *gpu) const;

    DrmBackend *m_backend;
    std::map<EglDisplay *, std::weak_ptr<EglContext>> m_contexts;

    friend class EglGbmTexture;
};

} // namespace
