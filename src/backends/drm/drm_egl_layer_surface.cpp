/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2022 Xaver Hugl <xaver.hugl@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "drm_egl_layer_surface.h"

#include "config-kwin.h"
#include "core/graphicsbufferview.h"
#include "drm_egl_backend.h"
#include "drm_gpu.h"
#include "drm_logging.h"
#include "platformsupport/scenes/opengl/eglnativefence.h"
#include "platformsupport/scenes/opengl/eglswapchain.h"
#include "platformsupport/scenes/qpainter/qpainterswapchain.h"
#include "utils/drm_format_helper.h"

#include <drm_fourcc.h>
#include <errno.h>
#include <gbm.h>
#include <unistd.h>

namespace KWin
{

static const QVector<uint64_t> linearModifier = {DRM_FORMAT_MOD_LINEAR};
static const QVector<uint64_t> implicitModifier = {DRM_FORMAT_MOD_INVALID};
static const QVector<uint32_t> cpuCopyFormats = {DRM_FORMAT_ARGB8888, DRM_FORMAT_XRGB8888};

static gbm_format_name_desc formatName(uint32_t format)
{
    gbm_format_name_desc ret;
    gbm_format_get_name(format, &ret);
    return ret;
}

EglGbmLayerSurface::EglGbmLayerSurface(DrmGpu *gpu, EglGbmBackend *eglBackend, BufferTarget target, FormatOption formatOption)
    : m_gpu(gpu)
    , m_eglBackend(eglBackend)
    , m_bufferTarget(target)
    , m_formatOption(formatOption)
{
}

EglGbmLayerSurface::~EglGbmLayerSurface()
{
    destroyResources();
}

void EglGbmLayerSurface::destroyResources()
{
    m_surface = {};
    m_oldSurface = {};
}

std::optional<OutputLayerBeginFrameInfo> EglGbmLayerSurface::startRendering(const QSize &bufferSize, TextureTransforms transformation, const QMap<uint32_t, QVector<uint64_t>> &formats, const ColorDescription &colorDescription, const QVector3D &channelFactors, bool enableColormanagement)
{
    if (!checkSurface(bufferSize, formats)) {
        return std::nullopt;
    }

    if (!m_eglBackend->contextObject()->makeCurrent()) {
        return std::nullopt;
    }

    auto slot = m_surface.gbmSwapchain->acquire();
    if (!slot) {
        return std::nullopt;
    }

    slot->framebuffer()->colorAttachment()->setContentTransform(transformation);
    m_surface.currentSlot = slot;

    if (m_surface.targetColorDescription != colorDescription || m_surface.channelFactors != channelFactors || m_surface.colormanagementEnabled != enableColormanagement) {
        m_surface.damageJournal.clear();
        m_surface.colormanagementEnabled = enableColormanagement;
        m_surface.targetColorDescription = colorDescription;
        m_surface.channelFactors = channelFactors;
        if (enableColormanagement) {
            m_surface.intermediaryColorDescription = ColorDescription(colorDescription.colorimetry(), NamedTransferFunction::linear,
                                                                      colorDescription.sdrBrightness(), colorDescription.minHdrBrightness(),
                                                                      colorDescription.maxHdrBrightness(), colorDescription.maxHdrHighlightBrightness());
        } else {
            m_surface.intermediaryColorDescription = colorDescription;
        }
    }

    QRegion repaint = m_surface.damageJournal.accumulate(slot->age(), infiniteRegion());
    if (enableColormanagement) {
        if (!m_surface.shadowBuffer || m_surface.shadowTexture->size() != m_surface.gbmSwapchain->size()) {
            m_surface.shadowTexture = GLTexture::allocate(GL_RGBA16F, m_surface.gbmSwapchain->size());
            if (!m_surface.shadowTexture) {
                return std::nullopt;
            }
            m_surface.shadowBuffer = GLFramebuffer::create(m_surface.shadowTexture.get());
        }
        return OutputLayerBeginFrameInfo{
            .renderTarget = RenderTarget(m_surface.shadowBuffer.get(), m_surface.intermediaryColorDescription),
            .repaint = repaint,
        };
    } else {
        m_surface.shadowTexture.reset();
        m_surface.shadowBuffer.reset();
        return OutputLayerBeginFrameInfo{
            .renderTarget = RenderTarget(m_surface.currentSlot->framebuffer()),
            .repaint = repaint,
        };
    }
}

bool EglGbmLayerSurface::endRendering(const QRegion &damagedRegion)
{
    if (m_surface.colormanagementEnabled) {
        GLFramebuffer *fbo = m_surface.currentSlot->framebuffer();
        GLTexture *texture = fbo->colorAttachment();
        GLFramebuffer::pushFramebuffer(fbo);
        ShaderBinder binder(ShaderTrait::MapTexture | ShaderTrait::TransformColorspace);
        QMatrix4x4 mat = texture->contentTransformMatrix();
        mat.ortho(QRectF(QPointF(), fbo->size()));
        binder.shader()->setUniform(GLShader::MatrixUniform::ModelViewProjectionMatrix, mat);
        QMatrix3x3 ctm;
        ctm(0, 0) = m_surface.channelFactors.x();
        ctm(1, 1) = m_surface.channelFactors.y();
        ctm(2, 2) = m_surface.channelFactors.z();
        binder.shader()->setUniform(GLShader::MatrixUniform::ColorimetryTransformation, ctm);
        binder.shader()->setUniform(GLShader::IntUniform::SourceNamedTransferFunction, int(m_surface.intermediaryColorDescription.transferFunction()));
        binder.shader()->setUniform(GLShader::IntUniform::DestinationNamedTransferFunction, int(m_surface.targetColorDescription.transferFunction()));
        m_surface.shadowTexture->render(m_surface.gbmSwapchain->size(), 1);
        GLFramebuffer::popFramebuffer();
    }
    m_surface.damageJournal.add(damagedRegion);
    m_surface.gbmSwapchain->release(m_surface.currentSlot);
    glFlush();
    const auto buffer = importBuffer(m_surface, m_surface.currentSlot.get());
    if (buffer) {
        m_surface.currentFramebuffer = buffer;
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
    return m_surface.currentFramebuffer;
}

const ColorDescription &EglGbmLayerSurface::colorDescription() const
{
    return m_surface.shadowTexture ? m_surface.intermediaryColorDescription : m_surface.targetColorDescription;
}

bool EglGbmLayerSurface::doesSurfaceFit(const QSize &size, const QMap<uint32_t, QVector<uint64_t>> &formats) const
{
    return doesSurfaceFit(m_surface, size, formats);
}

std::shared_ptr<GLTexture> EglGbmLayerSurface::texture() const
{
    return m_surface.shadowTexture ? m_surface.shadowTexture : m_surface.currentSlot->texture();
}

std::shared_ptr<DrmFramebuffer> EglGbmLayerSurface::renderTestBuffer(const QSize &bufferSize, const QMap<uint32_t, QVector<uint64_t>> &formats)
{
    if (checkSurface(bufferSize, formats)) {
        return m_surface.currentFramebuffer;
    } else {
        return nullptr;
    }
}

bool EglGbmLayerSurface::checkSurface(const QSize &size, const QMap<uint32_t, QVector<uint64_t>> &formats)
{
    if (doesSurfaceFit(m_surface, size, formats)) {
        return true;
    }
    if (doesSurfaceFit(m_oldSurface, size, formats)) {
        m_surface = m_oldSurface;
        return true;
    }
    if (const auto newSurface = createSurface(size, formats)) {
        m_oldSurface = m_surface;
        m_surface = newSurface.value();
        return true;
    }
    return false;
}

bool EglGbmLayerSurface::doesSurfaceFit(const Surface &surface, const QSize &size, const QMap<uint32_t, QVector<uint64_t>> &formats) const
{
    const auto &swapchain = surface.gbmSwapchain;
    return swapchain
        && swapchain->size() == size
        && formats.contains(swapchain->format())
        && (surface.forceLinear || swapchain->modifier() == DRM_FORMAT_MOD_INVALID || formats[swapchain->format()].contains(swapchain->modifier()));
}

std::optional<EglGbmLayerSurface::Surface> EglGbmLayerSurface::createSurface(const QSize &size, const QMap<uint32_t, QVector<uint64_t>> &formats) const
{
    QVector<FormatInfo> preferredFormats;
    QVector<FormatInfo> fallbackFormats;
    for (auto it = formats.begin(); it != formats.end(); it++) {
        const auto format = formatInfo(it.key());
        if (format.has_value() && format->bitsPerColor >= 8) {
            if (format->bitsPerPixel <= 32) {
                preferredFormats.push_back(format.value());
            } else {
                fallbackFormats.push_back(format.value());
            }
        }
    }
    const auto sort = [this](const auto &lhs, const auto &rhs) {
        if (lhs.drmFormat == rhs.drmFormat) {
            // prefer having an alpha channel
            return lhs.alphaBits > rhs.alphaBits;
        } else if (m_eglBackend->prefer10bpc() && ((lhs.bitsPerColor == 10) != (rhs.bitsPerColor == 10))) {
            // prefer 10bpc / 30bpp formats
            return lhs.bitsPerColor == 10;
        } else {
            // fallback: prefer formats with lower bandwidth requirements
            return lhs.bitsPerPixel < rhs.bitsPerPixel;
        }
    };
    const auto doTestFormats = [this, &size, &formats](const QVector<FormatInfo> &gbmFormats, MultiGpuImportMode importMode) -> std::optional<Surface> {
        for (const auto &format : gbmFormats) {
            if (m_formatOption == FormatOption::RequireAlpha && format.alphaBits == 0) {
                continue;
            }
            const auto surface = createSurface(size, format.drmFormat, formats[format.drmFormat], importMode);
            if (surface.has_value()) {
                return surface;
            }
        }
        return std::nullopt;
    };
    const auto testFormats = [this, &sort, &doTestFormats](QVector<FormatInfo> &formats) -> std::optional<Surface> {
        std::sort(formats.begin(), formats.end(), sort);
        if (m_gpu == m_eglBackend->gpu()) {
            return doTestFormats(formats, MultiGpuImportMode::None);
        }
        if (const auto surface = doTestFormats(formats, MultiGpuImportMode::Egl)) {
            qCDebug(KWIN_DRM) << "chose egl import with format" << formatName(surface->gbmSwapchain->format()).name << "and modifier" << surface->gbmSwapchain->modifier();
            return surface;
        }
        if (const auto surface = doTestFormats(formats, MultiGpuImportMode::Dmabuf)) {
            qCDebug(KWIN_DRM) << "chose dmabuf import with format" << formatName(surface->gbmSwapchain->format()).name << "and modifier" << surface->gbmSwapchain->modifier();
            return surface;
        }
        if (const auto surface = doTestFormats(formats, MultiGpuImportMode::LinearDmabuf)) {
            qCDebug(KWIN_DRM) << "chose linear dmabuf import with format" << formatName(surface->gbmSwapchain->format()).name << "and modifier" << surface->gbmSwapchain->modifier();
            return surface;
        }
        if (const auto surface = doTestFormats(formats, MultiGpuImportMode::DumbBuffer)) {
            qCDebug(KWIN_DRM) << "chose cpu import with format" << formatName(surface->gbmSwapchain->format()).name << "and modifier" << surface->gbmSwapchain->modifier();
            return surface;
        }
        return std::nullopt;
    };
    if (const auto ret = testFormats(preferredFormats)) {
        return ret;
    } else if (const auto ret = testFormats(fallbackFormats)) {
        return ret;
    } else {
        return std::nullopt;
    }
}

static QVector<uint64_t> filterModifiers(const QVector<uint64_t> &one, const QVector<uint64_t> &two)
{
    QVector<uint64_t> ret = one;
    ret.erase(std::remove_if(ret.begin(), ret.end(), [&two](uint64_t mod) {
                  return !two.contains(mod);
              }),
              ret.end());
    return ret;
}

std::optional<EglGbmLayerSurface::Surface> EglGbmLayerSurface::createSurface(const QSize &size, uint32_t format, const QVector<uint64_t> &modifiers, MultiGpuImportMode importMode) const
{
    const bool cpuCopy = importMode == MultiGpuImportMode::DumbBuffer || m_bufferTarget == BufferTarget::Dumb;
    QVector<uint64_t> renderModifiers;
    if (importMode == MultiGpuImportMode::Egl) {
        const auto context = m_eglBackend->contextForGpu(m_gpu);
        if (!context || context->isSoftwareRenderer()) {
            return std::nullopt;
        }
        renderModifiers = filterModifiers(context->displayObject()->allSupportedDrmFormats()[format],
                                          m_eglBackend->eglDisplayObject()->supportedDrmFormats().value(format));
    } else if (cpuCopy) {
        if (!cpuCopyFormats.contains(format)) {
            return std::nullopt;
        }
        renderModifiers = m_eglBackend->eglDisplayObject()->supportedDrmFormats().value(format);
    } else {
        renderModifiers = filterModifiers(modifiers, m_eglBackend->eglDisplayObject()->supportedDrmFormats().value(format));
    }
    if (renderModifiers.empty()) {
        return std::nullopt;
    }
    Surface ret;
    ret.importMode = importMode;
    ret.forceLinear = importMode == MultiGpuImportMode::DumbBuffer || importMode == MultiGpuImportMode::LinearDmabuf || m_bufferTarget != BufferTarget::Normal;
    ret.gbmSwapchain = createGbmSwapchain(m_eglBackend->gpu(), m_eglBackend->contextObject(), size, format, renderModifiers, ret.forceLinear);
    if (!ret.gbmSwapchain) {
        return std::nullopt;
    }
    if (cpuCopy) {
        ret.importDumbSwapchain = std::make_shared<QPainterSwapchain>(m_gpu->graphicsBufferAllocator(), size, format);
    } else if (importMode == MultiGpuImportMode::Egl) {
        ret.importGbmSwapchain = createGbmSwapchain(m_gpu, m_eglBackend->contextForGpu(m_gpu), size, format, modifiers, false);
        if (!ret.importGbmSwapchain) {
            return std::nullopt;
        }
    }
    if (!doRenderTestBuffer(ret)) {
        return std::nullopt;
    }
    return ret;
}

std::shared_ptr<EglSwapchain> EglGbmLayerSurface::createGbmSwapchain(DrmGpu *gpu, EglContext *context, const QSize &size, uint32_t format, const QVector<uint64_t> &modifiers, bool preferLinear) const
{
    static bool modifiersEnvSet = false;
    static const bool modifiersEnv = qEnvironmentVariableIntValue("KWIN_DRM_USE_MODIFIERS", &modifiersEnvSet) != 0;
    bool allowModifiers = gpu->addFB2ModifiersSupported() && (!modifiersEnvSet || (modifiersEnvSet && modifiersEnv)) && modifiers != implicitModifier;
#if !HAVE_GBM_BO_GET_FD_FOR_PLANE
    allowModifiers &= m_gpu == gpu;
#endif
    const bool linearSupported = modifiers.contains(DRM_FORMAT_MOD_LINEAR);
    const bool forceLinear = m_gpu != gpu && !allowModifiers;
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

std::shared_ptr<DrmFramebuffer> EglGbmLayerSurface::doRenderTestBuffer(Surface &surface) const
{
    auto slot = surface.gbmSwapchain->acquire();
    if (!slot) {
        return nullptr;
    }
    if (const auto ret = importBuffer(surface, slot.get())) {
        surface.currentSlot = slot;
        surface.currentFramebuffer = ret;
        return ret;
    } else {
        return nullptr;
    }
}

std::shared_ptr<DrmFramebuffer> EglGbmLayerSurface::importBuffer(Surface &surface, EglSwapchainSlot *slot) const
{
    if (m_bufferTarget == BufferTarget::Dumb || surface.importMode == MultiGpuImportMode::DumbBuffer) {
        return importWithCpu(surface, slot);
    } else if (surface.importMode == MultiGpuImportMode::Egl) {
        return importWithEgl(surface, slot->buffer());
    } else {
        const auto ret = m_gpu->importBuffer(slot->buffer());
        if (!ret) {
            qCWarning(KWIN_DRM, "Failed to create framebuffer: %s", strerror(errno));
        }
        return ret;
    }
}

std::shared_ptr<DrmFramebuffer> EglGbmLayerSurface::importWithEgl(Surface &surface, GraphicsBuffer *sourceBuffer) const
{
    Q_ASSERT(surface.importGbmSwapchain);

    EGLNativeFence sourceFence(m_eglBackend->eglDisplayObject());

    const auto display = m_eglBackend->displayForGpu(m_gpu);
    // the NVidia proprietary driver supports neither implicit sync nor EGL_ANDROID_native_fence_sync
    if (!sourceFence.isValid() || !display->supportsNativeFence()) {
        glFinish();
    }
    const auto context = m_eglBackend->contextForGpu(m_gpu);
    if (!context || context->isSoftwareRenderer() || !context->makeCurrent()) {
        return nullptr;
    }

    if (sourceFence.isValid()) {
        const auto destinationFence = EGLNativeFence::importFence(context->displayObject(), sourceFence.fileDescriptor().duplicate());
        destinationFence.waitSync();
    }

    auto &sourceTexture = surface.importedTextureCache[sourceBuffer];
    if (!sourceTexture) {
        sourceTexture = context->importDmaBufAsTexture(*sourceBuffer->dmabufAttributes());
    }
    if (!sourceTexture) {
        qCWarning(KWIN_DRM, "failed to import the source texture!");
        return nullptr;
    }
    auto slot = surface.importGbmSwapchain->acquire();
    if (!slot) {
        qCWarning(KWIN_DRM, "failed to import the local texture!");
        return nullptr;
    }

    GLFramebuffer *fbo = slot->framebuffer();
    glBindFramebuffer(GL_FRAMEBUFFER, fbo->handle());
    glViewport(0, 0, fbo->size().width(), fbo->size().height());

    const auto shader = context->shaderManager()->pushShader(sourceTexture->target() == GL_TEXTURE_EXTERNAL_OES ? ShaderTrait::MapExternalTexture : ShaderTrait::MapTexture);
    QMatrix4x4 mat;
    mat.scale(1, -1);
    mat.ortho(QRect(QPoint(), fbo->size()));
    shader->setUniform(GLShader::ModelViewProjectionMatrix, mat);

    sourceTexture->bind();
    sourceTexture->render(fbo->size(), 1);
    sourceTexture->unbind();

    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    context->shaderManager()->popShader();
    glFlush();
    surface.importGbmSwapchain->release(slot);

    // restore the old context
    context->doneCurrent();
    m_eglBackend->makeCurrent();
    return m_gpu->importBuffer(slot->buffer());
}

std::shared_ptr<DrmFramebuffer> EglGbmLayerSurface::importWithCpu(Surface &surface, EglSwapchainSlot *source) const
{
    Q_ASSERT(surface.importDumbSwapchain);
    const auto slot = surface.importDumbSwapchain->acquire();
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
        if (surface.cpuCopyCache.size() != dst->size()) {
            surface.cpuCopyCache = QImage(dst->size(), QImage::Format_RGBA8888);
        }
        glReadPixels(0, 0, dst->width(), dst->height(), GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV, surface.cpuCopyCache.bits());
        for (int i = 0; i < dst->height(); i++) {
            std::memcpy(dst->scanLine(i), surface.cpuCopyCache.scanLine(i), srcStride);
        }
    }
    GLFramebuffer::popFramebuffer();

    const auto ret = m_gpu->importBuffer(slot->buffer());
    if (!ret) {
        qCWarning(KWIN_DRM, "Failed to create a framebuffer: %s", strerror(errno));
    }
    surface.importDumbSwapchain->release(slot);
    return ret;
}
}

#include "moc_drm_egl_layer_surface.cpp"
