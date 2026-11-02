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
#include "drm_gpu.h"
#include "drm_output.h"
#include "drm_pipeline.h"
#include "multigpuswapchain.h"
#include "scene/surfaceitem.h"
#include "swapchain.h"
#include "utils/envvar.h"

#include <drm_fourcc.h>

namespace KWin
{

static const FormatModifierMap s_legacyFormats = {{DRM_FORMAT_XRGB8888, {DRM_FORMAT_MOD_INVALID, DRM_FORMAT_MOD_LINEAR}}};
static const FormatModifierMap s_legacyCursorFormats = {{DRM_FORMAT_ARGB8888, {DRM_FORMAT_MOD_LINEAR}}};

static DrmPipelineLayer::BufferTarget targetFor(DrmGpu *gpu, DrmPlane::TypeIndex planeType)
{
    if ((!gpu->atomicModeSetting() || gpu->drmDevice()->isVirtualMachine()) && planeType == DrmPlane::TypeIndex::Cursor) {
        return DrmPipelineLayer::BufferTarget::Dumb;
    } else {
        return DrmPipelineLayer::BufferTarget::Normal;
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
    , m_backend(backend)
    , m_gpu(plane->gpu())
    , m_plane(plane)
    , m_requestedBufferTarget(targetFor(m_gpu, plane->type.enumValue()))
{
    connect(m_gpu, &DrmGpu::renderDeviceChanged, this, &DrmPipelineLayer::destroyResources);
}

DrmPipelineLayer::DrmPipelineLayer(RenderBackend *backend, DrmGpu *gpu, DrmPlane::TypeIndex type)
    : OutputLayer(nullptr, planeToLayerType(nullptr, type))
    , m_backend(backend)
    , m_gpu(gpu)
    , m_requestedBufferTarget(targetFor(gpu, type))
{
    connect(gpu, &DrmGpu::renderDeviceChanged, this, &DrmPipelineLayer::destroyResources);
}

DrmPipelineLayer::~DrmPipelineLayer()
{
}

std::optional<OutputLayerBeginFrameInfo> DrmPipelineLayer::beginFrame(OutputFrame *frame)
{
    m_scanoutBuffer.reset();
    const bool tearing = frame && (frame->presentationMode() == PresentationMode::Async || frame->presentationMode() == PresentationMode::AdaptiveAsync);
    const auto formats = tearing && !supportedAsyncDrmFormats().isEmpty() ? supportedAsyncDrmFormats() : supportedDrmFormats();
    const QSize &bufferSize = targetRect().size();
    const OutputTransform transformation = drmOutput()->transform().combine(OutputTransform::FlipY);

    if (!checkSurface(bufferSize, formats, drmOutput()->colorPowerTradeoff(), m_requiredAlphaBits)) {
        return std::nullopt;
    }
    m_oldSurface.reset();

    auto slot = m_surface->swapchain->acquire();
    if (!slot) {
        return std::nullopt;
    }

    m_surface->currentSlot = slot;
    m_surface->transformation = transformation;

    return OutputLayerBeginFrameInfo{
        .renderTarget = RenderTarget(slot->buffer(), transformation, drmOutput()->layerBlendingColor()),
        .repaint = m_surface->damageJournal.accumulate(slot->age(), Region::infinite()),
    };
}

bool DrmPipelineLayer::endFrame(const Region &damagedDeviceRegion, FileDescriptor &&sync, OutputFrame *frame)
{
    m_surface->damageJournal.add(damagedDeviceRegion);
    m_surface->swapchain->release(m_surface->currentSlot.get(), sync.duplicate());
    const auto buffer = importBuffer(m_surface.get(), m_surface->currentSlot.get(), std::move(sync), frame, damagedDeviceRegion);
    if (buffer) {
        m_surface->currentFramebuffer = buffer;
        return true;
    } else {
        return false;
    }
}

bool DrmPipelineLayer::preparePresentationTest()
{
    if (m_type != OutputLayerType::Primary && drmOutput()->shouldDisableNonPrimaryPlanes()) {
        return false;
    }
    m_scanoutBuffer.reset();
    if (checkSurface(targetRect().size(), supportedDrmFormats(), drmOutput()->nextState().colorPowerTradeoff, m_requiredAlphaBits)) {
        return m_surface->currentFramebuffer != nullptr;
    } else {
        return false;
    }
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
    if (m_gpu->needsModeset()) {
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
            return colorop->colorOp()->matchPipeline(m_gpu, m_colorPipeline);
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
        if (!s_allowHardwareRotation.value_or(!m_gpu->drmDevice()->isAmdgpu() || m_gpu->addFB2ModifiersSupported())) {
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
    m_scanoutBuffer = m_gpu->importBuffer(buffer, FileDescriptor{});
    if (m_scanoutBuffer && m_surface) {
        // TODO: Use absolute frame sequence numbers for indexing the DamageJournal. It's more flexible and less error-prone
        m_surface->damageJournal.clear();
    }
    return m_scanoutBuffer != nullptr;
}

SurfaceItem *DrmPipelineLayer::presentedDirectScanoutItem() const
{
    return m_scanoutBuffer ? m_scanoutCandidate.data() : nullptr;
}

std::shared_ptr<DrmFramebuffer> DrmPipelineLayer::currentBuffer() const
{
    if (m_scanoutBuffer) {
        return m_scanoutBuffer;
    } else if (m_surface) {
        return m_surface->currentFramebuffer;
    } else {
        return nullptr;
    }
}

void DrmPipelineLayer::releaseBuffers()
{
    m_scanoutBuffer.reset();
    destroyResources();
}

DrmPlane *DrmPipelineLayer::plane() const
{
    return m_plane;
}

DrmPipeline *DrmPipelineLayer::pipeline() const
{
    return drmOutput()->pipeline();
}

DrmOutput *DrmPipelineLayer::drmOutput() const
{
    return static_cast<DrmOutput *>(m_output.get());
}

FormatModifierMap DrmPipelineLayer::supportedDrmFormats() const
{
    if (m_plane) {
        if (m_gpu->forceLowBandwidthMode() && m_type == OutputLayerType::Primary) {
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
        return {m_gpu->cursorSize()};
    } else {
        return {};
    }
}
void DrmPipelineLayer::destroyResources()
{
    m_surface = {};
    m_oldSurface = {};
}

bool DrmPipelineLayer::checkSurface(const QSize &size, const FormatModifierMap &formats, BackendOutput::ColorPowerTradeoff tradeoff, uint32_t requiredAlphaBits)
{
    if (doesSurfaceFit(m_surface.get(), size, formats, tradeoff, requiredAlphaBits)) {
        return true;
    }
    if (doesSurfaceFit(m_oldSurface.get(), size, formats, tradeoff, requiredAlphaBits)) {
        m_surface = std::move(m_oldSurface);
        return true;
    }
    if (m_gpu->renderDevice() && m_gpu->renderDevice() != m_backend->renderDevice() && m_gpu->renderDevice()->isInReset()) {
        // avoid creating a suboptimal swapchain until the reset is complete
        return false;
    }
    if (auto newSurface = createSurface(size, formats, tradeoff, requiredAlphaBits)) {
        m_oldSurface = std::move(m_surface);
        if (m_oldSurface) {
            // FIXME: Use absolute frame sequence numbers for indexing the DamageJournal
            m_oldSurface->damageJournal.clear();
            m_oldSurface->swapchain->resetBufferAge();
            if (m_oldSurface->importSwapchain) {
                m_oldSurface->importSwapchain->resetDamageTracking();
            }
        }
        m_surface = std::move(newSurface);
        return true;
    }
    return false;
}

bool DrmPipelineLayer::doesSurfaceFit(Surface *surface, const QSize &size, const FormatModifierMap &formats, BackendOutput::ColorPowerTradeoff tradeoff, uint32_t requiredAlphaBits) const
{
    if (!surface || surface->needsRecreation || !surface->swapchain || surface->swapchain->size() != size) {
        return false;
    }
    if (surface->tradeoff != tradeoff || surface->requiredAlphaBits != requiredAlphaBits) {
        // TODO requiredAlphaBits could be a bit more conservative with reallocations?
        return false;
    }
    if (surface->importMode == MultiGpuImportMode::None && surface->bufferTarget == BufferTarget::Normal) {
        return formats.containsFormat(surface->swapchain->format(), surface->swapchain->modifier());
    } else {
        return !surface->importSwapchain->needsRecreation()
            && formats.containsFormat(surface->importSwapchain->format(), surface->importSwapchain->modifier());
    }
}

std::unique_ptr<DrmPipelineLayer::Surface> DrmPipelineLayer::createSurface(const QSize &size, const FormatModifierMap &formats, BackendOutput::ColorPowerTradeoff tradeoff, uint32_t requiredAlphaBits) const
{
    const QList<FormatInfo> sortedFormats = OutputLayer::filterAndSortFormats(formats, requiredAlphaBits, tradeoff);

    // special case: the cursor plane needs linear, but not all GPUs (NVidia) can render to linear
    auto bufferTarget = m_requestedBufferTarget;
    if (m_gpu->renderDevice() == m_backend->renderDevice()) {
        const bool needsLinear = std::ranges::all_of(sortedFormats, [&formats](const FormatInfo &fmt) {
            const auto &mods = formats[fmt.drmFormat];
            return std::ranges::all_of(mods, [](uint64_t mod) {
                return mod == DRM_FORMAT_MOD_LINEAR;
            });
        });
        if (needsLinear) {
            const auto renderFormats = m_backend->supportedFormats();
            const bool noLinearSupport = std::ranges::none_of(sortedFormats, [&renderFormats](const auto &formatInfo) {
                return renderFormats.containsFormat(formatInfo.drmFormat, DRM_FORMAT_MOD_LINEAR);
            });
            if (noLinearSupport) {
                bufferTarget = BufferTarget::Dumb;
            }
        }
    }

    const auto doTestFormats = [this, &size, &formats, bufferTarget, tradeoff, requiredAlphaBits](const QList<FormatInfo> &gbmFormats, MultiGpuImportMode importMode) -> std::unique_ptr<Surface> {
        for (const auto &format : gbmFormats) {
            auto surface = createSurface(size, format.drmFormat, formats[format.drmFormat], importMode, bufferTarget, tradeoff, requiredAlphaBits);
            if (surface) {
                return surface;
            }
        }
        return nullptr;
    };
    if (m_gpu->renderDevice() == m_backend->renderDevice()) {
        return doTestFormats(sortedFormats, MultiGpuImportMode::None);
    } else {
        return doTestFormats(sortedFormats, MultiGpuImportMode::GpuCopy);
    }
}

std::unique_ptr<DrmPipelineLayer::Surface> DrmPipelineLayer::createSurface(const QSize &size, uint32_t format, const ModifierList &modifiers, MultiGpuImportMode importMode, BufferTarget bufferTarget, BackendOutput::ColorPowerTradeoff tradeoff, uint32_t requiredAlphaBits) const
{
    auto ret = std::make_unique<Surface>();
    ModifierList renderModifiers = m_backend->supportedFormats()[format];

    if (bufferTarget == BufferTarget::Dumb || importMode == MultiGpuImportMode::GpuCopy) {
        if (!m_backend->renderDevice() || !m_gpu->renderDevice()) {
            return nullptr;
        }
        GraphicsBufferOptions options{
            .size = size,
            .format = format,
            .modifiers = renderModifiers,
            .software = bufferTarget == BufferTarget::Dumb,
            .scanout = true,
        };
        auto copy = MultiGpuSwapchain::createForScanout(m_backend->renderDevice(), m_gpu->drmDevice(), options, FormatModifierMap{{format, modifiers}});
        if (!copy) {
            return nullptr;
        }
        ret->importSwapchain = std::move(copy->swapchain);
        renderModifiers = copy->importModifiers;
    } else {
        renderModifiers.intersect(modifiers);
    }
    if (renderModifiers.empty()) {
        return nullptr;
    }
    ret->bufferTarget = bufferTarget;
    ret->importMode = importMode;
    GraphicsBufferOptions options{
        .size = size,
        .format = format,
        .modifiers = renderModifiers,
        .software = false,
        .scanout = importMode == MultiGpuImportMode::None && bufferTarget == BufferTarget::Normal,
    };
    if (importMode == MultiGpuImportMode::None && bufferTarget == BufferTarget::Normal) {
        ret->swapchain = Swapchain::create(m_gpu->drmDevice()->allocator(), options);
    } else {
        ret->swapchain = Swapchain::create(m_backend->renderDevice()->allocator(), options);
    }
    ret->tradeoff = tradeoff;
    ret->requiredAlphaBits = requiredAlphaBits;
    if (!ret->swapchain) {
        return nullptr;
    }
    if (!renderTestBuffer(ret.get())) {
        return nullptr;
    }
    return ret;
}

std::shared_ptr<DrmFramebuffer> DrmPipelineLayer::renderTestBuffer(Surface *surface) const
{
    auto slot = surface->swapchain->acquire();
    if (!slot) {
        return nullptr;
    }
    FileDescriptor sync;
    if (!m_gpu->atomicModeSetting()) {
        sync = m_gpu->renderDevice()->clearBuffer(slot->buffer());
    }
    if (const auto ret = importBuffer(surface, slot.get(), std::move(sync), nullptr, Region::infinite())) {
        // clear the render journal, because this was just a nonsense frame
        if (surface->importSwapchain) {
            surface->importSwapchain->resetDamageTracking();
        }
        surface->currentSlot = slot;
        surface->currentFramebuffer = ret;
        return ret;
    } else {
        return nullptr;
    }
}

std::shared_ptr<DrmFramebuffer> DrmPipelineLayer::importBuffer(Surface *surface, SwapchainSlot *slot, FileDescriptor &&readFence, OutputFrame *frame, const Region &damagedDeviceRegion) const
{
    if (surface->importSwapchain) {
        // The source texture transform looks as follows: output transform + flip-y transform. The
        // flip-y transform is added by the render backend to handle the OpenGL render target origin
        // being in the bottom-left corner.
        //
        // As is, without the flip-y transform, the top-left corner of the graphics buffer will
        // map to the bottom-left corner of the render target and the final image will look upside down.
        //
        // .combine(FlipY) undoes the flip-y transform in "output transform + flip-y" so we only end
        // with the output transform and the bufferDamage has its origin in the top-left corner rather
        // than in the bottom-left corner.
        const OutputTransform mapping = surface->transformation.combine(OutputTransform::FlipY);
        const QSize orientedSize = mapping.map(slot->buffer()->size());
        const Region bufferDamage = mapping.map(damagedDeviceRegion, orientedSize);

        auto imported = surface->importSwapchain->copyRgbBuffer(slot->buffer(), bufferDamage, std::move(readFence), frame, slot->releasePoint());
        if (!imported) {
            // this is probably caused by a GPU reset, let's not take any chances
            surface->needsRecreation = true;
            return nullptr;
        }
        return m_gpu->importBuffer(imported->buffer, std::move(imported->sync));
    } else {
        const auto ret = m_gpu->importBuffer(slot->buffer(), std::move(readFence));
        if (!ret) {
            qCWarning(KWIN_DRM, "Failed to create framebuffer: %s", strerror(errno));
        }
        return ret;
    }
}

}

#include "moc_drm_layer.cpp"
