/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2022 Xaver Hugl <xaver.hugl@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "drm_egl_layer.h"
#include "core/colorpipeline.h"
#include "core/iccprofile.h"
#include "drm_backend.h"
#include "drm_buffer.h"
#include "drm_crtc.h"
#include "drm_egl_backend.h"
#include "drm_gpu.h"
#include "drm_output.h"
#include "drm_pipeline.h"
#include "scene/surfaceitem_wayland.h"
#include "utils/envvar.h"
#include "wayland/surface.h"

#include <QRegion>
#include <drm_fourcc.h>
#include <errno.h>
#include <gbm.h>
#include <unistd.h>

namespace KWin
{

static EglGbmLayerSurface::BufferTarget targetFor(DrmGpu *gpu, DrmPlane::TypeIndex planeType)
{
    if ((!gpu->atomicModeSetting() || gpu->isVirtualMachine()) && planeType == DrmPlane::TypeIndex::Cursor) {
        return EglGbmLayerSurface::BufferTarget::Dumb;
    } else {
        return EglGbmLayerSurface::BufferTarget::Normal;
    }
}

EglGbmLayer::EglGbmLayer(EglGbmBackend *eglBackend, DrmPlane *plane)
    : DrmPipelineLayer(plane)
    , m_surface(plane->gpu(), eglBackend, targetFor(plane->gpu(), plane->type.enumValue()))
{
}

EglGbmLayer::EglGbmLayer(EglGbmBackend *eglBackend, DrmGpu *gpu, DrmPlane::TypeIndex type)
    : DrmPipelineLayer(type)
    , m_surface(gpu, eglBackend, targetFor(gpu, type))
{
}

std::optional<OutputLayerBeginFrameInfo> EglGbmLayer::doBeginFrame()
{
    m_scanoutBuffer.reset();
    return m_surface.startRendering(targetRect().size(),
                                    drmOutput()->transform().combine(OutputTransform::FlipY),
                                    supportedDrmFormats(),
                                    drmOutput()->blendingColor(),
                                    drmOutput()->layerBlendingColor(),
                                    drmOutput()->needsShadowBuffer() ? pipeline()->iccProfile() : nullptr,
                                    drmOutput()->scale(),
                                    drmOutput()->colorPowerTradeoff(),
                                    drmOutput()->needsShadowBuffer(),
                                    m_requiredAlphaBits);
}

bool EglGbmLayer::doEndFrame(const QRegion &renderedRegion, const QRegion &damagedRegion, OutputFrame *frame)
{
    return m_surface.endRendering(damagedRegion, frame);
}

bool EglGbmLayer::preparePresentationTest()
{
    if (m_type != OutputLayerType::Primary && drmOutput()->shouldDisableNonPrimaryPlanes()) {
        return false;
    }
    m_scanoutBuffer.reset();
    return m_surface.renderTestBuffer(targetRect().size(), supportedDrmFormats(), drmOutput()->colorPowerTradeoff(), m_requiredAlphaBits) != nullptr;
}

bool EglGbmLayer::importScanoutBuffer(GraphicsBuffer *buffer, const std::shared_ptr<OutputFrame> &frame)
{
    static const bool directScanoutDisabled = environmentVariableBoolValue("KWIN_DRM_NO_DIRECT_SCANOUT").value_or(false);
    if (directScanoutDisabled) {
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
    if (gpu() != gpu()->platform()->primaryGpu()) {
        // Disable direct scanout between GPUs, as
        // - there are some significant driver bugs with direct scanout from other GPUs,
        //   like https://gitlab.freedesktop.org/drm/amd/-/issues/2075
        // - with implicit modifiers, direct scanout on secondary GPUs
        //   is also very unlikely to yield the correct results.
        // TODO once we know what buffer a GPU is meant for, loosen this check again
        // Right now this just assumes all buffers are on the primary GPU
        return false;
    }
    if (!m_colorPipeline.isIdentity() && drmOutput()->colorPowerTradeoff() == Output::ColorPowerTradeoff::PreferAccuracy) {
        return false;
    }
    // kernel documentation says that
    // "Devices that don’t support subpixel plane coordinates can ignore the fractional part."
    // so we need to make sure that doesn't cause a difference vs the composited result
    if (sourceRect() != sourceRect().toRect()) {
        return false;
    }
    if (offloadTransform() != OutputTransform::Kind::Normal && (!m_plane || !m_plane->supportsTransformation(offloadTransform()))) {
        return false;
    }
    m_scanoutBuffer = gpu()->importBuffer(buffer, FileDescriptor{});
    if (m_scanoutBuffer) {
        m_surface.forgetDamage(); // TODO: Use absolute frame sequence numbers for indexing the DamageJournal. It's more flexible and less error-prone
    }
    return m_scanoutBuffer != nullptr;
}

std::shared_ptr<DrmFramebuffer> EglGbmLayer::currentBuffer() const
{
    return m_scanoutBuffer ? m_scanoutBuffer : m_surface.currentBuffer();
}

void EglGbmLayer::releaseBuffers()
{
    m_scanoutBuffer.reset();
    m_surface.destroyResources();
}
}
