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

class DrmOutputLayer : public OutputLayer
{
public:
    explicit DrmOutputLayer(Output *output);
    virtual ~DrmOutputLayer();

    virtual std::shared_ptr<GLTexture> texture() const;
    virtual void releaseBuffers() = 0;
};

class DrmPipelineLayer : public DrmOutputLayer
{
public:
    explicit DrmPipelineLayer(DrmPipeline *pipeline, DrmPlane::TypeIndex type);

    virtual std::shared_ptr<DrmFramebuffer> currentBuffer() const = 0;
    virtual ColorDescription colorDescription() const = 0;

    DrmPlane::TypeIndex type() const;

protected:
    DrmPipeline *const m_pipeline;
    const DrmPlane::TypeIndex m_type;
};
}
