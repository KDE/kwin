/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2022 Xaver Hugl <xaver.hugl@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "egl_gbm_layer_surface.h"

#include "config-kwin.h"
#include "drm_gpu.h"
#include "drm_output.h"
#include "dumb_swapchain.h"
#include "egl_dmabuf.h"
#include "egl_gbm_backend.h"
#include "gbm_surface.h"
#include "kwineglutils_p.h"
#include "logging.h"
#include "shadowbuffer.h"
#include "surfaceitem_wayland.h"

#include <KWaylandServer/linuxdmabufv1clientbuffer.h>
#include <KWaylandServer/surface_interface.h>

#include <drm_fourcc.h>
#include <errno.h>
#include <gbm.h>
#include <unistd.h>

namespace KWin
{

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
    if (m_gbmSurface && (m_shadowBuffer || m_oldShadowBuffer)) {
        m_gbmSurface->makeContextCurrent();
    }
    m_shadowBuffer.reset();
    m_oldShadowBuffer.reset();
    m_gbmSurface.reset();
    m_oldGbmSurface.reset();
}

OutputLayerBeginFrameInfo EglGbmLayerSurface::startRendering(const QSize &bufferSize, DrmPlane::Transformations renderTransform, DrmPlane::Transformations bufferTransform, const QMap<uint32_t, QVector<uint64_t>> &formats)
{
    // gbm surface
    if (doesGbmSurfaceFit(m_gbmSurface.data(), bufferSize, formats)) {
        m_oldGbmSurface.reset();
    } else {
        if (doesGbmSurfaceFit(m_oldGbmSurface.data(), bufferSize, formats)) {
            m_gbmSurface = m_oldGbmSurface;
        } else {
            if (!createGbmSurface(bufferSize, formats)) {
                return {};
            }
            // dmabuf might work with the new surface
            m_importMode = MultiGpuImportMode::Dmabuf;
            m_importSwapchain.reset();
            m_oldImportSwapchain.reset();
        }
    }
    if (!m_gbmSurface->makeContextCurrent()) {
        return {};
    }

    // shadow buffer
    const QSize renderSize = (renderTransform & (DrmPlane::Transformation::Rotate90 | DrmPlane::Transformation::Rotate270)) ? m_gbmSurface->size().transposed() : m_gbmSurface->size();
    if (doesShadowBufferFit(m_shadowBuffer.data(), renderSize, renderTransform, bufferTransform)) {
        m_oldShadowBuffer.reset();
    } else {
        if (doesShadowBufferFit(m_oldShadowBuffer.data(), renderSize, renderTransform, bufferTransform)) {
            m_shadowBuffer = m_oldShadowBuffer;
        } else {
            if (renderTransform != bufferTransform) {
                const auto format = m_eglBackend->gbmFormatForDrmFormat(m_gbmSurface->format());
                if (!format.has_value()) {
                    return {};
                }
                m_shadowBuffer = QSharedPointer<ShadowBuffer>::create(renderSize, format.value());
                if (!m_shadowBuffer->isComplete()) {
                    return {};
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
        return OutputLayerBeginFrameInfo {
            .renderTarget = RenderTarget(m_shadowBuffer->fbo()),
            .repaint = {},
        };
    } else {
        return OutputLayerBeginFrameInfo {
            .renderTarget = RenderTarget(m_gbmSurface->fbo()),
            .repaint = m_gbmSurface->repaintRegion(),
        };
    }
}

void EglGbmLayerSurface::aboutToStartPainting(DrmOutput *output, const QRegion &damagedRegion)
{
    if (m_gbmSurface && m_gbmSurface->bufferAge() > 0 && !damagedRegion.isEmpty() && m_eglBackend->supportsPartialUpdate()) {
        QVector<EGLint> rects = output->regionToRects(damagedRegion);
        const bool correct = eglSetDamageRegionKHR(m_eglBackend->eglDisplay(), m_gbmSurface->eglSurface(), rects.data(), rects.count() / 4);
        if (!correct) {
            qCWarning(KWIN_DRM) << "eglSetDamageRegionKHR failed:" << getEglErrorString();
        }
    }
}

std::optional<std::tuple<QSharedPointer<DrmBuffer>, QRegion>> EglGbmLayerSurface::endRendering(DrmPlane::Transformations renderTransform, const QRegion &damagedRegion)
{
    if (m_shadowBuffer) {
        GLFramebuffer::popFramebuffer();
        // TODO handle bufferTransform != Rotate0
        m_shadowBuffer->render(renderTransform);
    }
    GLFramebuffer::popFramebuffer();
    QSharedPointer<DrmBuffer> buffer;
    if (m_gpu == m_eglBackend->gpu()) {
        buffer = m_gbmSurface->swapBuffersForDrm(damagedRegion);
    } else {
        if (m_gbmSurface->swapBuffers(damagedRegion)) {
            buffer = importBuffer();
        }
    }
    if (buffer) {
        return std::tuple(buffer, damagedRegion);
    } else {
        return {};
    }
}

bool EglGbmLayerSurface::createGbmSurface(const QSize &size, uint32_t format, const QVector<uint64_t> &modifiers)
{
    static bool modifiersEnvSet = false;
    static const bool modifiersEnv = qEnvironmentVariableIntValue("KWIN_DRM_USE_MODIFIERS", &modifiersEnvSet) != 0;
    const bool allowModifiers = m_eglBackend->gpu()->addFB2ModifiersSupported() && m_gpu->addFB2ModifiersSupported()
        && ((m_eglBackend->gpu()->isNVidia() && !modifiersEnvSet) || (modifiersEnvSet && modifiersEnv));

    const auto config = m_eglBackend->config(format);

    QSharedPointer<GbmSurface> gbmSurface;
#if HAVE_GBM_BO_GET_FD_FOR_PLANE
    if (!allowModifiers) {
#else
    // modifiers have to be disabled with multi-gpu if gbm_bo_get_fd_for_plane is not available
    if (!allowModifiers || m_pipeline->gpu() != m_eglBackend->gpu()) {
#endif
        int flags = GBM_BO_USE_RENDERING;
        if (m_gpu == m_eglBackend->gpu()) {
            flags |= GBM_BO_USE_SCANOUT;
        } else {
            flags |= GBM_BO_USE_LINEAR;
        }
        gbmSurface = QSharedPointer<GbmSurface>::create(m_eglBackend->gpu(), size, format, flags, config);
    } else {
        gbmSurface = QSharedPointer<GbmSurface>::create(m_eglBackend->gpu(), size, format, modifiers, config);
        if (!gbmSurface->isValid()) {
            // the egl / gbm implementation may reject the modifier list from another gpu
            // as a fallback use linear, to at least make CPU copy more efficient
            const QVector<uint64_t> linear = {DRM_FORMAT_MOD_LINEAR};
            gbmSurface = QSharedPointer<GbmSurface>::create(m_eglBackend->gpu(), size, format, linear, config);
        }
    }
    if (gbmSurface->isValid()) {
        m_oldGbmSurface = m_gbmSurface;
        m_gbmSurface = gbmSurface;
        return true;
    } else {
        return false;
    }
}

bool EglGbmLayerSurface::createGbmSurface(const QSize &size, const QMap<uint32_t, QVector<uint64_t>> &formats)
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
        if (formats.contains(format.drmFormat) && createGbmSurface(size, format.drmFormat, formats[format.drmFormat])) {
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

bool EglGbmLayerSurface::doesShadowBufferFit(ShadowBuffer *buffer, const QSize &size, DrmPlane::Transformations renderTransform, DrmPlane::Transformations bufferTransform) const
{
    if (renderTransform != bufferTransform) {
        return buffer && buffer->texture()->size() == size && buffer->drmFormat() == m_gbmSurface->format();
    } else {
        return buffer == nullptr;
    }
}

QSharedPointer<DrmBuffer> EglGbmLayerSurface::importBuffer()
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

QSharedPointer<DrmBuffer> EglGbmLayerSurface::importDmabuf()
{
    const auto bo = m_gbmSurface->currentBuffer()->getBo();
    gbm_bo *importedBuffer;
#if HAVE_GBM_BO_GET_FD_FOR_PLANE
    if (gbm_bo_get_handle_for_plane(bo, 0).s32 != -1) {
        gbm_import_fd_modifier_data data = {
            .width = gbm_bo_get_width(bo),
            .height = gbm_bo_get_height(bo),
            .format = gbm_bo_get_format(bo),
            .num_fds = static_cast<uint32_t>(gbm_bo_get_plane_count(bo)),
            .fds = {},
            .strides = {},
            .offsets = {},
            .modifier = gbm_bo_get_modifier(bo),
        };
        for (uint32_t i = 0; i < data.num_fds; i++) {
            data.fds[i] = gbm_bo_get_fd_for_plane(bo, i);
            if (data.fds[i] < 0) {
                qCWarning(KWIN_DRM, "failed to export gbm_bo plane %d as dma-buf: %s", i, strerror(errno));
                for (uint32_t f = 0; f < i; f++) {
                    close(data.fds[f]);
                }
                return nullptr;
            }
            data.strides[i] = gbm_bo_get_stride_for_plane(bo, i);
            data.offsets[i] = gbm_bo_get_offset(bo, i);
        }
        importedBuffer = gbm_bo_import(m_gpu->gbmDevice(), GBM_BO_IMPORT_FD_MODIFIER, &data, GBM_BO_USE_SCANOUT);
    } else {
#endif
        gbm_import_fd_data data = {
            .fd = gbm_bo_get_fd(bo),
            .width = gbm_bo_get_width(bo),
            .height = gbm_bo_get_height(bo),
            .stride = gbm_bo_get_stride(bo),
            .format = gbm_bo_get_format(bo),
        };
        if (data.fd < 0) {
            qCWarning(KWIN_DRM, "failed to export gbm_bo as dma-buf: %s", strerror(errno));
            return nullptr;
        }
        importedBuffer = gbm_bo_import(m_gpu->gbmDevice(), GBM_BO_IMPORT_FD_MODIFIER, &data, GBM_BO_USE_SCANOUT);
#if HAVE_GBM_BO_GET_FD_FOR_PLANE
    }
#endif
    if (!importedBuffer) {
        qCWarning(KWIN_DRM, "failed to import gbm_bo for multi-gpu usage: %s", strerror(errno));
        return nullptr;
    }
    const auto buffer = QSharedPointer<DrmGbmBuffer>::create(m_gpu, nullptr, importedBuffer);
    return buffer->bufferId() ? buffer : nullptr;
}

QSharedPointer<DrmBuffer> EglGbmLayerSurface::importWithCpu()
{
    if (doesSwapchainFit(m_importSwapchain.data())) {
        m_oldImportSwapchain.reset();
    } else {
        if (doesSwapchainFit(m_oldImportSwapchain.data())) {
            m_importSwapchain = m_oldImportSwapchain;
        } else {
            const auto swapchain = QSharedPointer<DumbSwapchain>::create(m_gpu, m_gbmSurface->size(), m_gbmSurface->format());
            if (swapchain->isEmpty()) {
                return nullptr;
            }
            m_importSwapchain = swapchain;
        }
    }

    const auto bo = m_gbmSurface->currentBuffer();
    if (!bo->map(GBM_BO_TRANSFER_READ)) {
        qCWarning(KWIN_DRM, "mapping a gbm_bo failed: %s", strerror(errno));
        return nullptr;
    }
    const auto importBuffer = m_importSwapchain->acquireBuffer();
    if (bo->stride() != importBuffer->stride()) {
        qCCritical(KWIN_DRM, "stride of gbm_bo (%d) and dumb buffer (%d) don't match!", bo->stride(), importBuffer->stride());
        return nullptr;
    }
    if (!memcpy(importBuffer->data(), bo->mappedData(), importBuffer->size().height() * importBuffer->stride())) {
        return nullptr;
    }
    return importBuffer;
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
    return doesGbmSurfaceFit(m_gbmSurface.data(), size, formats);
}

QSharedPointer<GLTexture> EglGbmLayerSurface::texture() const
{
    if (m_shadowBuffer) {
        return m_shadowBuffer->texture();
    }
    GbmBuffer *gbmBuffer = m_gbmSurface->currentBuffer().data();
    if (!gbmBuffer) {
        qCWarning(KWIN_DRM) << "Failed to record frame: No gbm buffer!";
        return nullptr;
    }
    return gbmBuffer->createTexture(m_eglBackend->eglDisplay());
}
}
