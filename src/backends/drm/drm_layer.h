/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2022 Xaver Hugl <xaver.hugl@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once
#include "core/outputlayer.h"
#include "drm_layer_surface.h"

#include <QMap>
#include <QPointer>
#include <epoxy/egl.h>
#include <optional>

namespace KWin
{

class RenderBackend;
class DrmPlane;
class DrmPipeline;

class DrmPipelineLayer : public OutputLayer
{
public:
    explicit DrmPipelineLayer(RenderBackend *backend, DrmPlane *plane);
    explicit DrmPipelineLayer(RenderBackend *backend, DrmGpu *gpu, DrmPlane::TypeIndex type);

    std::optional<OutputLayerBeginFrameInfo> beginFrame(OutputFrame *frame) override;
    bool endFrame(const Region &damagedDeviceRegion, FileDescriptor &&sync, OutputFrame *frame) override;
    bool preparePresentationTest() override;
    void releaseBuffers() override;

    FormatModifierMap supportedDrmFormats() const override;
    QList<QSize> recommendedSizes() const override;
    FormatModifierMap supportedAsyncDrmFormats() const override;

    std::shared_ptr<DrmFramebuffer> currentBuffer() const;
    DrmPlane *plane() const;

private:
    bool earlyScanoutChecks() override;
    bool importScanoutBuffer(GraphicsBuffer *buffer, const std::shared_ptr<OutputFrame> &frame) override;
    SurfaceItem *presentedDirectScanoutItem() const override;

    DrmPipeline *pipeline() const;
    DrmGpu *gpu() const;
    DrmOutput *drmOutput() const;

    DrmPlane *m_plane = nullptr;

    DrmLayerSurface m_surface;
    std::shared_ptr<DrmFramebuffer> m_scanoutBuffer;
};

}
