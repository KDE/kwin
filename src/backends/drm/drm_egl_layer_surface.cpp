/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2022 Xaver Hugl <xaver.hugl@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "drm_egl_layer_surface.h"

#include "config-kwin.h"

#include "core/colortransformation.h"
#include "core/drm_formats.h"
#include "core/graphicsbufferview.h"
#include "core/iccprofile.h"
#include "core/renderdevice.h"
#include "drm_egl_backend.h"
#include "drm_gpu.h"
#include "drm_logging.h"
#include "multigpuswapchain.h"
#include "opengl/eglnativefence.h"
#include "opengl/gllut.h"
#include "opengl/icc_shader.h"
#include "qpainter/qpainterswapchain.h"
#include "swapchain.h"
#include "utils/envvar.h"

#include <drm_fourcc.h>
#include <errno.h>
#include <gbm.h>
#include <unistd.h>

namespace KWin
{

EglGbmLayerSurface::EglGbmLayerSurface(DrmGpu *gpu, EglGbmBackend *eglBackend, BufferTarget target)
    : m_gpu(gpu)
    , m_eglBackend(eglBackend)
    , m_requestedBufferTarget(target)
{
    connect(m_gpu, &DrmGpu::renderDeviceChanged, this, &EglGbmLayerSurface::destroyResources);
}

EglGbmLayerSurface::~EglGbmLayerSurface() = default;

EglGbmLayerSurface::Surface::~Surface()
{
    importSwapchain.reset();
    if (context) {
        (void)context->makeCurrent();
    }
}

void EglGbmLayerSurface::destroyResources()
{
    m_surface = {};
    m_oldSurface = {};
}

std::optional<OutputLayerBeginFrameInfo> EglGbmLayerSurface::startRendering(const QSize &bufferSize, OutputTransform transformation,
                                                                            const FormatModifierMap &formats,
                                                                            const std::shared_ptr<ColorDescription> &layerBlendingColor,
                                                                            BackendOutput::ColorPowerTradeoff tradeoff,
                                                                            uint32_t requiredAlphaBits)
{
    if (!checkSurface(bufferSize, formats, tradeoff, requiredAlphaBits)) {
        return std::nullopt;
    }
    m_oldSurface.reset();

    if (!m_eglBackend->openglContext()->makeCurrent()) {
        return std::nullopt;
    }

    auto slot = m_surface->swapchain->acquire();
    if (!slot) {
        return std::nullopt;
    }

    auto fbo = m_surface->context->importDmaBufAsFbo(slot->buffer());
    if (!fbo) {
        return std::nullopt;
    }

    if (fbo->colorAttachment()->contentTransform() != transformation) {
        m_surface->damageJournal.clear();
    }
    fbo->colorAttachment()->setContentTransform(transformation);
    m_surface->currentSlot = slot;
    m_surface->transformation = transformation;

    return OutputLayerBeginFrameInfo{
        .renderTarget = RenderTarget(fbo, layerBlendingColor),
        .repaint = m_surface->damageJournal.accumulate(slot->age(), Region::infinite()),
    };
}

bool EglGbmLayerSurface::endRendering(const Region &damagedDeviceRegion, FileDescriptor &&sync, OutputFrame *frame)
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

EglGbmBackend *EglGbmLayerSurface::eglBackend() const
{
    return m_eglBackend;
}

std::shared_ptr<DrmFramebuffer> EglGbmLayerSurface::currentBuffer() const
{
    return m_surface ? m_surface->currentFramebuffer : nullptr;
}

std::shared_ptr<DrmFramebuffer> EglGbmLayerSurface::renderTestBuffer(const QSize &bufferSize, const FormatModifierMap &formats, BackendOutput::ColorPowerTradeoff tradeoff, uint32_t requiredAlphaBits)
{
    EglContext *context = m_eglBackend->openglContext();
    if (!context->makeCurrent()) {
        qCWarning(KWIN_DRM) << "EglGbmLayerSurface::renderTestBuffer: failed to make opengl context current";
        return nullptr;
    }

    if (checkSurface(bufferSize, formats, tradeoff, requiredAlphaBits)) {
        return m_surface->currentFramebuffer;
    } else {
        return nullptr;
    }
}

void EglGbmLayerSurface::forgetDamage()
{
    if (m_surface) {
        m_surface->damageJournal.clear();
    }
}

bool EglGbmLayerSurface::checkSurface(const QSize &size, const FormatModifierMap &formats, BackendOutput::ColorPowerTradeoff tradeoff, uint32_t requiredAlphaBits)
{
    if (doesSurfaceFit(m_surface.get(), size, formats, tradeoff, requiredAlphaBits)) {
        return true;
    }
    if (doesSurfaceFit(m_oldSurface.get(), size, formats, tradeoff, requiredAlphaBits)) {
        m_surface = std::move(m_oldSurface);
        return true;
    }
    if (m_gpu->renderDevice() && m_gpu->renderDevice() != m_eglBackend->renderDevice() && m_gpu->renderDevice()->isInReset()) {
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

bool EglGbmLayerSurface::doesSurfaceFit(Surface *surface, const QSize &size, const FormatModifierMap &formats, BackendOutput::ColorPowerTradeoff tradeoff, uint32_t requiredAlphaBits) const
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

std::unique_ptr<EglGbmLayerSurface::Surface> EglGbmLayerSurface::createSurface(const QSize &size, const FormatModifierMap &formats, BackendOutput::ColorPowerTradeoff tradeoff, uint32_t requiredAlphaBits) const
{
    const QList<FormatInfo> sortedFormats = OutputLayer::filterAndSortFormats(formats, requiredAlphaBits, tradeoff);

    // special case: the cursor plane needs linear, but not all GPUs (NVidia) can render to linear
    auto bufferTarget = m_requestedBufferTarget;
    if (m_gpu->renderDevice() == m_eglBackend->renderDevice()) {
        const bool needsLinear = std::ranges::all_of(sortedFormats, [&formats](const FormatInfo &fmt) {
            const auto &mods = formats[fmt.drmFormat];
            return std::ranges::all_of(mods, [](uint64_t mod) {
                return mod == DRM_FORMAT_MOD_LINEAR;
            });
        });
        if (needsLinear) {
            const auto renderFormats = m_eglBackend->eglDisplayObject()->nonExternalOnlySupportedDrmFormats();
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
    if (m_gpu->renderDevice() == m_eglBackend->renderDevice()) {
        return doTestFormats(sortedFormats, MultiGpuImportMode::None);
    } else {
        return doTestFormats(sortedFormats, MultiGpuImportMode::GpuCopy);
    }
}

std::unique_ptr<EglGbmLayerSurface::Surface> EglGbmLayerSurface::createSurface(const QSize &size, uint32_t format, const ModifierList &modifiers, MultiGpuImportMode importMode, BufferTarget bufferTarget, BackendOutput::ColorPowerTradeoff tradeoff, uint32_t requiredAlphaBits) const
{
    auto ret = std::make_unique<Surface>();
    ModifierList renderModifiers = m_eglBackend->eglDisplayObject()->nonExternalOnlySupportedDrmFormats()[format];

    if (bufferTarget == BufferTarget::Dumb || importMode == MultiGpuImportMode::GpuCopy) {
        if (!m_eglBackend->renderDevice() || !m_gpu->renderDevice()) {
            return nullptr;
        }
        GraphicsBufferOptions options{
            .size = size,
            .format = format,
            .modifiers = renderModifiers,
            .software = bufferTarget == BufferTarget::Dumb,
            .scanout = true,
        };
        auto copy = MultiGpuSwapchain::createForScanout(m_eglBackend->renderDevice(), m_gpu->drmDevice(), options, FormatModifierMap{{format, modifiers}});
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
    ret->context = m_eglBackend->openglContextRef();
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
        ret->swapchain = Swapchain::create(m_eglBackend->renderDevice()->allocator(), options);
    }
    ret->tradeoff = tradeoff;
    ret->requiredAlphaBits = requiredAlphaBits;
    if (!ret->swapchain) {
        return nullptr;
    }
    if (!doRenderTestBuffer(ret.get())) {
        return nullptr;
    }
    return ret;
}

std::shared_ptr<DrmFramebuffer> EglGbmLayerSurface::doRenderTestBuffer(Surface *surface) const
{
    auto slot = surface->swapchain->acquire();
    if (!slot) {
        return nullptr;
    }
    FileDescriptor sync;
    if (!m_gpu->atomicModeSetting()) {
        sync = m_gpu->renderDevice()->clearBuffer(surface->currentSlot->buffer());
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

std::shared_ptr<DrmFramebuffer> EglGbmLayerSurface::importBuffer(Surface *surface, SwapchainSlot *slot, FileDescriptor &&readFence, OutputFrame *frame, const Region &damagedDeviceRegion) const
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

#include "moc_drm_egl_layer_surface.cpp"
