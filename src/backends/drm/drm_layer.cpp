/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2022 Xaver Hugl <xaver.hugl@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "drm_layer.h"
#include "core/graphicsbuffer.h"
#include "drm_buffer.h"
#include "drm_output.h"
#include "drm_pipeline.h"

#include <QMatrix4x4>
#include <drm_fourcc.h>

namespace KWin
{

DrmOutputLayer::DrmOutputLayer(Output *output)
    : OutputLayer(output)
{
}

DrmOutputLayer::~DrmOutputLayer() = default;

std::shared_ptr<GLTexture> DrmOutputLayer::texture() const
{
    return nullptr;
}

DrmPipelineLayer::DrmPipelineLayer(DrmPipeline *pipeline, DrmPlane::TypeIndex type)
    : DrmOutputLayer(pipeline->output())
    , m_pipeline(pipeline)
    , m_type(type)
{
}

DrmPlane::TypeIndex DrmPipelineLayer::type() const
{
    return m_type;
}
}
