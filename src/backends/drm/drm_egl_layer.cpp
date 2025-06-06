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

static EglGbmLayerSurface::BufferTarget targetFor(DrmPipeline *pipeline, DrmPlane::TypeIndex planeType)
{
    if (planeType != DrmPlane::TypeIndex::Cursor) {
        return EglGbmLayerSurface::BufferTarget::Normal;
    }
    if (pipeline->gpu()->atomicModeSetting() && !pipeline->gpu()->isVirtualMachine()) {
        return EglGbmLayerSurface::BufferTarget::Linear;
    }
    return EglGbmLayerSurface::BufferTarget::Dumb;
}

EglGbmLayer::EglGbmLayer(EglGbmBackend *eglBackend, DrmPipeline *pipeline, DrmPlane::TypeIndex type)
    : DrmPipelineLayer(pipeline, type)
    , m_surface(pipeline->gpu(), eglBackend, targetFor(pipeline, type), type == DrmPlane::TypeIndex::Primary ? EglGbmLayerSurface::FormatOption::PreferAlpha : EglGbmLayerSurface::FormatOption::RequireAlpha)
{
}

std::optional<OutputLayerBeginFrameInfo> EglGbmLayer::doBeginFrame()
{
    if (m_type == DrmPlane::TypeIndex::Cursor && m_pipeline->output()->shouldDisableCursorPlane()) {
        return std::nullopt;
    }
    // note that this allows blending to happen in sRGB or PQ encoding with the cursor plane.
    // That's technically incorrect, but it looks okay and is intentionally allowed
    // as the hardware cursor is more important than an incorrectly blended cursor edge

    m_scanoutBuffer.reset();
    return m_surface.startRendering(targetRect().size(),
                                    m_pipeline->output()->transform().combine(OutputTransform::FlipY),
                                    m_pipeline->formats(m_type),
                                    m_pipeline->output()->blendingColor(),
                                    m_pipeline->output()->layerBlendingColor(),
                                    m_pipeline->output()->needsShadowBuffer() ? m_pipeline->iccProfile() : nullptr,
                                    m_pipeline->output()->scale(),
                                    m_pipeline->output()->colorPowerTradeoff(),
                                    m_pipeline->output()->needsShadowBuffer());
}

bool EglGbmLayer::doEndFrame(const QRegion &renderedRegion, const QRegion &damagedRegion, OutputFrame *frame)
{
    return m_surface.endRendering(damagedRegion, frame);
}

bool EglGbmLayer::preparePresentationTest()
{
    m_scanoutBuffer.reset();
    return m_surface.renderTestBuffer(targetRect().size(), m_pipeline->formats(m_type), m_pipeline->output()->colorPowerTradeoff()) != nullptr;
}

std::shared_ptr<GLTexture> EglGbmLayer::texture() const
{
    if (m_scanoutBuffer) {
        const auto ret = m_surface.eglBackend()->importDmaBufAsTexture(*m_scanoutBuffer->buffer()->dmabufAttributes());
        if (ret) {
            ret->setContentTransform(offloadTransform().combine(OutputTransform::FlipY));
        }
        return ret;
    } else {
        return m_surface.texture();
    }
}

ColorDescription EglGbmLayer::colorDescription() const
{
    if (m_scanoutBuffer) {
        return m_scanoutColor;
    } else {
        return m_surface.colorDescription();
    }
}

bool EglGbmLayer::doImportScanoutBuffer(GraphicsBuffer *buffer, const ColorDescription &color, RenderingIntent intent, const std::shared_ptr<OutputFrame> &frame)
{
    static const bool directScanoutDisabled = environmentVariableBoolValue("KWIN_DRM_NO_DIRECT_SCANOUT").value_or(false);
    if (directScanoutDisabled) {
        return false;
    }
    if (m_pipeline->gpu()->needsModeset()) {
        // don't do direct scanout with modeset, it might lead to locking
        // the hardware to some buffer format we can't switch away from
        return false;
    }
    if (m_pipeline->output()->needsShadowBuffer()) {
        // while there are cases where this could still work (if the client prepares the buffer to match the output exactly)
        // it's likely not worth making this code more complicated to handle those edge cases
        return false;
    }
    m_colorPipeline = ColorPipeline::create(color, m_pipeline->output()->layerBlendingColor(), intent);
    if (!m_colorPipeline.isIdentity() && m_pipeline->output()->colorPowerTradeoff() == Output::ColorPowerTradeoff::PreferAccuracy) {
        return false;
    }
    m_scanoutColor = color;
    // kernel documentation says that
    // "Devices that don’t support subpixel plane coordinates can ignore the fractional part."
    // so we need to make sure that doesn't cause a difference vs the composited result
    if (sourceRect() != sourceRect().toRect()) {
        return false;
    }
    const auto plane = m_type == DrmPlane::TypeIndex::Primary ? m_pipeline->crtc()->primaryPlane() : m_pipeline->crtc()->cursorPlane();
    if (offloadTransform() != OutputTransform::Kind::Normal && (!plane || !plane->supportsTransformation(offloadTransform()))) {
        return false;
    }
    // importing a buffer from another GPU without an explicit modifier can mess up the buffer format
    if (buffer->dmabufAttributes()->modifier == DRM_FORMAT_MOD_INVALID && m_pipeline->gpu()->platform()->gpuCount() > 1) {
        return false;
    }
    m_scanoutBuffer = m_pipeline->gpu()->importBuffer(buffer, FileDescriptor{});
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

DrmDevice *EglGbmLayer::scanoutDevice() const
{
    return m_pipeline->gpu()->drmDevice();
}

QHash<uint32_t, QList<uint64_t>> EglGbmLayer::supportedDrmFormats() const
{
    return m_pipeline->formats(m_type);
}

QHash<uint32_t, QList<uint64_t>> EglGbmLayer::supportedAsyncDrmFormats() const
{
    return m_pipeline->asyncFormats(m_type);
}

QList<QSize> EglGbmLayer::recommendedSizes() const
{
    return m_pipeline->recommendedSizes(m_type);
}
}
