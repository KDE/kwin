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
class BackendOutput;
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

    QList<OutputLayer *> compatibleOutputLayers(BackendOutput *output) override;

    bool init() override;
    std::unique_ptr<DrmPipelineLayer> createDrmPlaneLayer(DrmPlane *plane) override;
    std::unique_ptr<DrmPipelineLayer> createDrmPlaneLayer(DrmGpu *gpu, DrmPlane::TypeIndex type) override;
    std::unique_ptr<DrmOutputLayer> createLayer(DrmVirtualOutput *output) override;

    DrmGpu *gpu() const;

    RenderDevice *renderDeviceForGpu(DrmGpu *gpu);
    std::shared_ptr<EglContext> contextForGpu(DrmGpu *gpu);
    void resetContextForGpu(DrmGpu *gpu);

private:
    bool initializeEgl();
    RenderDevice *createRenderDevice(DrmGpu *gpu) const;

    DrmBackend *m_backend;
    std::map<RenderDevice *, std::weak_ptr<EglContext>> m_contexts;

    friend class EglGbmTexture;
};

} // namespace
