/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2022 Xaver Hugl <xaver.hugl@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "drm_layer.h"
#include "core/graphicsbuffer.h"
#include "drm_buffer.h"
#include "drm_gpu.h"
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

DrmPipelineLayer::DrmPipelineLayer(DrmPlane *plane)
    : DrmOutputLayer(nullptr)
    , m_type(plane->type.enumValue())
    , m_plane(plane)
{
}

DrmPipelineLayer::DrmPipelineLayer(DrmPlane::TypeIndex type)
    : DrmOutputLayer(nullptr)
    , m_type(type)
{
}

DrmPlane::TypeIndex DrmPipelineLayer::type() const
{
    return m_type;
}

DrmPipeline *DrmPipelineLayer::pipeline() const
{
    return drmOutput()->pipeline();
}

DrmGpu *DrmPipelineLayer::gpu() const
{
    return pipeline()->gpu();
}

DrmOutput *DrmPipelineLayer::drmOutput() const
{
    return static_cast<DrmOutput *>(m_output.get());
}

DrmDevice *DrmPipelineLayer::scanoutDevice() const
{
    return pipeline()->gpu()->drmDevice();
}

static const QHash<uint32_t, QList<uint64_t>> s_legacyFormats = {{DRM_FORMAT_XRGB8888, {DRM_FORMAT_MOD_INVALID}}};
static const QHash<uint32_t, QList<uint64_t>> s_legacyCursorFormats = {{DRM_FORMAT_ARGB8888, {DRM_FORMAT_MOD_LINEAR}}};

QHash<uint32_t, QList<uint64_t>> DrmPipelineLayer::supportedDrmFormats() const
{
    if (m_plane) {
        return m_plane->formats();
    } else if (m_type == DrmPlane::TypeIndex::Cursor) {
        return s_legacyCursorFormats;
    } else {
        return s_legacyFormats;
    }
}

QHash<uint32_t, QList<uint64_t>> DrmPipelineLayer::supportedAsyncDrmFormats() const
{
    if (m_plane) {
        return m_plane->tearingFormats();
    } else {
        return {};
    }
}

QList<QSize> DrmPipelineLayer::recommendedSizes() const
{
    if (m_plane) {
        return m_plane->recommendedSizes();
    } else if (m_type == DrmPlane::TypeIndex::Cursor) {
        return {gpu()->cursorSize()};
    } else {
        return {};
    }
}
}
