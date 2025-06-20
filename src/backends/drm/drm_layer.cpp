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

DrmOutputLayer::DrmOutputLayer(Output *output, OutputLayerType type)
    : OutputLayer(output, type)
{
}

DrmOutputLayer::~DrmOutputLayer() = default;

static OutputLayerType planeToLayerType(DrmPlane *plane, DrmPlane::TypeIndex type)
{
    switch (type) {
    case DrmPlane::TypeIndex::Overlay:
        return OutputLayerType::GenericLayer;
    case DrmPlane::TypeIndex::Primary:
        return OutputLayerType::Primary;
    case DrmPlane::TypeIndex::Cursor:
        if (plane && plane->gpu()->isVirtualMachine()) {
            return OutputLayerType::CursorOnly;
        } else {
            return OutputLayerType::EfficientOverlay;
        }
    }
    Q_UNREACHABLE();
}

DrmPipelineLayer::DrmPipelineLayer(DrmPlane *plane)
    : DrmOutputLayer(nullptr, planeToLayerType(plane, plane->type.enumValue()))
    , m_plane(plane)
{
}

DrmPipelineLayer::DrmPipelineLayer(DrmPlane::TypeIndex type)
    : DrmOutputLayer(nullptr, planeToLayerType(nullptr, type))
{
}

DrmPlane *DrmPipelineLayer::plane() const
{
    return m_plane;
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
    } else if (m_type == OutputLayerType::CursorOnly) {
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
    } else if (m_type == OutputLayerType::CursorOnly) {
        return {gpu()->cursorSize()};
    } else {
        return {};
    }
}
}
