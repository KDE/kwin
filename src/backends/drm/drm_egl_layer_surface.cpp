/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2022 Xaver Hugl <xaver.hugl@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "drm_egl_layer_surface.h"

#include "config-kwin.h"
#include "drm_buffer_gbm.h"
#include "drm_dumb_buffer.h"
#include "drm_dumb_swapchain.h"
#include "drm_egl_backend.h"
#include "drm_gbm_swapchain.h"
#include "drm_gpu.h"
#include "drm_logging.h"
#include "drm_output.h"
#include "gbm_dmabuf.h"
#include "kwineglutils_p.h"
#include "libkwineffects/kwinglplatform.h"
#include "scene/surfaceitem_wayland.h"
#include "wayland/surface_interface.h"

#include <drm_fourcc.h>
#include <errno.h>
#include <gbm.h>
#include <unistd.h>

namespace KWin
{

static const QVector<uint64_t> linearModifier = {DRM_FORMAT_MOD_LINEAR};

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

    auto [buffer, repaint] = m_surface.gbmSwapchain->acquire();
    if (!buffer) {
        return std::nullopt;
    }
    auto &[texture, fbo] = m_surface.textureCache[buffer->bo()];
    if (!texture) {
        texture = m_eglBackend->importBufferObjectAsTexture(buffer->bo());
        if (!texture) {
            return std::nullopt;
        }
    }
    texture->setContentTransform(transformation);
    if (!fbo) {
        fbo = std::make_shared<GLFramebuffer>(texture.get());
        if (!fbo->valid()) {
            fbo.reset();
            return std::nullopt;
        }
    }
    m_surface.currentBuffer = buffer;

    if (m_surface.targetColorDescription != colorDescription || m_surface.channelFactors != channelFactors) {
        m_surface.gbmSwapchain->resetDamage();
        repaint = infiniteRegion();
        m_surface.targetColorDescription = colorDescription;
        m_surface.channelFactors = channelFactors;
        if (colorDescription != ColorDescription::sRGB) {
            m_surface.intermediaryColorDescription = ColorDescription(colorDescription.colorimetry(), NamedTransferFunction::linear,
                                                                      colorDescription.sdrBrightness(), colorDescription.minHdrBrightness(),
                                                                      colorDescription.maxHdrBrightness(), colorDescription.maxHdrHighlightBrightness());
        } else {
            m_surface.intermediaryColorDescription = colorDescription;
        }
    }
    if (enableColormanagement) {
        if (!m_surface.shadowBuffer) {
            m_surface.shadowTexture = std::make_shared<GLTexture>(GL_RGBA16F, m_surface.gbmSwapchain->size());
            m_surface.shadowBuffer = std::make_shared<GLFramebuffer>(m_surface.shadowTexture.get());
        }
        return OutputLayerBeginFrameInfo{
            .renderTarget = RenderTarget(m_surface.shadowBuffer.get(), m_surface.intermediaryColorDescription),
            .repaint = repaint,
        };
    } else {
        m_surface.shadowTexture.reset();
        m_surface.shadowBuffer.reset();
        return OutputLayerBeginFrameInfo{
            .renderTarget = RenderTarget(fbo.get()),
            .repaint = repaint,
        };
    }
}

bool EglGbmLayerSurface::endRendering(const QRegion &damagedRegion)
{
    if (m_surface.shadowTexture) {
        const auto &[texture, fbo] = m_surface.textureCache[m_surface.currentBuffer->bo()];
        GLFramebuffer::pushFramebuffer(fbo.get());
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
    m_surface.gbmSwapchain->damage(damagedRegion);
    glFlush();
    const auto buffer = importBuffer(m_surface, m_surface.currentBuffer);
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
    return m_surface.shadowTexture ? m_surface.shadowTexture : m_surface.textureCache[m_surface.currentBuffer->bo()].first;
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
    QVector<GbmFormat> preferredFormats;
    QVector<GbmFormat> fallbackFormats;
    for (auto it = formats.begin(); it != formats.end(); it++) {
        const auto format = m_eglBackend->gbmFormatForDrmFormat(it.key());
        if (format.has_value() && format->bpp >= 24) {
            if (format->bpp <= 32) {
                preferredFormats.push_back(format.value());
            } else {
                fallbackFormats.push_back(format.value());
            }
        }
    }
    const auto sort = [this](const auto &lhs, const auto &rhs) {
        if (lhs.drmFormat == rhs.drmFormat) {
            // prefer having an alpha channel
            return lhs.alphaSize > rhs.alphaSize;
        } else if (m_eglBackend->prefer10bpc() && ((lhs.bpp == 30) != (rhs.bpp == 30))) {
            // prefer 10bpc / 30bpp formats
            return lhs.bpp == 30;
        } else {
            // fallback: prefer formats with lower bandwidth requirements
            return lhs.bpp < rhs.bpp;
        }
    };
    const auto doTestFormats = [this, &size, &formats](const QVector<GbmFormat> &gbmFormats, MultiGpuImportMode importMode) -> std::optional<Surface> {
        for (const auto &format : gbmFormats) {
            if (m_formatOption == FormatOption::RequireAlpha && format.alphaSize == 0) {
                continue;
            }
            const auto surface = createSurface(size, format.drmFormat, formats[format.drmFormat], importMode);
            if (surface.has_value()) {
                return surface;
            }
        }
        return std::nullopt;
    };
    const auto testFormats = [this, &sort, &doTestFormats](QVector<GbmFormat> &formats) -> std::optional<Surface> {
        std::sort(formats.begin(), formats.end(), sort);
        if (const auto surface = doTestFormats(formats, MultiGpuImportMode::Dmabuf)) {
            if (m_gpu != m_eglBackend->gpu()) {
                qCDebug(KWIN_DRM) << "chose dmabuf import with format" << formatName(surface->gbmSwapchain->format()).name << "and modifier" << surface->gbmSwapchain->modifier();
            }
            return surface;
        }
        if (m_gpu != m_eglBackend->gpu()) {
            if (const auto surface = doTestFormats(formats, MultiGpuImportMode::LinearDmabuf)) {
                qCDebug(KWIN_DRM) << "chose linear dmabuf import with format" << formatName(surface->gbmSwapchain->format()).name << "and modifier" << surface->gbmSwapchain->modifier();
                return surface;
            }
            if (const auto surface = doTestFormats(formats, MultiGpuImportMode::Egl)) {
                qCDebug(KWIN_DRM) << "chose egl import with format" << formatName(surface->gbmSwapchain->format()).name << "and modifier" << surface->gbmSwapchain->modifier();
                return surface;
            }
            if (const auto surface = doTestFormats(formats, MultiGpuImportMode::DumbBuffer)) {
                qCDebug(KWIN_DRM) << "chose cpu import with format" << formatName(surface->gbmSwapchain->format()).name << "and modifier" << surface->gbmSwapchain->modifier();
                return surface;
            }
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

std::optional<EglGbmLayerSurface::Surface> EglGbmLayerSurface::createSurface(const QSize &size, uint32_t format, const QVector<uint64_t> &modifiers, MultiGpuImportMode importMode) const
{
    QVector<uint64_t> renderModifiers = modifiers;
    if (importMode == MultiGpuImportMode::Egl) {
        const auto context = m_eglBackend->contextForGpu(m_gpu);
        if (!context || context->isSoftwareRenderer()) {
            return std::nullopt;
        }
        renderModifiers = context->displayObject()->supportedDrmFormats()[format];
    }
    Surface ret;
    ret.importMode = importMode;
    ret.forceLinear = importMode == MultiGpuImportMode::DumbBuffer || importMode == MultiGpuImportMode::LinearDmabuf || m_bufferTarget != BufferTarget::Normal;
    ret.gbmSwapchain = createGbmSwapchain(m_eglBackend->gpu(), size, format, renderModifiers, ret.forceLinear);
    if (!ret.gbmSwapchain) {
        return std::nullopt;
    }
    if (importMode == MultiGpuImportMode::DumbBuffer || m_bufferTarget == BufferTarget::Dumb) {
        ret.importDumbSwapchain = std::make_shared<DumbSwapchain>(m_gpu, size, format);
        if (ret.importDumbSwapchain->isEmpty()) {
            return std::nullopt;
        }
    } else if (importMode == MultiGpuImportMode::Egl) {
        ret.importGbmSwapchain = createGbmSwapchain(m_gpu, size, format, modifiers, false);
        if (!ret.importGbmSwapchain) {
            return std::nullopt;
        }
    }
    if (!doRenderTestBuffer(ret)) {
        return std::nullopt;
    }
    return ret;
}

std::shared_ptr<GbmSwapchain> EglGbmLayerSurface::createGbmSwapchain(DrmGpu *gpu, const QSize &size, uint32_t format, const QVector<uint64_t> &modifiers, bool forceLinear) const
{
    static bool modifiersEnvSet = false;
    static const bool modifiersEnv = qEnvironmentVariableIntValue("KWIN_DRM_USE_MODIFIERS", &modifiersEnvSet) != 0;
    bool allowModifiers = gpu->addFB2ModifiersSupported() && (!modifiersEnvSet || (modifiersEnvSet && modifiersEnv)) && !modifiers.isEmpty();
#if !HAVE_GBM_BO_GET_FD_FOR_PLANE
    allowModifiers &= m_gpu == gpu;
#endif

    if (allowModifiers) {
        const auto ret = GbmSwapchain::createSwapchain(gpu, size, format, forceLinear ? linearModifier : modifiers);
        if (const auto surface = std::get_if<std::shared_ptr<GbmSwapchain>>(&ret)) {
            return *surface;
        } else if (std::get<GbmSwapchain::Error>(ret) != GbmSwapchain::Error::ModifiersUnsupported) {
            return nullptr;
        }
    }
    uint32_t gbmFlags = GBM_BO_USE_RENDERING;
    if (m_gpu == gpu) {
        gbmFlags |= GBM_BO_USE_SCANOUT;
    }
    if (forceLinear || m_gpu != gpu) {
        gbmFlags |= GBM_BO_USE_LINEAR;
    }
    const auto ret = GbmSwapchain::createSwapchain(gpu, size, format, gbmFlags);
    const auto swapchain = std::get_if<std::shared_ptr<GbmSwapchain>>(&ret);
    return swapchain ? *swapchain : nullptr;
}

std::shared_ptr<DrmFramebuffer> EglGbmLayerSurface::doRenderTestBuffer(Surface &surface) const
{
    const auto [buffer, repair] = surface.gbmSwapchain->acquire();
    if (!buffer) {
        return nullptr;
    }
    if (const auto ret = importBuffer(surface, buffer)) {
        surface.currentBuffer = buffer;
        surface.currentFramebuffer = ret;
        return ret;
    } else {
        return nullptr;
    }
}

std::shared_ptr<DrmFramebuffer> EglGbmLayerSurface::importBuffer(Surface &surface, const std::shared_ptr<GbmBuffer> &sourceBuffer) const
{
    if (m_bufferTarget == BufferTarget::Dumb || surface.importMode == MultiGpuImportMode::DumbBuffer) {
        return importWithCpu(surface, sourceBuffer.get());
    } else if (surface.importMode == MultiGpuImportMode::Egl) {
        return importWithEgl(surface, sourceBuffer.get());
    } else if (m_gpu != m_eglBackend->gpu()) {
        return importDmabuf(sourceBuffer.get());
    } else {
        const auto ret = DrmFramebuffer::createFramebuffer(sourceBuffer);
        if (!ret) {
            qCWarning(KWIN_DRM, "Failed to create %s framebuffer: %s", formatName(sourceBuffer->format()).name, strerror(errno));
        }
        return ret;
    }
}

std::shared_ptr<DrmFramebuffer> EglGbmLayerSurface::importDmabuf(GbmBuffer *sourceBuffer) const
{
    const auto imported = GbmBuffer::importBuffer(m_gpu, sourceBuffer, sourceBuffer->flags() | GBM_BO_USE_SCANOUT);
    if (!imported) {
        qCWarning(KWIN_DRM, "failed to import %s gbm_bo for multi-gpu usage: %s", formatName(sourceBuffer->format()).name, strerror(errno));
        return nullptr;
    }
    const auto ret = DrmFramebuffer::createFramebuffer(imported);
    if (!ret) {
        qCWarning(KWIN_DRM, "Failed to create %s framebuffer for multi-gpu: %s", formatName(imported->format()).name, strerror(errno));
    }
    return ret;
}

std::shared_ptr<DrmFramebuffer> EglGbmLayerSurface::importWithEgl(Surface &surface, GbmBuffer *sourceBuffer) const
{
    Q_ASSERT(surface.importGbmSwapchain);

    const auto context = m_eglBackend->contextForGpu(m_gpu);
    if (!context || context->isSoftwareRenderer()) {
        return nullptr;
    }
    context->makeCurrent();
    auto &sourceTexture = surface.importedTextureCache[sourceBuffer->bo()];
    if (!sourceTexture) {
        if (std::optional<DmaBufAttributes> attributes = dmaBufAttributesForBo(sourceBuffer->bo())) {
            sourceTexture = context->importDmaBufAsTexture(attributes.value());
        }
    }
    if (!sourceTexture) {
        qCWarning(KWIN_DRM, "failed to import the source texture!");
        return nullptr;
    }
    const auto [localBuffer, repaint] = surface.importGbmSwapchain->acquire();
    auto &[texture, fbo] = surface.importTextureCache[localBuffer->bo()];
    if (!texture) {
        if (std::optional<DmaBufAttributes> attributes = dmaBufAttributesForBo(localBuffer->bo())) {
            texture = context->importDmaBufAsTexture(attributes.value());
        }
        if (!texture) {
            qCWarning(KWIN_DRM, "failed to import the local texture!");
            return nullptr;
        }
    }
    if (!fbo) {
        fbo = std::make_shared<GLFramebuffer>(texture.get());
        if (!fbo->valid()) {
            qCWarning(KWIN_DRM, "failed to create the fbo!");
            fbo.reset();
            return nullptr;
        }
    }
    glBindFramebuffer(GL_FRAMEBUFFER, fbo->handle());
    glViewport(0, 0, fbo->size().width(), fbo->size().height());
    glClearColor(0, 1, 0, 0);
    glClear(GL_COLOR_BUFFER_BIT);

    const auto shader = context->shaderManager()->pushShader(ShaderTrait::MapTexture);
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
    // restore the old context
    context->doneCurrent();
    m_eglBackend->makeCurrent();
    return DrmFramebuffer::createFramebuffer(localBuffer);
}

std::shared_ptr<DrmFramebuffer> EglGbmLayerSurface::importWithCpu(Surface &surface, GbmBuffer *sourceBuffer) const
{
    Q_ASSERT(surface.importDumbSwapchain && !surface.importDumbSwapchain->isEmpty());
    if (!sourceBuffer->map(GBM_BO_TRANSFER_READ)) {
        qCWarning(KWIN_DRM, "mapping a %s gbm_bo failed: %s", formatName(sourceBuffer->format()).name, strerror(errno));
        return nullptr;
    }
    const auto importBuffer = surface.importDumbSwapchain->acquireBuffer();
    if (sourceBuffer->planeCount() != 1 || sourceBuffer->strides()[0] != importBuffer->strides()[0]) {
        qCCritical(KWIN_DRM, "stride of gbm_bo (%d) and dumb buffer (%d) with format %s don't match!",
                   sourceBuffer->strides()[0], importBuffer->strides()[0], formatName(sourceBuffer->format()).name);
        return nullptr;
    }
    if (!memcpy(importBuffer->data(), sourceBuffer->mappedData(), importBuffer->size().height() * importBuffer->strides()[0])) {
        return nullptr;
    }
    const auto ret = DrmFramebuffer::createFramebuffer(importBuffer);
    if (!ret) {
        qCWarning(KWIN_DRM, "Failed to create %s framebuffer for CPU import: %s", formatName(sourceBuffer->format()).name, strerror(errno));
    }
    return ret;
}
}
