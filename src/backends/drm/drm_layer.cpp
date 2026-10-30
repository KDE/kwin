/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2022 Xaver Hugl <xaver.hugl@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "drm_layer.h"
#include "core/colorpipeline.h"
#include "core/renderdevice.h"
#include "drm_backend.h"
#include "drm_buffer.h"
#include "drm_crtc.h"
#include "drm_gpu.h"
#include "drm_output.h"
#include "drm_pipeline.h"
#include "scene/surfaceitem.h"
#include "utils/envvar.h"

#include <drm_fourcc.h>

namespace KWin
{

static const FormatModifierMap s_legacyFormats = {{DRM_FORMAT_XRGB8888, {DRM_FORMAT_MOD_INVALID, DRM_FORMAT_MOD_LINEAR}}};
static const FormatModifierMap s_legacyCursorFormats = {{DRM_FORMAT_ARGB8888, {DRM_FORMAT_MOD_LINEAR}}};

static DrmLayerSurface::BufferTarget targetFor(DrmGpu *gpu, DrmPlane::TypeIndex planeType)
{
    if ((!gpu->atomicModeSetting() || gpu->drmDevice()->isVirtualMachine()) && planeType == DrmPlane::TypeIndex::Cursor) {
        return DrmLayerSurface::BufferTarget::Dumb;
    } else {
        return DrmLayerSurface::BufferTarget::Normal;
    }
}

static OutputLayerType planeToLayerType(DrmPlane *plane, DrmPlane::TypeIndex type)
{
    switch (type) {
    case DrmPlane::TypeIndex::Overlay:
        return OutputLayerType::GenericLayer;
    case DrmPlane::TypeIndex::Primary:
        return OutputLayerType::Primary;
    case DrmPlane::TypeIndex::Cursor:
        if (!plane || plane->gpu()->drmDevice()->isVirtualMachine()) {
            return OutputLayerType::CursorOnly;
        } else {
            return OutputLayerType::EfficientOverlay;
        }
    }
    Q_UNREACHABLE();
}

static int determineZpos(DrmPlane *plane)
{
    if (plane->zpos.isValid()) {
        return plane->zpos.value();
    } else {
        switch (plane->type.enumValue()) {
        case DrmPlane::TypeIndex::Primary:
            return 0;
        case DrmPlane::TypeIndex::Overlay:
            return 1;
        case DrmPlane::TypeIndex::Cursor:
            return 255;
        }
        return 0;
    }
}

static int determineMinZpos(DrmPlane *plane)
{
    if (plane->zpos.isValid()) {
        return plane->zpos.minValue();
    } else {
        return determineZpos(plane);
    }
}

static int determineMaxZpos(DrmPlane *plane)
{
    if (plane->zpos.isValid()) {
        return plane->zpos.maxValue();
    } else {
        return determineZpos(plane);
    }
}

DrmPipelineLayer::DrmPipelineLayer(RenderBackend *backend, DrmPlane *plane)
    : OutputLayer(nullptr, planeToLayerType(plane, plane->type.enumValue()), determineZpos(plane), determineMinZpos(plane), determineMaxZpos(plane))
    , m_plane(plane)
    , m_surface(plane->gpu(), backend, targetFor(plane->gpu(), plane->type.enumValue()))
{
}

DrmPipelineLayer::DrmPipelineLayer(RenderBackend *backend, DrmGpu *gpu, DrmPlane::TypeIndex type)
    : OutputLayer(nullptr, planeToLayerType(nullptr, type))
    , m_surface(gpu, backend, targetFor(gpu, type))
{
}

std::optional<OutputLayerBeginFrameInfo> DrmPipelineLayer::beginFrame(OutputFrame *frame)
{
    m_scanoutBuffer.reset();
    const bool tearing = frame && (frame->presentationMode() == PresentationMode::Async || frame->presentationMode() == PresentationMode::AdaptiveAsync);
    const auto formats = tearing && !supportedAsyncDrmFormats().isEmpty() ? supportedAsyncDrmFormats() : supportedDrmFormats();
    return m_surface.startRendering(targetRect().size(),
                                    drmOutput()->transform().combine(OutputTransform::FlipY),
                                    formats,
                                    drmOutput()->layerBlendingColor(),
                                    drmOutput()->colorPowerTradeoff(),
                                    m_requiredAlphaBits);
}

bool DrmPipelineLayer::endFrame(const Region &damagedDeviceRegion, FileDescriptor &&sync, OutputFrame *frame)
{
    return m_surface.endRendering(damagedDeviceRegion, std::move(sync), frame);
}

bool DrmPipelineLayer::preparePresentationTest()
{
    if (m_type != OutputLayerType::Primary && drmOutput()->shouldDisableNonPrimaryPlanes()) {
        return false;
    }
    m_scanoutBuffer.reset();
    return m_surface.renderTestBuffer(targetRect().size(), supportedDrmFormats(), drmOutput()->nextState().colorPowerTradeoff, m_requiredAlphaBits) != nullptr;
}

static const auto s_allowHardwareRotation = environmentVariableBoolValue("KWIN_ENABLE_HW_ROTATION");
static const bool s_directScanoutDisabled = environmentVariableBoolValue("KWIN_DRM_NO_DIRECT_SCANOUT").value_or(false);

bool DrmPipelineLayer::earlyScanoutChecks()
{
    if (s_directScanoutDisabled) {
        return false;
    }
    if (m_type != OutputLayerType::Primary && drmOutput()->shouldDisableNonPrimaryPlanes()) {
        return false;
    }
    if (gpu()->needsModeset()) {
        // don't do direct scanout with modeset, it might lead to locking
        // the hardware to some buffer format we can't switch away from
        return false;
    }
    if (drmOutput()->needsShadowBuffer()) {
        // while there are cases where this could still work (if the client prepares the buffer to match the output exactly)
        // it's likely not worth making this code more complicated to handle those edge cases
        return false;
    }
    if (!m_colorPipeline.isIdentity()) {
        if (!m_plane || drmOutput()->colorPowerTradeoff() == BackendOutput::ColorPowerTradeoff::PreferAccuracy) {
            return false;
        }
        const auto pipelines = m_plane->colorPipelines();
        const bool match = std::ranges::any_of(pipelines, [this](DrmColorOp *colorop) {
            return colorop->colorOp()->matchPipeline(gpu(), m_colorPipeline);
        });
        if (!match) {
            return false;
        }
    }
    // kernel documentation says that
    // "Devices that don’t support subpixel plane coordinates can ignore the fractional part."
    // so we need to make sure that doesn't cause a difference vs the composited result
    if (sourceRect() != sourceRect().toRect()) {
        return false;
    }
    if (offloadTransform() != OutputTransform::Kind::Normal) {
        // hardware rotation is broken on AMD GPUs that don't support modifiers,
        // see https://gitlab.freedesktop.org/drm/amd/-/work_items/5403
        if (!s_allowHardwareRotation.value_or(!gpu()->drmDevice()->isAmdgpu() || gpu()->addFB2ModifiersSupported())) {
            return false;
        }
        if (!m_plane || !m_plane->supportsTransformation(offloadTransform())) {
            return false;
        }
    }
    return true;
}

bool DrmPipelineLayer::importScanoutBuffer(GraphicsBuffer *buffer, const std::shared_ptr<OutputFrame> &frame)
{
    m_scanoutBuffer = gpu()->importBuffer(buffer, FileDescriptor{});
    if (m_scanoutBuffer) {
        m_surface.forgetDamage(); // TODO: Use absolute frame sequence numbers for indexing the DamageJournal. It's more flexible and less error-prone
    }
    return m_scanoutBuffer != nullptr;
}

SurfaceItem *DrmPipelineLayer::presentedDirectScanoutItem() const
{
    return m_scanoutBuffer ? m_scanoutCandidate.data() : nullptr;
}

std::shared_ptr<DrmFramebuffer> DrmPipelineLayer::currentBuffer() const
{
    return m_scanoutBuffer ? m_scanoutBuffer : m_surface.currentBuffer();
}

void DrmPipelineLayer::releaseBuffers()
{
    m_scanoutBuffer.reset();
    m_surface.destroyResources();
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

FormatModifierMap DrmPipelineLayer::supportedDrmFormats() const
{
    if (m_plane) {
        if (m_plane->gpu()->forceLowBandwidthMode() && m_type == OutputLayerType::Primary) {
            return m_plane->lowBandwidthFormats();
        } else {
            return m_plane->formats();
        }
    } else if (m_type == OutputLayerType::CursorOnly) {
        return s_legacyCursorFormats;
    } else {
        return s_legacyFormats;
    }
}

FormatModifierMap DrmPipelineLayer::supportedAsyncDrmFormats() const
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
    } else if (m_type == OutputLayerType::CursorOnly && pipeline()) {
        return {gpu()->cursorSize()};
    } else {
        return {};
    }
}

}
