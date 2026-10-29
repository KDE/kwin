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
#include "opengl/glrendertimequery.h"
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

static const ModifierList linearModifier = {DRM_FORMAT_MOD_LINEAR};
static const ModifierList implicitModifier = {DRM_FORMAT_MOD_INVALID};
static const QList<uint32_t> cpuCopyFormats = {DRM_FORMAT_ARGB8888, DRM_FORMAT_XRGB8888};

static const bool bufferAgeEnabled = environmentVariableBoolValue("KWIN_USE_BUFFER_AGE").value_or(true);
static const bool s_forceMGPUSync = environmentVariableBoolValue("KWIN_DRM_FORCE_GL_FINISH_MGPU_COPY").value_or(false);
static const bool s_forcePresentSync = environmentVariableBoolValue("KWIN_DRM_FORCE_GL_FINISH_PRESENT").value_or(false);

static gbm_format_name_desc formatName(uint32_t format)
{
    gbm_format_name_desc ret;
    gbm_format_get_name(format, &ret);
    return ret;
}

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
                                                                            const std::shared_ptr<ColorDescription> &blendingColor,
                                                                            const std::shared_ptr<ColorDescription> &layerBlendingColor,
                                                                            const std::shared_ptr<IccProfile> &iccProfile,
                                                                            const Colorimetry &wireColor, const TransferFunction::Type &wireTransfer,
                                                                            double scale, BackendOutput::ColorPowerTradeoff tradeoff,
                                                                            bool useShadowBuffer, uint32_t requiredAlphaBits)
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
    m_surface->scale = scale;
    m_surface->transformation = transformation;

    if (m_surface->blendingColor != blendingColor
        || m_surface->layerBlendingColor != layerBlendingColor
        || m_surface->iccProfile != iccProfile
        || m_surface->wireColor != wireColor
        || m_surface->wireTransfer != wireTransfer) {
        m_surface->damageJournal.clear();
        m_surface->shadowDamageJournal.clear();
        m_surface->needsShadowBuffer = useShadowBuffer;
        m_surface->blendingColor = blendingColor;
        m_surface->layerBlendingColor = layerBlendingColor;
        m_surface->iccProfile = iccProfile;
        m_surface->wireColor = wireColor;
        m_surface->wireTransfer = wireTransfer;
        if (iccProfile) {
            if (!m_surface->iccShader) {
                m_surface->iccShader = IccShader::create();
                if (!m_surface->iccShader) {
                    qCWarning(KWIN_DRM) << "Failed to load the ICC shader.";
                }
            }
        } else {
            m_surface->iccShader.reset();
        }
    }

    m_surface->compositingTimeQuery = GLRenderTimeQuery::begin(m_surface->context);
    if (m_surface->needsShadowBuffer) {
        if (!m_surface->shadowSwapchain || m_surface->shadowSwapchain->size() != m_surface->swapchain->size()) {
            const auto formats = m_eglBackend->eglDisplayObject()->nonExternalOnlySupportedDrmFormats();
            const QList<FormatInfo> sortedFormats = OutputLayer::filterAndSortFormats(formats, requiredAlphaBits, tradeoff);
            for (const auto format : sortedFormats) {
                GraphicsBufferOptions options{
                    .size = m_surface->swapchain->size(),
                    .format = format.drmFormat,
                    .modifiers = formats[format.drmFormat],
                    .software = false,
                    .scanout = false,
                };
                m_surface->shadowSwapchain = Swapchain::create(m_eglBackend->renderDevice()->allocator(), options);
                if (m_surface->shadowSwapchain) {
                    break;
                }
            }
        }
        if (!m_surface->shadowSwapchain) {
            qCCritical(KWIN_DRM) << "Failed to create shadow swapchain!";
            return std::nullopt;
        }
        m_surface->currentShadowSlot = m_surface->shadowSwapchain->acquire();
        if (!m_surface->currentShadowSlot) {
            return std::nullopt;
        }

        auto shadowFbo = m_surface->context->importDmaBufAsFbo(slot->buffer());
        if (!shadowFbo) {
            return std::nullopt;
        }

        shadowFbo->colorAttachment()->setContentTransform(transformation);
        return OutputLayerBeginFrameInfo{
            .renderTarget = RenderTarget(shadowFbo, m_surface->blendingColor),
            .repaint = bufferAgeEnabled ? m_surface->shadowDamageJournal.accumulate(m_surface->currentShadowSlot->age(), Region::infinite()) : Region::infinite(),
        };
    } else {
        m_surface->shadowSwapchain.reset();
        m_surface->currentShadowSlot.reset();
        return OutputLayerBeginFrameInfo{
            .renderTarget = RenderTarget(fbo, m_surface->blendingColor),
            .repaint = bufferAgeEnabled ? m_surface->damageJournal.accumulate(slot->age(), Region::infinite()) : Region::infinite(),
        };
    }
}

bool EglGbmLayerSurface::endRendering(const Region &damagedDeviceRegion, OutputFrame *frame)
{
    if (m_surface->needsShadowBuffer) {
        const Region deviceRepaint = damagedDeviceRegion | m_surface->damageJournal.accumulate(m_surface->currentSlot->age(), Region::infinite());
        m_surface->damageJournal.add(damagedDeviceRegion);
        m_surface->shadowDamageJournal.add(damagedDeviceRegion);

        auto fbo = m_surface->context->importDmaBufAsFbo(m_surface->currentSlot->buffer());
        auto shadowFbo = m_surface->context->importDmaBufAsFbo(m_surface->currentShadowSlot->buffer());

        const auto mapping = shadowFbo->colorAttachment()->contentTransform();
        const QSize rotatedSize = mapping.map(m_surface->swapchain->size());
        const Region repaint = mapping.map(deviceRepaint & Rect(QPoint(), rotatedSize), rotatedSize);

        GLFramebuffer::pushFramebuffer(fbo);

        ShaderBinder binder = m_surface->iccShader ? ShaderBinder(m_surface->iccShader->shader()) : ShaderBinder(ShaderTrait::MapTexture | ShaderTrait::TransformColorspace);
        // this transform is absolute colorimetric, whitepoint adjustment is done in compositing already
        if (m_surface->iccShader) {
            m_surface->iccShader->setUniforms(m_surface->iccProfile, m_surface->blendingColor,
                                              m_surface->wireColor, m_surface->wireTransfer,
                                              RenderingIntent::AbsoluteColorimetricNoAdaptation);
        } else {
            binder.shader()->setColorspaceUniforms(m_surface->blendingColor, m_surface->layerBlendingColor, RenderingIntent::AbsoluteColorimetricNoAdaptation);
        }
        QMatrix4x4 mat;
        mat.scale(1, -1);
        mat.ortho(QRectF(QPointF(), fbo->size()));
        binder.shader()->setUniform(GLShader::Mat4Uniform::ModelViewProjectionMatrix, mat);
        glDisable(GL_BLEND);

        shadowFbo->colorAttachment()->bind();
        m_surface->context->renderRegion(repaint, [&]() {
            shadowFbo->colorAttachment()->render(fbo->size());
        });
        shadowFbo->colorAttachment()->unbind();

        EGLNativeFence fence(m_surface->context->displayObject());
        m_surface->shadowSwapchain->release(m_surface->currentShadowSlot.get(), fence.takeFileDescriptor());

        GLFramebuffer::popFramebuffer();
    } else {
        m_surface->damageJournal.add(damagedDeviceRegion);
    }
    if (frame && m_surface->compositingTimeQuery) {
        m_surface->compositingTimeQuery->end();
        frame->addRenderTimeQuery(std::move(m_surface->compositingTimeQuery));
    }
    glFlush();
    EGLNativeFence sourceFence(m_eglBackend->eglDisplayObject());
    if (!sourceFence.isValid() || s_forcePresentSync) {
        // llvmpipe doesn't do synchronization properly: https://gitlab.freedesktop.org/mesa/mesa/-/issues/9375
        // and NVidia doesn't support implicit sync
        glFinish();
    }
    m_surface->swapchain->release(m_surface->currentSlot.get(), sourceFence.fileDescriptor().duplicate());
    const auto buffer = importBuffer(m_surface.get(), m_surface->currentSlot.get(), sourceFence.takeFileDescriptor(), frame, damagedDeviceRegion);
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

const std::shared_ptr<ColorDescription> &EglGbmLayerSurface::colorDescription() const
{
    if (m_surface) {
        return m_surface->blendingColor;
    } else {
        return ColorDescription::sRGB;
    }
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
        m_surface->shadowDamageJournal.clear();
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
            m_oldSurface->shadowDamageJournal.clear();
            m_oldSurface->swapchain->resetBufferAge();
            if (m_oldSurface->shadowSwapchain) {
                m_oldSurface->shadowSwapchain->resetBufferAge();
            }
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
    if (surface->bufferTarget == BufferTarget::Dumb) {
        return formats.contains(surface->importDumbSwapchain->format());
    }
    switch (surface->importMode) {
    case MultiGpuImportMode::None:
        return formats.containsFormat(surface->swapchain->format(), surface->swapchain->modifier());
    case MultiGpuImportMode::DumbBuffer:
        return formats.contains(surface->importDumbSwapchain->format());
    case MultiGpuImportMode::GpuCopy:
        return !surface->importSwapchain->needsRecreation()
            && formats.containsFormat(surface->importSwapchain->format(), surface->importSwapchain->modifier());
    }
    Q_UNREACHABLE();
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
    }
    // special case, we're using different display devices but the same render device
    const auto device = m_gpu->renderDevice();
    if (device && !device->eglDisplay()->renderNode().isEmpty() && device->eglDisplay()->renderNode() == m_eglBackend->eglDisplayObject()->renderNode()) {
        if (auto surface = doTestFormats(sortedFormats, MultiGpuImportMode::None)) {
            return surface;
        }
    }
    if (auto surface = doTestFormats(sortedFormats, MultiGpuImportMode::GpuCopy)) {
        // qCDebug(KWIN_DRM) << "chose egl import with format" << formatName(surface->gbmSwapchain->format()).name << "and modifier" << surface->gbmSwapchain->modifier();
        return surface;
    }
    if (auto surface = doTestFormats(sortedFormats, MultiGpuImportMode::DumbBuffer)) {
        qCDebug(KWIN_DRM) << "chose cpu import with format" << formatName(surface->swapchain->format()).name << "and modifier" << surface->swapchain->modifier();
        return surface;
    }
    return nullptr;
}

std::unique_ptr<EglGbmLayerSurface::Surface> EglGbmLayerSurface::createSurface(const QSize &size, uint32_t format, const ModifierList &modifiers, MultiGpuImportMode importMode, BufferTarget bufferTarget, BackendOutput::ColorPowerTradeoff tradeoff, uint32_t requiredAlphaBits) const
{
    const bool cpuCopy = importMode == MultiGpuImportMode::DumbBuffer || bufferTarget == BufferTarget::Dumb;
    auto ret = std::make_unique<Surface>();
    ModifierList renderModifiers = m_eglBackend->eglDisplayObject()->nonExternalOnlySupportedDrmFormats()[format];
    if (cpuCopy) {
        if (!cpuCopyFormats.contains(format)) {
            return nullptr;
        }
        ret->importDumbSwapchain = std::make_unique<QPainterSwapchain>(m_gpu->drmDevice()->allocator(), size, format, true);
        if (!ret->importDumbSwapchain) {
            return nullptr;
        }
    } else if (importMode == MultiGpuImportMode::GpuCopy) {
        if (!m_eglBackend->renderDevice()
            || !m_gpu->renderDevice()
            || m_eglBackend->renderDevice()->isSoftwareDevice()
            || m_gpu->renderDevice()->isSoftwareDevice()) {
            return nullptr;
        }
        auto copy = MultiGpuSwapchain::createForScanout(m_eglBackend->renderDevice(), m_gpu->drmDevice(), format, renderModifiers, size, FormatModifierMap{{format, modifiers}});
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
    if (!m_gpu->atomicModeSetting()) {
        auto fbo = surface->context->importDmaBufAsFbo(surface->currentSlot->buffer());
        EglContext::currentContext()->pushFramebuffer(fbo);
        glClearColor(0, 0, 0, 0);
        glClear(GL_COLOR_BUFFER_BIT);
        EglContext::currentContext()->popFramebuffer();
    }
    if (const auto ret = importBuffer(surface, slot.get(), FileDescriptor{}, nullptr, Region::infinite())) {
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
    if (surface->bufferTarget == BufferTarget::Dumb || surface->importMode == MultiGpuImportMode::DumbBuffer) {
        return importWithCpu(surface, slot, frame);
    } else if (surface->importMode == MultiGpuImportMode::GpuCopy) {
        return importWithCopy(surface, slot, std::move(readFence), frame, damagedDeviceRegion);
    } else {
        const auto ret = m_gpu->importBuffer(slot->buffer(), std::move(readFence));
        if (!ret) {
            qCWarning(KWIN_DRM, "Failed to create framebuffer: %s", strerror(errno));
        }
        return ret;
    }
}

std::shared_ptr<DrmFramebuffer> EglGbmLayerSurface::importWithCopy(Surface *surface, SwapchainSlot *source, FileDescriptor &&readFence, OutputFrame *frame, const Region &damagedDeviceRegion) const
{
    Q_ASSERT(surface->importSwapchain);

    const auto renderDevice = m_gpu->renderDevice();
    // older versions of the NVidia proprietary driver support neither implicit sync nor EGL_ANDROID_native_fence_sync
    if (!readFence.isValid() || !renderDevice->eglDisplay()->supportsNativeFence() || s_forceMGPUSync) {
        glFinish();
    }

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
    const QSize orientedSize = mapping.map(source->buffer()->size());
    const Region bufferDamage = mapping.map(damagedDeviceRegion, orientedSize);
    auto imported = surface->importSwapchain->copyRgbBuffer(source->buffer(), bufferDamage, std::move(readFence), frame, source->releasePoint());
    if (!imported) {
        // this is probably caused by a GPU reset, let's not take any chances
        surface->needsRecreation = true;
        return nullptr;
    }
    return m_gpu->importBuffer(imported->buffer, std::move(imported->sync));
}

std::shared_ptr<DrmFramebuffer> EglGbmLayerSurface::importWithCpu(Surface *surface, SwapchainSlot *source, OutputFrame *frame) const
{
    std::unique_ptr<CpuRenderTimeQuery> copyTime;
    if (frame) {
        copyTime = std::make_unique<CpuRenderTimeQuery>();
    }
    Q_ASSERT(surface->importDumbSwapchain);
    const auto slot = surface->importDumbSwapchain->acquire();
    if (!slot) {
        qCWarning(KWIN_DRM) << "EglGbmLayerSurface::importWithCpu: failed to get a target dumb buffer";
        return nullptr;
    }
    const auto size = source->buffer()->size();
    const qsizetype srcStride = 4 * size.width();

    const auto &context = surface->context;
    auto fbo = context->importDmaBufAsFbo(surface->currentSlot->buffer());

    GLFramebuffer::pushFramebuffer(fbo);
    QImage *const dst = slot->view()->image();
    if (dst->bytesPerLine() == srcStride) {
        context->glReadnPixels(0, 0, dst->width(), dst->height(), GL_BGRA, GL_UNSIGNED_BYTE, dst->sizeInBytes(), dst->bits());
    } else {
        // there's padding, need to copy line by line
        if (surface->cpuCopyCache.size() != dst->size()) {
            surface->cpuCopyCache = QImage(dst->size(), QImage::Format_RGBA8888);
        }
        context->glReadnPixels(0, 0, dst->width(), dst->height(), GL_BGRA, GL_UNSIGNED_BYTE, surface->cpuCopyCache.sizeInBytes(), surface->cpuCopyCache.bits());
        for (int i = 0; i < dst->height(); i++) {
            std::memcpy(dst->scanLine(i), surface->cpuCopyCache.scanLine(i), srcStride);
        }
    }
    GLFramebuffer::popFramebuffer();

    const auto ret = m_gpu->importBuffer(slot->buffer(), FileDescriptor{});
    if (!ret) {
        qCWarning(KWIN_DRM, "Failed to create a framebuffer: %s", strerror(errno));
    }
    surface->importDumbSwapchain->release(slot);
    if (frame) {
        copyTime->end();
        frame->addRenderTimeQuery(std::move(copyTime));
    }
    return ret;
}

}

#include "moc_drm_egl_layer_surface.cpp"
