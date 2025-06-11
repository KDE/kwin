/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2015 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once
#include "drm_plane.h"
#include "drm_render_backend.h"
#include "opengl/eglbackend.h"
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
class EglGbmBackend : public EglBackend, public DrmRenderBackend
{
    Q_OBJECT
public:
    EglGbmBackend(DrmBackend *drmBackend);
    ~EglGbmBackend() override;

    DrmDevice *drmDevice() const override;

    bool present(Output *output, const std::shared_ptr<OutputFrame> &frame) override;
    void repairPresentation(Output *output) override;
    OutputLayer *primaryLayer(Output *output) override;
    OutputLayer *cursorLayer(Output *output) override;

    void init() override;
    std::shared_ptr<DrmPipelineLayer> createDrmPlaneLayer(DrmPipeline *pipeline, DrmPlane::TypeIndex type) override;
    std::shared_ptr<DrmOutputLayer> createLayer(DrmVirtualOutput *output) override;

    std::pair<std::shared_ptr<KWin::GLTexture>, ColorDescription> textureForOutput(Output *requestedOutput) const override;

    DrmGpu *gpu() const;

    EglDisplay *displayForGpu(DrmGpu *gpu);
    std::shared_ptr<EglContext> contextForGpu(DrmGpu *gpu);
    void resetContextForGpu(DrmGpu *gpu);

private:
    bool initializeEgl();
    bool initRenderingContext();
    EglDisplay *createEglDisplay(DrmGpu *gpu) const;

    DrmBackend *m_backend;
    std::map<EglDisplay *, std::weak_ptr<EglContext>> m_contexts;

    friend class EglGbmTexture;
};

} // namespace
