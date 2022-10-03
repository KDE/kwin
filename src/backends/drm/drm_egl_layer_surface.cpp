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
#include "surfaceitem_wayland.h"
#include "wayland/linuxdmabufv1clientbuffer.h"
#include "wayland/surface_interface.h"

#include <drm_fourcc.h>
#include <errno.h>
#include <gbm.h>
#include <unistd.h>

namespace KWin
{

static const QVector<uint64_t> linearModifier = {DRM_FORMAT_MOD_LINEAR};

EglGbmLayerSurface::EglGbmLayerSurface(DrmGpu *gpu, EglGbmBackend *eglBackend)
    : m_gpu(gpu)
    , m_eglBackend(eglBackend)
{
}

EglGbmLayerSurface::~EglGbmLayerSurface()
{
    destroyResources();
}

void EglGbmLayerSurface::destroyResources()
{
    m_currentBuffer.reset();
    if (m_gbmSurface && (m_shadowBuffer || m_oldShadowBuffer)) {
        m_gbmSurface->makeContextCurrent();
    }
    m_shadowBuffer.reset();
    m_oldShadowBuffer.reset();
    m_gbmSurface.reset();
    m_oldGbmSurface.reset();
}

std::optional<OutputLayerBeginFrameInfo> EglGbmLayerSurface::startRendering(const QSize &bufferSize, DrmPlane::Transformations renderOrientation, DrmPlane::Transformations bufferOrientation, const QMap<uint32_t, QVector<uint64_t>> &formats, BufferTarget target)
{
    if (!checkGbmSurface(bufferSize, formats, target == BufferTarget::Linear || target == BufferTarget::Dumb)) {
        return std::nullopt;
    }
    if (!m_gbmSurface->makeContextCurrent()) {
        return std::nullopt;
    }

    // shadow buffer
    const QSize renderSize = (renderOrientation & (DrmPlane::Transformation::Rotate90 | DrmPlane::Transformation::Rotate270)) ? m_gbmSurface->size().transposed() : m_gbmSurface->size();
    if (doesShadowBufferFit(m_shadowBuffer.get(), renderSize, renderOrientation, bufferOrientation)) {
        m_oldShadowBuffer.reset();
    } else {
        if (doesShadowBufferFit(m_oldShadowBuffer.get(), renderSize, renderOrientation, bufferOrientation)) {
            m_shadowBuffer = m_oldShadowBuffer;
        } else {
            if (renderOrientation != bufferOrientation) {
                const auto format = m_eglBackend->gbmFormatForDrmFormat(m_gbmSurface->format());
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

    GLFramebuffer::pushFramebuffer(m_gbmSurface->fbo());
    if (m_shadowBuffer) {
        GLFramebuffer::pushFramebuffer(m_shadowBuffer->fbo());
        // the blit after rendering will completely overwrite the back buffer anyways
        return OutputLayerBeginFrameInfo{
            .renderTarget = RenderTarget(m_shadowBuffer->fbo()),
            .repaint = {},
        };
    } else {
        return OutputLayerBeginFrameInfo{
            .renderTarget = RenderTarget(m_gbmSurface->fbo()),
            .repaint = m_gbmSurface->repaintRegion(),
        };
    }
}

void EglGbmLayerSurface::aboutToStartPainting(DrmOutput *output, const QRegion &damagedRegion)
{
    if (m_shadowBuffer) {
        // with a shadow buffer, we always fully damage the surface
        return;
    }
    if (m_gbmSurface && m_gbmSurface->bufferAge() > 0 && !damagedRegion.isEmpty() && m_eglBackend->supportsPartialUpdate()) {
        QVector<EGLint> rects = output->regionToRects(damagedRegion);
        const bool correct = eglSetDamageRegionKHR(m_eglBackend->eglDisplay(), m_gbmSurface->eglSurface(), rects.data(), rects.count() / 4);
        if (!correct) {
            qCWarning(KWIN_DRM) << "eglSetDamageRegionKHR failed:" << getEglErrorString();
        }
    }
}

std::optional<std::tuple<std::shared_ptr<DrmFramebuffer>, QRegion>> EglGbmLayerSurface::endRendering(DrmPlane::Transformations renderOrientation, const QRegion &damagedRegion, BufferTarget target)
{
    if (m_shadowBuffer) {
        GLFramebuffer::popFramebuffer();
        // TODO handle bufferOrientation != Rotate0
        m_shadowBuffer->render(renderOrientation);
    }
    GLFramebuffer::popFramebuffer();
    if (m_gpu == m_eglBackend->gpu() && target != BufferTarget::Dumb) {
        if (const auto buffer = m_gbmSurface->swapBuffers(damagedRegion)) {
            m_currentBuffer = buffer;
            auto ret = DrmFramebuffer::createFramebuffer(buffer);
            if (!ret) {
                qCWarning(KWIN_DRM, "Failed to create framebuffer for EglGbmLayerSurface: %s", strerror(errno));
            }
            return std::tuple(ret, damagedRegion);
        }
    } else {
        if (const auto gbmBuffer = m_gbmSurface->swapBuffers(damagedRegion)) {
            m_currentBuffer = gbmBuffer;
            const auto buffer = target == BufferTarget::Dumb ? importWithCpu() : importBuffer();
            if (buffer) {
                return std::tuple(buffer, damagedRegion);
            }
        }
    }
    return {};
}

bool EglGbmLayerSurface::checkGbmSurface(const QSize &bufferSize, const QMap<uint32_t, QVector<uint64_t>> &formats, bool forceLinear)
{
    forceLinear |= m_importMode == MultiGpuImportMode::DumbBuffer || m_importMode == MultiGpuImportMode::DumbBufferXrgb8888;
    if (doesGbmSurfaceFit(m_gbmSurface.get(), bufferSize, formats)) {
        m_oldGbmSurface.reset();
    } else {
        if (doesGbmSurfaceFit(m_oldGbmSurface.get(), bufferSize, formats)) {
            m_gbmSurface = m_oldGbmSurface;
        } else {
            if (!createGbmSurface(bufferSize, formats, forceLinear)) {
                return false;
            }
            // dmabuf might work with the new surface
            m_importMode = MultiGpuImportMode::Dmabuf;
            m_importSwapchain.reset();
            m_oldImportSwapchain.reset();
        }
    }
    return m_gbmSurface != nullptr;
}

bool EglGbmLayerSurface::createGbmSurface(const QSize &size, uint32_t format, const QVector<uint64_t> &modifiers, bool forceLinear)
{
    static bool modifiersEnvSet = false;
    static const bool modifiersEnv = qEnvironmentVariableIntValue("KWIN_DRM_USE_MODIFIERS", &modifiersEnvSet) != 0;
    bool allowModifiers = m_gpu->addFB2ModifiersSupported() && (!modifiersEnvSet || (modifiersEnvSet && modifiersEnv)) && !modifiers.isEmpty();
#if !HAVE_GBM_BO_GET_FD_FOR_PLANE
    allowModifiers &= m_gpu == m_eglBackend->gpu();
#endif
    const auto config = m_eglBackend->config(format);
    if (!config) {
        return false;
    }

    if (allowModifiers) {
        const auto ret = GbmSurface::createSurface(m_eglBackend, size, format, forceLinear ? linearModifier : modifiers, config);
        if (const auto surface = std::get_if<std::shared_ptr<GbmSurface>>(&ret)) {
            m_oldGbmSurface = m_gbmSurface;
            m_gbmSurface = *surface;
            return true;
        } else if (std::get<GbmSurface::Error>(ret) != GbmSurface::Error::ModifiersUnsupported) {
            return false;
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
    if (const auto surface = std::get_if<std::shared_ptr<GbmSurface>>(&ret)) {
        m_oldGbmSurface = m_gbmSurface;
        m_gbmSurface = *surface;
        return true;
    } else {
        return false;
    }
}

bool EglGbmLayerSurface::createGbmSurface(const QSize &size, const QMap<uint32_t, QVector<uint64_t>> &formats, bool forceLinear)
{
    QVector<GbmFormat> sortedFormats;
    for (auto it = formats.begin(); it != formats.end(); it++) {
        const auto format = m_eglBackend->gbmFormatForDrmFormat(it.key());
        if (format.has_value()) {
            sortedFormats << format.value();
        }
    }
    std::sort(sortedFormats.begin(), sortedFormats.end(), [this](const auto &lhs, const auto &rhs) {
        if (lhs.drmFormat == rhs.drmFormat) {
            // prefer having an alpha channel
            return lhs.alphaSize > rhs.alphaSize;
        } else if (m_eglBackend->prefer10bpc() && ((lhs.bpp == 30) != (rhs.bpp == 30))) {
            // prefer 10bpc / 30bpp formats
            return lhs.bpp == 30 ? true : false;
        } else {
            // fallback
            return lhs.drmFormat < rhs.drmFormat;
        }
    });
    for (const auto &format : qAsConst(sortedFormats)) {
        if (m_importMode == MultiGpuImportMode::DumbBufferXrgb8888 && format.drmFormat != DRM_FORMAT_XRGB8888) {
            continue;
        }
        if (formats.contains(format.drmFormat) && createGbmSurface(size, format.drmFormat, formats[format.drmFormat], forceLinear)) {
            return true;
        }
    }
    qCCritical(KWIN_DRM, "Failed to create a gbm surface!");
    return false;
}

bool EglGbmLayerSurface::doesGbmSurfaceFit(GbmSurface *surf, const QSize &size, const QMap<uint32_t, QVector<uint64_t>> &formats) const
{
    return surf && surf->size() == size
        && formats.contains(surf->format())
        && (m_importMode != MultiGpuImportMode::DumbBufferXrgb8888 || surf->format() == DRM_FORMAT_XRGB8888)
        && (surf->modifiers().isEmpty() || formats[surf->format()] == surf->modifiers());
}

bool EglGbmLayerSurface::doesShadowBufferFit(ShadowBuffer *buffer, const QSize &size, DrmPlane::Transformations renderOrientation, DrmPlane::Transformations bufferOrientation) const
{
    if (renderOrientation != bufferOrientation) {
        return buffer && buffer->texture()->size() == size && buffer->drmFormat() == m_gbmSurface->format();
    } else {
        return buffer == nullptr;
    }
}

std::shared_ptr<DrmFramebuffer> EglGbmLayerSurface::importBuffer()
{
    if (m_importMode == MultiGpuImportMode::Dmabuf) {
        if (const auto buffer = importDmabuf()) {
            return buffer;
        } else {
            // don't bother trying again, it will most likely fail every time
            m_importMode = MultiGpuImportMode::DumbBuffer;
        }
    }
    if (const auto buffer = importWithCpu()) {
        return buffer;
    } else if (m_importMode == MultiGpuImportMode::DumbBuffer) {
        m_importMode = MultiGpuImportMode::DumbBufferXrgb8888;
        return nullptr;
    }
    if (m_importMode != MultiGpuImportMode::Failed) {
        qCCritical(KWIN_DRM, "All multi gpu imports failed!");
        m_importMode = MultiGpuImportMode::Failed;
    }
    return nullptr;
}

std::shared_ptr<DrmFramebuffer> EglGbmLayerSurface::importDmabuf()
{
    const auto imported = GbmBuffer::importBuffer(m_gpu, m_currentBuffer.get(), m_gbmSurface->flags() | GBM_BO_USE_SCANOUT);
    if (!imported) {
        qCWarning(KWIN_DRM, "failed to import gbm_bo for multi-gpu usage: %s", strerror(errno));
        return nullptr;
    }
    const auto ret = DrmFramebuffer::createFramebuffer(imported);
    if (!ret) {
        qCWarning(KWIN_DRM, "Failed to create framebuffer for multi-gpu: %s", strerror(errno));
    }
    return ret;
}

std::shared_ptr<DrmFramebuffer> EglGbmLayerSurface::importWithCpu()
{
    if (doesSwapchainFit(m_importSwapchain.get())) {
        m_oldImportSwapchain.reset();
    } else {
        if (doesSwapchainFit(m_oldImportSwapchain.get())) {
            m_importSwapchain = m_oldImportSwapchain;
        } else {
            const auto swapchain = std::make_shared<DumbSwapchain>(m_gpu, m_gbmSurface->size(), m_gbmSurface->format());
            if (swapchain->isEmpty()) {
                return nullptr;
            }
            m_importSwapchain = swapchain;
        }
    }

    if (!m_currentBuffer->map(GBM_BO_TRANSFER_READ)) {
        qCWarning(KWIN_DRM, "mapping a gbm_bo failed: %s", strerror(errno));
        return nullptr;
    }
    const auto importBuffer = m_importSwapchain->acquireBuffer();
    if (m_currentBuffer->planeCount() != 1 || m_currentBuffer->strides()[0] != importBuffer->strides()[0]) {
        qCCritical(KWIN_DRM, "stride of gbm_bo (%d) and dumb buffer (%d) don't match!", m_currentBuffer->strides()[0], importBuffer->strides()[0]);
        return nullptr;
    }
    if (!memcpy(importBuffer->data(), m_currentBuffer->mappedData(), importBuffer->size().height() * importBuffer->strides()[0])) {
        return nullptr;
    }
    const auto ret = DrmFramebuffer::createFramebuffer(importBuffer);
    if (!ret) {
        qCWarning(KWIN_DRM, "Failed to create framebuffer for CPU import: %s", strerror(errno));
    }
    return ret;
}

bool EglGbmLayerSurface::doesSwapchainFit(DumbSwapchain *swapchain) const
{
    return swapchain && swapchain->size() == m_gbmSurface->size() && swapchain->drmFormat() == m_gbmSurface->format();
}

EglGbmBackend *EglGbmLayerSurface::eglBackend() const
{
    return m_eglBackend;
}

bool EglGbmLayerSurface::doesSurfaceFit(const QSize &size, const QMap<uint32_t, QVector<uint64_t>> &formats) const
{
    return doesGbmSurfaceFit(m_gbmSurface.get(), size, formats);
}

std::shared_ptr<GLTexture> EglGbmLayerSurface::texture() const
{
    if (m_shadowBuffer) {
        return m_shadowBuffer->texture();
    }
    if (!m_currentBuffer) {
        qCWarning(KWIN_DRM) << "Failed to record frame: No gbm buffer!";
        return nullptr;
    }
    return m_eglBackend->importBufferObjectAsTexture(m_currentBuffer->bo());
}

std::shared_ptr<DrmFramebuffer> EglGbmLayerSurface::renderTestBuffer(const QSize &bufferSize, const QMap<uint32_t, QVector<uint64_t>> &formats, BufferTarget target)
{
    if (!checkGbmSurface(bufferSize, formats, target == BufferTarget::Linear) || !m_gbmSurface->makeContextCurrent()) {
        return nullptr;
    }
    glClear(GL_COLOR_BUFFER_BIT);
    m_currentBuffer = m_gbmSurface->swapBuffers(infiniteRegion());
    if (m_currentBuffer) {
        if (m_gpu != m_eglBackend->gpu()) {
            auto oldImportMode = m_importMode;
            auto buffer = target == BufferTarget::Dumb ? importWithCpu() : importBuffer();
            if (buffer) {
                return buffer;
            } else if (m_importMode != oldImportMode) {
                // try again with the new import mode
                // recursion depth is limited by the number of import modes
                return renderTestBuffer(bufferSize, formats);
            } else {
                return nullptr;
            }
        }
        const auto ret = DrmFramebuffer::createFramebuffer(m_currentBuffer);
        if (!ret) {
            qCWarning(KWIN_DRM, "Failed to create framebuffer for testing: %s", strerror(errno));
        }
        return ret;
    } else {
        return nullptr;
    }
}
}
