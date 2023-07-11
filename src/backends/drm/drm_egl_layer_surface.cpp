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
#include "drm_gbm_surface.h"
#include "drm_gpu.h"
#include "drm_logging.h"
#include "drm_output.h"
#include "drm_shadow_buffer.h"
#include "egl_dmabuf.h"
#include "kwineglutils_p.h"
#include "scene/surfaceitem_wayland.h"
#include "wayland/linuxdmabufv1clientbuffer.h"
#include "wayland/surface_interface.h"

#include <cstring>
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
    if (m_surface.gbmSurface && (m_shadowBuffer || m_oldShadowBuffer)) {
        m_surface.gbmSurface->makeContextCurrent();
    }
    m_shadowBuffer.reset();
    m_oldShadowBuffer.reset();
    m_surface = {};
    m_oldSurface = {};
}

std::optional<OutputLayerBeginFrameInfo> EglGbmLayerSurface::startRendering(const QSize &bufferSize, DrmPlane::Transformations renderOrientation, DrmPlane::Transformations bufferOrientation, const QMap<uint32_t, QVector<uint64_t>> &formats)
{
    if (!checkSurface(bufferSize, formats)) {
        return std::nullopt;
    }
    if (!m_surface.gbmSurface->makeContextCurrent()) {
        return std::nullopt;
    }

    // shadow buffer
    const QSize renderSize = (renderOrientation & (DrmPlane::Transformation::Rotate90 | DrmPlane::Transformation::Rotate270)) ? m_surface.gbmSurface->size().transposed() : m_surface.gbmSurface->size();
    if (doesShadowBufferFit(m_shadowBuffer.get(), renderSize, renderOrientation, bufferOrientation)) {
        m_oldShadowBuffer.reset();
    } else {
        if (doesShadowBufferFit(m_oldShadowBuffer.get(), renderSize, renderOrientation, bufferOrientation)) {
            m_shadowBuffer = m_oldShadowBuffer;
        } else {
            if (renderOrientation != bufferOrientation) {
                const auto format = m_eglBackend->gbmFormatForDrmFormat(m_surface.gbmSurface->format());
                if (!format.has_value()) {
                    return std::nullopt;
                }
                m_shadowBuffer = std::make_shared<ShadowBuffer>(renderSize, format.value());
                if (!m_shadowBuffer->isComplete()) {
                    return std::nullopt;
                }
            } else {
                m_shadowBuffer.reset();
            }
        }
    }

    if (m_shadowBuffer) {
        // the blit after rendering will completely overwrite the back buffer anyways
        return OutputLayerBeginFrameInfo{
            .renderTarget = RenderTarget(m_shadowBuffer->fbo()),
            .repaint = {},
        };
    } else {
        return OutputLayerBeginFrameInfo{
            .renderTarget = RenderTarget(m_surface.gbmSurface->fbo()),
            .repaint = m_surface.gbmSurface->repaintRegion(),
        };
    }
}

void EglGbmLayerSurface::aboutToStartPainting(DrmOutput *output, const QRegion &damagedRegion)
{
    if (m_shadowBuffer) {
        // with a shadow buffer, we always fully damage the surface
        return;
    }
    if (m_surface.gbmSurface && m_surface.gbmSurface->bufferAge() > 0 && !damagedRegion.isEmpty() && m_eglBackend->supportsPartialUpdate()) {
        QVector<EGLint> rects = output->regionToRects(damagedRegion);
        const bool correct = eglSetDamageRegionKHR(m_eglBackend->eglDisplay(), m_surface.gbmSurface->eglSurface(), rects.data(), rects.count() / 4);
        if (!correct) {
            qCWarning(KWIN_DRM) << "eglSetDamageRegionKHR failed:" << getEglErrorString();
        }
    }
}

bool EglGbmLayerSurface::endRendering(DrmPlane::Transformations renderOrientation, const QRegion &damagedRegion)
{
    if (m_shadowBuffer) {
        GLFramebuffer::pushFramebuffer(m_surface.gbmSurface->fbo());
        // TODO handle bufferOrientation != Rotate0
        m_shadowBuffer->render(renderOrientation);
        GLFramebuffer::popFramebuffer();
    }
    const auto gbmBuffer = m_surface.gbmSurface->swapBuffers(damagedRegion);
    if (!gbmBuffer) {
        return false;
    }
    const auto buffer = importBuffer(m_surface, gbmBuffer);
    if (buffer) {
        m_surface.currentBuffer = gbmBuffer;
        m_surface.currentFramebuffer = buffer;
        return true;
    } else {
        return false;
    }
}

bool EglGbmLayerSurface::doesShadowBufferFit(ShadowBuffer *buffer, const QSize &size, DrmPlane::Transformations renderOrientation, DrmPlane::Transformations bufferOrientation) const
{
    if (renderOrientation != bufferOrientation) {
        return buffer && buffer->texture()->size() == size && buffer->drmFormat() == m_surface.gbmSurface->format();
    } else {
        return buffer == nullptr;
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

bool EglGbmLayerSurface::doesSurfaceFit(const QSize &size, const QMap<uint32_t, QVector<uint64_t>> &formats) const
{
    return doesSurfaceFit(m_surface, size, formats);
}

std::shared_ptr<GLTexture> EglGbmLayerSurface::texture() const
{
    if (m_shadowBuffer) {
        return m_shadowBuffer->texture();
    }
    if (!m_surface.currentBuffer) {
        qCWarning(KWIN_DRM) << "Failed to record frame: No gbm buffer!";
        return nullptr;
    }
    return m_eglBackend->importBufferObjectAsTexture(m_surface.currentBuffer->bo());
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
    return surface.gbmSurface
        && surface.gbmSurface->size() == size
        && formats.contains(surface.gbmSurface->format())
        && (surface.forceLinear || surface.gbmSurface->modifiers().empty() || surface.gbmSurface->modifiers() == formats[surface.gbmSurface->format()]);
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
    const auto testFormats = [this, &size, &formats](const QVector<GbmFormat> &gbmFormats, MultiGpuImportMode importMode) -> std::optional<Surface> {
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
    std::sort(preferredFormats.begin(), preferredFormats.end(), sort);
    if (const auto surface = testFormats(preferredFormats, MultiGpuImportMode::Dmabuf)) {
        return surface;
    }
    if (m_gpu != m_eglBackend->gpu()) {
        if (const auto surface = testFormats(preferredFormats, MultiGpuImportMode::DumbBuffer)) {
            return surface;
        }
    }
    std::sort(fallbackFormats.begin(), fallbackFormats.end(), sort);
    if (const auto surface = testFormats(fallbackFormats, MultiGpuImportMode::Dmabuf)) {
        return surface;
    }
    if (m_gpu != m_eglBackend->gpu()) {
        if (const auto surface = testFormats(fallbackFormats, MultiGpuImportMode::DumbBuffer)) {
            return surface;
        }
    }
    return std::nullopt;
}

std::optional<EglGbmLayerSurface::Surface> EglGbmLayerSurface::createSurface(const QSize &size, uint32_t format, const QVector<uint64_t> &modifiers, MultiGpuImportMode importMode) const
{
    Surface ret;
    ret.importMode = importMode;
    ret.forceLinear = importMode == MultiGpuImportMode::DumbBuffer || m_bufferTarget != BufferTarget::Normal;
    ret.gbmSurface = createGbmSurface(size, format, modifiers, ret.forceLinear);
    if (!ret.gbmSurface) {
        return std::nullopt;
    }
    if (importMode == MultiGpuImportMode::DumbBuffer || m_bufferTarget == BufferTarget::Dumb) {
        ret.importSwapchain = std::make_shared<DumbSwapchain>(m_gpu, size, format);
        if (ret.importSwapchain->isEmpty()) {
            return std::nullopt;
        }
    }
    if (!doRenderTestBuffer(ret)) {
        return std::nullopt;
    }
    return ret;
}

std::shared_ptr<GbmSurface> EglGbmLayerSurface::createGbmSurface(const QSize &size, uint32_t format, const QVector<uint64_t> &modifiers, bool forceLinear) const
{
    static bool modifiersEnvSet = false;
    static const bool modifiersEnv = qEnvironmentVariableIntValue("KWIN_DRM_USE_MODIFIERS", &modifiersEnvSet) != 0;
    bool allowModifiers = m_gpu->addFB2ModifiersSupported() && (!modifiersEnvSet || (modifiersEnvSet && modifiersEnv)) && !modifiers.isEmpty();
#if !HAVE_GBM_BO_GET_FD_FOR_PLANE
    allowModifiers &= m_gpu == m_eglBackend->gpu();
#endif
    const auto config = m_eglBackend->config(format);
    if (!config) {
        return nullptr;
    }

    if (allowModifiers) {
        const auto ret = GbmSurface::createSurface(m_eglBackend, size, format, forceLinear ? linearModifier : modifiers, config);
        if (const auto surface = std::get_if<std::shared_ptr<GbmSurface>>(&ret)) {
            return *surface;
        } else if (std::get<GbmSurface::Error>(ret) != GbmSurface::Error::ModifiersUnsupported) {
            return nullptr;
        }
    }
    uint32_t gbmFlags = GBM_BO_USE_RENDERING;
    if (m_gpu == m_eglBackend->gpu()) {
        gbmFlags |= GBM_BO_USE_SCANOUT;
    }
    if (forceLinear || m_gpu != m_eglBackend->gpu()) {
        gbmFlags |= GBM_BO_USE_LINEAR;
    }
    const auto ret = GbmSurface::createSurface(m_eglBackend, size, format, gbmFlags, config);
    const auto surface = std::get_if<std::shared_ptr<GbmSurface>>(&ret);
    return surface ? *surface : nullptr;
}

std::shared_ptr<DrmFramebuffer> EglGbmLayerSurface::doRenderTestBuffer(Surface &surface) const
{
    if (!surface.gbmSurface->makeContextCurrent()) {
        return nullptr;
    }
    glClear(GL_COLOR_BUFFER_BIT);
    const auto buffer = surface.gbmSurface->swapBuffers(infiniteRegion());
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

std::shared_ptr<DrmFramebuffer> EglGbmLayerSurface::importWithCpu(Surface &surface, GbmBuffer *sourceBuffer) const
{
    Q_ASSERT(surface.importSwapchain && !surface.importSwapchain->isEmpty());
    const auto map = sourceBuffer->map(GBM_BO_TRANSFER_READ);
    if (!map.data) {
        qCWarning(KWIN_DRM, "mapping a %s gbm_bo failed: %s", formatName(sourceBuffer->format()).name, strerror(errno));
        return nullptr;
    }
    const auto importBuffer = surface.importSwapchain->acquireBuffer();
    if (map.stride == importBuffer->strides()[0]) {
        std::memcpy(importBuffer->data(), map.data, importBuffer->size().height() * importBuffer->strides()[0]);
    } else {
        const uint64_t usedLineWidth = std::min(map.stride, importBuffer->strides()[0]);
        for (int i = 0; i < importBuffer->size().height(); i++) {
            const char *srcAddress = reinterpret_cast<const char *>(map.data) + map.stride * i;
            char *dstAddress = reinterpret_cast<char *>(importBuffer->data()) + importBuffer->strides()[0] * i;
            std::memcpy(dstAddress, srcAddress, usedLineWidth);
        }
    }
    const auto ret = DrmFramebuffer::createFramebuffer(importBuffer);
    if (!ret) {
        qCWarning(KWIN_DRM, "Failed to create %s framebuffer for CPU import: %s", formatName(sourceBuffer->format()).name, strerror(errno));
    }
    return ret;
}
}
