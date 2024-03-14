/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2022 Xaver Hugl <xaver.hugl@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "drm_egl_layer_surface.h"

#include "config-kwin.h"

#include "core/colortransformation.h"
#include "core/graphicsbufferview.h"
#include "core/iccprofile.h"
#include "drm_egl_backend.h"
#include "drm_gpu.h"
#include "drm_logging.h"
#include "icc_shader.h"
#include "opengl/eglnativefence.h"
#include "opengl/eglswapchain.h"
#include "opengl/gllut.h"
#include "opengl/glrendertimequery.h"
#include "platformsupport/scenes/qpainter/qpainterswapchain.h"
#include "utils/drm_format_helper.h"

#include <drm_fourcc.h>
#include <errno.h>
#include <gbm.h>
#include <unistd.h>

namespace KWin
{

static const QList<uint64_t> linearModifier = {DRM_FORMAT_MOD_LINEAR};
static const QList<uint64_t> implicitModifier = {DRM_FORMAT_MOD_INVALID};
static const QList<uint32_t> cpuCopyFormats = {DRM_FORMAT_ARGB8888, DRM_FORMAT_XRGB8888};

static const bool bufferAgeEnabled = qEnvironmentVariable("KWIN_USE_BUFFER_AGE") != QStringLiteral("0");

static gbm_format_name_desc formatName(uint32_t format)
{
    gbm_format_name_desc ret;
    gbm_format_get_name(format, &ret);
    return ret;
}

EglGbmLayerSurface::EglGbmLayerSurface(DrmGpu *gpu, EglGbmBackend *eglBackend, BufferTarget target, FormatOption formatOption)
    : m_gpu(gpu)
    , m_eglBackend(eglBackend)
    , m_requestedBufferTarget(target)
    , m_formatOption(formatOption)
{
}

EglGbmLayerSurface::~EglGbmLayerSurface() = default;

EglGbmLayerSurface::Surface::~Surface()
{
    if (importContext) {
        importContext->makeCurrent();
        importGbmSwapchain.reset();
        importedTextureCache.clear();
        importContext.reset();
    }
    if (context) {
        context->makeCurrent();
    }
}

void EglGbmLayerSurface::destroyResources()
{
    m_surface = {};
    m_oldSurface = {};
}

std::optional<OutputLayerBeginFrameInfo> EglGbmLayerSurface::startRendering(const QSize &bufferSize, OutputTransform transformation, const QMap<uint32_t, QList<uint64_t>> &formats, const ColorDescription &colorDescription, const QVector3D &channelFactors, const std::shared_ptr<IccProfile> &iccProfile, bool enableColormanagement)
{
    if (!checkSurface(bufferSize, formats)) {
        return std::nullopt;
    }

    if (!m_eglBackend->openglContext()->makeCurrent()) {
        return std::nullopt;
    }

    auto slot = m_surface->gbmSwapchain->acquire();
    if (!slot) {
        return std::nullopt;
    }

    if (slot->framebuffer()->colorAttachment()->contentTransform() != transformation) {
        m_surface->damageJournal.clear();
    }
    slot->framebuffer()->colorAttachment()->setContentTransform(transformation);
    m_surface->currentSlot = slot;

    if (m_surface->targetColorDescription != colorDescription || m_surface->channelFactors != channelFactors
        || m_surface->colormanagementEnabled != enableColormanagement || m_surface->iccProfile != iccProfile) {
        m_surface->damageJournal.clear();
        m_surface->colormanagementEnabled = enableColormanagement;
        m_surface->targetColorDescription = colorDescription;
        m_surface->channelFactors = channelFactors;
        m_surface->iccProfile = iccProfile;
        if (iccProfile) {
            if (!m_surface->iccShader) {
                m_surface->iccShader = std::make_unique<IccShader>();
            }
        } else {
            m_surface->iccShader.reset();
        }
        if (enableColormanagement) {
            m_surface->intermediaryColorDescription = ColorDescription(colorDescription.colorimetry(), NamedTransferFunction::linear,
                                                                       colorDescription.sdrBrightness(), colorDescription.minHdrBrightness(),
                                                                       colorDescription.maxFrameAverageBrightness(), colorDescription.maxHdrHighlightBrightness(),
                                                                       colorDescription.sdrColorimetry());
        } else {
            m_surface->intermediaryColorDescription = colorDescription;
        }
    }

    const QRegion repaint = bufferAgeEnabled ? m_surface->damageJournal.accumulate(slot->age(), infiniteRegion()) : infiniteRegion();
    if (enableColormanagement) {
        if (!m_surface->shadowBuffer || m_surface->shadowTexture->size() != m_surface->gbmSwapchain->size()) {
            m_surface->shadowTexture = GLTexture::allocate(GL_RGBA16F, m_surface->gbmSwapchain->size());
            if (!m_surface->shadowTexture) {
                return std::nullopt;
            }
            m_surface->shadowBuffer = std::make_unique<GLFramebuffer>(m_surface->shadowTexture.get());
        }
        m_surface->shadowTexture->setContentTransform(m_surface->currentSlot->framebuffer()->colorAttachment()->contentTransform());
        m_surface->renderStart = std::chrono::steady_clock::now();
        m_surface->timeQuery->begin();
        return OutputLayerBeginFrameInfo{
            .renderTarget = RenderTarget(m_surface->shadowBuffer.get(), m_surface->intermediaryColorDescription),
            .repaint = repaint,
        };
    } else {
        m_surface->shadowTexture.reset();
        m_surface->shadowBuffer.reset();
        m_surface->renderStart = std::chrono::steady_clock::now();
        m_surface->timeQuery->begin();
        return OutputLayerBeginFrameInfo{
            .renderTarget = RenderTarget(m_surface->currentSlot->framebuffer()),
            .repaint = repaint,
        };
    }
}

bool EglGbmLayerSurface::endRendering(const QRegion &damagedRegion)
{
    if (m_surface->colormanagementEnabled) {
        GLFramebuffer *fbo = m_surface->currentSlot->framebuffer();
        GLFramebuffer::pushFramebuffer(fbo);
        ShaderBinder binder = m_surface->iccShader ? ShaderBinder(m_surface->iccShader->shader()) : ShaderBinder(ShaderTrait::MapTexture | ShaderTrait::TransformColorspace);
        if (m_surface->iccShader) {
            m_surface->iccShader->setUniforms(m_surface->iccProfile, m_surface->intermediaryColorDescription.sdrBrightness(), m_surface->channelFactors);
        } else {
            QMatrix4x4 ctm;
            ctm(0, 0) = m_surface->channelFactors.x();
            ctm(1, 1) = m_surface->channelFactors.y();
            ctm(2, 2) = m_surface->channelFactors.z();
            binder.shader()->setUniform(GLShader::Mat4Uniform::ColorimetryTransformation, ctm);
            binder.shader()->setUniform(GLShader::IntUniform::SourceNamedTransferFunction, int(m_surface->intermediaryColorDescription.transferFunction()));
            binder.shader()->setUniform(GLShader::IntUniform::DestinationNamedTransferFunction, int(m_surface->targetColorDescription.transferFunction()));
            binder.shader()->setUniform(GLShader::FloatUniform::SdrBrightness, m_surface->intermediaryColorDescription.sdrBrightness());
            binder.shader()->setUniform(GLShader::FloatUniform::MaxHdrBrightness, m_surface->intermediaryColorDescription.maxHdrHighlightBrightness());
        }
        QMatrix4x4 mat;
        mat.scale(1, -1);
        mat *= fbo->colorAttachment()->contentTransform().toMatrix();
        mat.scale(1, -1);
        mat.ortho(QRectF(QPointF(), fbo->size()));
        binder.shader()->setUniform(GLShader::Mat4Uniform::ModelViewProjectionMatrix, mat);
        glDisable(GL_BLEND);
        m_surface->shadowTexture->render(m_surface->gbmSwapchain->size());
        GLFramebuffer::popFramebuffer();
    }
    m_surface->damageJournal.add(damagedRegion);
    m_surface->timeQuery->end();
    glFlush();
    EGLNativeFence sourceFence(m_eglBackend->eglDisplayObject());
    if (!sourceFence.isValid()) {
        // llvmpipe doesn't do synchronization properly: https://gitlab.freedesktop.org/mesa/mesa/-/issues/9375
        // and NVidia doesn't support implicit sync
        glFinish();
    }
    m_surface->gbmSwapchain->release(m_surface->currentSlot, sourceFence.fileDescriptor().duplicate());
    const auto buffer = importBuffer(m_surface.get(), m_surface->currentSlot.get(), sourceFence.fileDescriptor());
    m_surface->renderEnd = std::chrono::steady_clock::now();
    if (buffer) {
        m_surface->currentFramebuffer = buffer;
        return true;
    } else {
        return false;
    }
}

std::chrono::nanoseconds EglGbmLayerSurface::queryRenderTime() const
{
    if (!m_surface) {
        return std::chrono::nanoseconds::zero();
    }
    const auto cpuTime = m_surface->renderEnd - m_surface->renderStart;
    if (m_surface->timeQuery) {
        m_eglBackend->makeCurrent();
        auto gpuTime = m_surface->timeQuery->result();
        if (m_surface->importTimeQuery && m_eglBackend->contextForGpu(m_gpu)->makeCurrent()) {
            gpuTime += m_surface->importTimeQuery->result();
        }
        return std::max(gpuTime, cpuTime);
    } else {
        return cpuTime;
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

const ColorDescription &EglGbmLayerSurface::colorDescription() const
{
    if (m_surface) {
        return m_surface->shadowTexture ? m_surface->intermediaryColorDescription : m_surface->targetColorDescription;
    } else {
        return ColorDescription::sRGB;
    }
}

bool EglGbmLayerSurface::doesSurfaceFit(const QSize &size, const QMap<uint32_t, QList<uint64_t>> &formats) const
{
    return doesSurfaceFit(m_surface.get(), size, formats);
}

std::shared_ptr<GLTexture> EglGbmLayerSurface::texture() const
{
    if (m_surface) {
        return m_surface->shadowTexture ? m_surface->shadowTexture : m_surface->currentSlot->texture();
    } else {
        return nullptr;
    }
}

std::shared_ptr<DrmFramebuffer> EglGbmLayerSurface::renderTestBuffer(const QSize &bufferSize, const QMap<uint32_t, QList<uint64_t>> &formats)
{
    if (checkSurface(bufferSize, formats)) {
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

bool EglGbmLayerSurface::checkSurface(const QSize &size, const QMap<uint32_t, QList<uint64_t>> &formats)
{
    if (doesSurfaceFit(m_surface.get(), size, formats)) {
        return true;
    }
    if (doesSurfaceFit(m_oldSurface.get(), size, formats)) {
        m_surface = std::move(m_oldSurface);
        return true;
    }
    if (auto newSurface = createSurface(size, formats)) {
        m_oldSurface = std::move(m_surface);
        if (m_oldSurface) {
            m_oldSurface->damageJournal.clear(); // TODO: Use absolute frame sequence numbers for indexing the DamageJournal
        }
        m_surface = std::move(newSurface);
        return true;
    }
    return false;
}

bool EglGbmLayerSurface::doesSurfaceFit(Surface *surface, const QSize &size, const QMap<uint32_t, QList<uint64_t>> &formats) const
{
    if (!surface || !surface->gbmSwapchain || surface->gbmSwapchain->size() != size) {
        return false;
    }
    if (surface->bufferTarget == BufferTarget::Dumb) {
        return formats.contains(surface->importDumbSwapchain->format());
    }
    switch (surface->importMode) {
    case MultiGpuImportMode::None:
    case MultiGpuImportMode::Dmabuf:
    case MultiGpuImportMode::LinearDmabuf: {
        const auto format = surface->gbmSwapchain->format();
        return formats.contains(format) && (surface->gbmSwapchain->modifier() == DRM_FORMAT_MOD_INVALID || formats[format].contains(surface->gbmSwapchain->modifier()));
    }
    case MultiGpuImportMode::DumbBuffer:
        return formats.contains(surface->importDumbSwapchain->format());
    case MultiGpuImportMode::Egl: {
        const auto format = surface->importGbmSwapchain->format();
        return formats.contains(format) && (surface->importGbmSwapchain->modifier() == DRM_FORMAT_MOD_INVALID || formats[format].contains(surface->importGbmSwapchain->modifier()));
    }
    }
    Q_UNREACHABLE();
}

std::unique_ptr<EglGbmLayerSurface::Surface> EglGbmLayerSurface::createSurface(const QSize &size, const QMap<uint32_t, QList<uint64_t>> &formats) const
{
    QList<FormatInfo> preferredFormats;
    QList<FormatInfo> fallbackFormats;
    for (auto it = formats.begin(); it != formats.end(); it++) {
        const auto format = FormatInfo::get(it.key());
        if (format.has_value() && format->bitsPerColor >= 8) {
            if (format->bitsPerPixel <= 32) {
                preferredFormats.push_back(format.value());
            } else {
                fallbackFormats.push_back(format.value());
            }
        }
    }

    // special case: the cursor plane needs linear, but not all GPUs (NVidia) can render to linear
    auto bufferTarget = m_requestedBufferTarget;
    if (m_gpu == m_eglBackend->gpu()) {
        const auto checkSurfaceNeedsLinear = [&formats](const FormatInfo &fmt) {
            const auto &mods = formats[fmt.drmFormat];
            return std::ranges::all_of(mods, [](const auto &mod) {
                return mod == DRM_FORMAT_MOD_LINEAR;
            });
        };
        const bool needsLinear = std::ranges::all_of(preferredFormats, checkSurfaceNeedsLinear) && std::ranges::all_of(fallbackFormats, checkSurfaceNeedsLinear);
        if (needsLinear) {
            const auto renderFormats = m_eglBackend->eglDisplayObject()->allSupportedDrmFormats();
            const auto checkFormatSupportsLinearRender = [&renderFormats](const auto &formatInfo) {
                const auto it = renderFormats.constFind(formatInfo.drmFormat);
                return it != renderFormats.cend() && it->nonExternalOnlyModifiers.contains(DRM_FORMAT_MOD_LINEAR);
            };
            const bool noLinearSupport = std::ranges::none_of(preferredFormats, checkFormatSupportsLinearRender) && std::ranges::none_of(fallbackFormats, checkFormatSupportsLinearRender);
            if (noLinearSupport) {
                bufferTarget = BufferTarget::Dumb;
            }
        }
    }

    const auto sort = [](const auto &lhs, const auto &rhs) {
        if (lhs.drmFormat == rhs.drmFormat) {
            // prefer having an alpha channel
            return lhs.alphaBits > rhs.alphaBits;
        } else if ((lhs.bitsPerColor == 10) != (rhs.bitsPerColor == 10)) {
            // prefer 10bpc / 30bpp formats
            return lhs.bitsPerColor == 10;
        } else {
            // fallback: prefer formats with lower bandwidth requirements
            return lhs.bitsPerPixel < rhs.bitsPerPixel;
        }
    };
    const auto doTestFormats = [this, &size, &formats, bufferTarget](const QList<FormatInfo> &gbmFormats, MultiGpuImportMode importMode) -> std::unique_ptr<Surface> {
        for (const auto &format : gbmFormats) {
            if (m_formatOption == FormatOption::RequireAlpha && format.alphaBits == 0) {
                continue;
            }
            auto surface = createSurface(size, format.drmFormat, formats[format.drmFormat], importMode, bufferTarget);
            if (surface) {
                return surface;
            }
        }
        return nullptr;
    };
    const auto testFormats = [this, &sort, &doTestFormats](QList<FormatInfo> &formats) -> std::unique_ptr<Surface> {
        std::sort(formats.begin(), formats.end(), sort);
        if (m_gpu == m_eglBackend->gpu()) {
            return doTestFormats(formats, MultiGpuImportMode::None);
        }
        if (auto surface = doTestFormats(formats, MultiGpuImportMode::Egl)) {
            qCDebug(KWIN_DRM) << "chose egl import with format" << formatName(surface->gbmSwapchain->format()).name << "and modifier" << surface->gbmSwapchain->modifier();
            return surface;
        }
        if (auto surface = doTestFormats(formats, MultiGpuImportMode::Dmabuf)) {
            qCDebug(KWIN_DRM) << "chose dmabuf import with format" << formatName(surface->gbmSwapchain->format()).name << "and modifier" << surface->gbmSwapchain->modifier();
            return surface;
        }
        if (auto surface = doTestFormats(formats, MultiGpuImportMode::LinearDmabuf)) {
            qCDebug(KWIN_DRM) << "chose linear dmabuf import with format" << formatName(surface->gbmSwapchain->format()).name << "and modifier" << surface->gbmSwapchain->modifier();
            return surface;
        }
        if (auto surface = doTestFormats(formats, MultiGpuImportMode::DumbBuffer)) {
            qCDebug(KWIN_DRM) << "chose cpu import with format" << formatName(surface->gbmSwapchain->format()).name << "and modifier" << surface->gbmSwapchain->modifier();
            return surface;
        }
        return nullptr;
    };
    if (auto ret = testFormats(preferredFormats)) {
        return ret;
    } else if (auto ret = testFormats(fallbackFormats)) {
        return ret;
    } else {
        return nullptr;
    }
}

static QList<uint64_t> filterModifiers(const QList<uint64_t> &one, const QList<uint64_t> &two)
{
    QList<uint64_t> ret = one;
    ret.erase(std::remove_if(ret.begin(), ret.end(), [&two](uint64_t mod) {
                  return !two.contains(mod);
              }),
              ret.end());
    return ret;
}

std::unique_ptr<EglGbmLayerSurface::Surface> EglGbmLayerSurface::createSurface(const QSize &size, uint32_t format, const QList<uint64_t> &modifiers, MultiGpuImportMode importMode, BufferTarget bufferTarget) const
{
    const bool cpuCopy = importMode == MultiGpuImportMode::DumbBuffer || bufferTarget == BufferTarget::Dumb;
    QList<uint64_t> renderModifiers;
    auto ret = std::make_unique<Surface>();
    const auto drmFormat = m_eglBackend->eglDisplayObject()->allSupportedDrmFormats()[format];
    if (importMode == MultiGpuImportMode::Egl) {
        ret->importContext = m_eglBackend->contextForGpu(m_gpu);
        if (!ret->importContext || ret->importContext->isSoftwareRenderer()) {
            return nullptr;
        }
        const auto importDrmFormat = ret->importContext->displayObject()->allSupportedDrmFormats()[format];
        renderModifiers = filterModifiers(importDrmFormat.allModifiers,
                                          drmFormat.nonExternalOnlyModifiers);
        // transferring non-linear buffers with implicit modifiers between GPUs is likely to yield wrong results
        renderModifiers.removeAll(DRM_FORMAT_MOD_INVALID);
    } else if (cpuCopy) {
        if (!cpuCopyFormats.contains(format)) {
            return nullptr;
        }
        renderModifiers = drmFormat.nonExternalOnlyModifiers;
    } else {
        renderModifiers = filterModifiers(modifiers, drmFormat.nonExternalOnlyModifiers);
    }
    if (renderModifiers.empty()) {
        return nullptr;
    }
    ret->context = m_eglBackend->contextForGpu(m_eglBackend->gpu());
    ret->bufferTarget = bufferTarget;
    ret->importMode = importMode;
    ret->gbmSwapchain = createGbmSwapchain(m_eglBackend->gpu(), m_eglBackend->openglContext(), size, format, renderModifiers, importMode, bufferTarget);
    if (!ret->gbmSwapchain) {
        return nullptr;
    }
    if (cpuCopy) {
        ret->importDumbSwapchain = std::make_unique<QPainterSwapchain>(m_gpu->graphicsBufferAllocator(), size, format);
    } else if (importMode == MultiGpuImportMode::Egl) {
        ret->importGbmSwapchain = createGbmSwapchain(m_gpu, ret->importContext.get(), size, format, modifiers, MultiGpuImportMode::None, BufferTarget::Normal);
        if (!ret->importGbmSwapchain) {
            return nullptr;
        }
        ret->importTimeQuery = std::make_unique<GLRenderTimeQuery>();
    }
    ret->timeQuery = std::make_unique<GLRenderTimeQuery>();
    if (!doRenderTestBuffer(ret.get())) {
        return nullptr;
    }
    return ret;
}

std::shared_ptr<EglSwapchain> EglGbmLayerSurface::createGbmSwapchain(DrmGpu *gpu, EglContext *context, const QSize &size, uint32_t format, const QList<uint64_t> &modifiers, MultiGpuImportMode importMode, BufferTarget bufferTarget) const
{
    static bool modifiersEnvSet = false;
    static const bool modifiersEnv = qEnvironmentVariableIntValue("KWIN_DRM_USE_MODIFIERS", &modifiersEnvSet) != 0;
    bool allowModifiers = (m_gpu->addFB2ModifiersSupported() || importMode == MultiGpuImportMode::Egl || importMode == MultiGpuImportMode::DumbBuffer) && (!modifiersEnvSet || (modifiersEnvSet && modifiersEnv)) && modifiers != implicitModifier;
#if !HAVE_GBM_BO_GET_FD_FOR_PLANE
    allowModifiers &= m_gpu == gpu;
#endif
    const bool linearSupported = modifiers.contains(DRM_FORMAT_MOD_LINEAR);
    const bool preferLinear = importMode == MultiGpuImportMode::DumbBuffer || bufferTarget == BufferTarget::Linear;
    const bool forceLinear = importMode == MultiGpuImportMode::LinearDmabuf || (importMode != MultiGpuImportMode::None && importMode != MultiGpuImportMode::DumbBuffer && !allowModifiers);
    if (forceLinear && !linearSupported) {
        return nullptr;
    }
    if (linearSupported && (preferLinear || forceLinear)) {
        if (const auto swapchain = EglSwapchain::create(gpu->graphicsBufferAllocator(), context, size, format, linearModifier)) {
            return swapchain;
        } else if (forceLinear) {
            return nullptr;
        }
    }

    if (allowModifiers) {
        if (auto swapchain = EglSwapchain::create(gpu->graphicsBufferAllocator(), context, size, format, modifiers)) {
            return swapchain;
        }
    }

    return EglSwapchain::create(gpu->graphicsBufferAllocator(), context, size, format, implicitModifier);
}

std::shared_ptr<DrmFramebuffer> EglGbmLayerSurface::doRenderTestBuffer(Surface *surface) const
{
    auto slot = surface->gbmSwapchain->acquire();
    if (!slot) {
        return nullptr;
    }
    if (const auto ret = importBuffer(surface, slot.get(), FileDescriptor{})) {
        surface->currentSlot = slot;
        surface->currentFramebuffer = ret;
        return ret;
    } else {
        return nullptr;
    }
}

std::shared_ptr<DrmFramebuffer> EglGbmLayerSurface::importBuffer(Surface *surface, EglSwapchainSlot *slot, const FileDescriptor &readFence) const
{
    if (surface->bufferTarget == BufferTarget::Dumb || surface->importMode == MultiGpuImportMode::DumbBuffer) {
        return importWithCpu(surface, slot);
    } else if (surface->importMode == MultiGpuImportMode::Egl) {
        return importWithEgl(surface, slot->buffer(), readFence);
    } else {
        const auto ret = m_gpu->importBuffer(slot->buffer(), readFence.duplicate());
        if (!ret) {
            qCWarning(KWIN_DRM, "Failed to create framebuffer: %s", strerror(errno));
        }
        return ret;
    }
}

std::shared_ptr<DrmFramebuffer> EglGbmLayerSurface::importWithEgl(Surface *surface, GraphicsBuffer *sourceBuffer, const FileDescriptor &readFence) const
{
    Q_ASSERT(surface->importGbmSwapchain);

    const auto display = m_eglBackend->displayForGpu(m_gpu);
    // older versions of the NVidia proprietary driver support neither implicit sync nor EGL_ANDROID_native_fence_sync
    if (!readFence.isValid() || !display->supportsNativeFence()) {
        glFinish();
    }

    if (!surface->importContext->makeCurrent()) {
        return nullptr;
    }
    surface->importTimeQuery->begin();

    if (readFence.isValid()) {
        const auto destinationFence = EGLNativeFence::importFence(surface->importContext->displayObject(), readFence.duplicate());
        destinationFence.waitSync();
    }

    auto &sourceTexture = surface->importedTextureCache[sourceBuffer];
    if (!sourceTexture) {
        sourceTexture = surface->importContext->importDmaBufAsTexture(*sourceBuffer->dmabufAttributes());
    }
    if (!sourceTexture) {
        qCWarning(KWIN_DRM, "failed to import the source texture!");
        return nullptr;
    }
    auto slot = surface->importGbmSwapchain->acquire();
    if (!slot) {
        qCWarning(KWIN_DRM, "failed to import the local texture!");
        return nullptr;
    }

    GLFramebuffer *fbo = slot->framebuffer();
    glBindFramebuffer(GL_FRAMEBUFFER, fbo->handle());
    glViewport(0, 0, fbo->size().width(), fbo->size().height());

    const auto shader = surface->importContext->shaderManager()->pushShader(sourceTexture->target() == GL_TEXTURE_EXTERNAL_OES ? ShaderTrait::MapExternalTexture : ShaderTrait::MapTexture);
    QMatrix4x4 mat;
    mat.scale(1, -1);
    mat.ortho(QRect(QPoint(), fbo->size()));
    shader->setUniform(GLShader::Mat4Uniform::ModelViewProjectionMatrix, mat);

    sourceTexture->bind();
    sourceTexture->render(fbo->size());
    sourceTexture->unbind();

    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    surface->importContext->shaderManager()->popShader();
    glFlush();
    EGLNativeFence endFence(display);
    if (!endFence.isValid()) {
        glFinish();
    }
    surface->importGbmSwapchain->release(slot, endFence.fileDescriptor().duplicate());
    surface->importTimeQuery->end();

    // restore the old context
    m_eglBackend->makeCurrent();
    return m_gpu->importBuffer(slot->buffer(), endFence.fileDescriptor().duplicate());
}

std::shared_ptr<DrmFramebuffer> EglGbmLayerSurface::importWithCpu(Surface *surface, EglSwapchainSlot *source) const
{
    Q_ASSERT(surface->importDumbSwapchain);
    const auto slot = surface->importDumbSwapchain->acquire();
    if (!slot) {
        qCWarning(KWIN_DRM) << "EglGbmLayerSurface::importWithCpu: failed to get a target dumb buffer";
        return nullptr;
    }
    const auto size = source->buffer()->size();
    const qsizetype srcStride = 4 * size.width();
    GLFramebuffer::pushFramebuffer(source->framebuffer());
    QImage *const dst = slot->view()->image();
    if (dst->bytesPerLine() == srcStride) {
        glReadPixels(0, 0, dst->width(), dst->height(), GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV, dst->bits());
    } else {
        // there's padding, need to copy line by line
        if (surface->cpuCopyCache.size() != dst->size()) {
            surface->cpuCopyCache = QImage(dst->size(), QImage::Format_RGBA8888);
        }
        glReadPixels(0, 0, dst->width(), dst->height(), GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV, surface->cpuCopyCache.bits());
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
    return ret;
}
}

#include "moc_drm_egl_layer_surface.cpp"
