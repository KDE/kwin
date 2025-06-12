/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2022 Xaver Hugl <xaver.hugl@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once
#include "drm_plane.h"

#include <memory>

namespace KWin
{

class DrmPipelineLayer;
class DrmVirtualOutput;
class DrmPipeline;
class DrmOutputLayer;
class DrmOverlayLayer;

class DrmRenderBackend
{
public:
    virtual ~DrmRenderBackend() = default;

    virtual std::unique_ptr<DrmPipelineLayer> createDrmPlaneLayer(DrmPlane *plane) = 0;
    virtual std::unique_ptr<DrmPipelineLayer> createDrmPlaneLayer(DrmGpu *gpu, DrmPlane::TypeIndex type) = 0;
    virtual std::unique_ptr<DrmOutputLayer> createLayer(DrmVirtualOutput *output) = 0;
};

}
