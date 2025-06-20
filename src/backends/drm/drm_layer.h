/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2022 Xaver Hugl <xaver.hugl@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once
#include "core/colorpipeline.h"
#include "core/outputlayer.h"
#include "drm_plane.h"

#include <QRegion>
#include <memory>
#include <optional>

namespace KWin
{

class SurfaceItem;
class DrmFramebuffer;
class GLTexture;
class DrmPipeline;
class DrmOutput;

class DrmOutputLayer : public OutputLayer
{
public:
    explicit DrmOutputLayer(Output *output, OutputLayerType type);
    explicit DrmOutputLayer(Output *output, OutputLayerType type, int zpos);
    virtual ~DrmOutputLayer();

    virtual void releaseBuffers() = 0;
};

class DrmPipelineLayer : public DrmOutputLayer
{
public:
    explicit DrmPipelineLayer(DrmPlane *plane);
    explicit DrmPipelineLayer(DrmPlane::TypeIndex type);

    DrmDevice *scanoutDevice() const override;
    QHash<uint32_t, QList<uint64_t>> supportedDrmFormats() const override;
    QList<QSize> recommendedSizes() const override;
    QHash<uint32_t, QList<uint64_t>> supportedAsyncDrmFormats() const override;

    virtual std::shared_ptr<DrmFramebuffer> currentBuffer() const = 0;

    DrmPlane *plane() const;

protected:
    DrmPipeline *pipeline() const;
    DrmGpu *gpu() const;
    DrmOutput *drmOutput() const;

    DrmPlane *m_plane = nullptr;
};
}
