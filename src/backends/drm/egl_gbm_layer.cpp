/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2022 Xaver Hugl <xaver.hugl@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "egl_gbm_layer.h"
#include "gbm_surface.h"
#include "drm_abstract_output.h"
#include "drm_gpu.h"
#include "egl_gbm_backend.h"
#include "shadowbuffer.h"
#include "drm_output.h"
#include "drm_pipeline.h"
#include "dumb_swapchain.h"
#include "logging.h"
#include "egl_dmabuf.h"
#include "surfaceitem_wayland.h"
#include "kwineglimagetexture.h"
#include "drm_backend.h"
#include "kwineglutils_p.h"

#include "KWaylandServer/surface_interface.h"
#include "KWaylandServer/linuxdmabufv1clientbuffer.h"

#include <QRegion>
#include <drm_fourcc.h>
#include <gbm.h>
#include <errno.h>
#include <unistd.h>

namespace KWin
{

EglGbmLayer::EglGbmLayer(EglGbmBackend *eglBackend, DrmDisplayDevice *displayDevice)
    : DrmLayer(displayDevice)
    , m_eglBackend(eglBackend)
{
    connect(eglBackend, &EglGbmBackend::aboutToBeDestroyed, this, &EglGbmLayer::destroyResources);
}

EglGbmLayer::~EglGbmLayer()
{
    destroyResources();
}

void EglGbmLayer::destroyResources()
{
    if (m_shadowBuffer || m_oldShadowBuffer) {
        if (m_gbmSurface) {
            m_gbmSurface->makeContextCurrent();
        } else if (m_oldGbmSurface) {
            m_oldGbmSurface->makeContextCurrent();
        }
    }
    m_shadowBuffer.reset();
    m_oldShadowBuffer.reset();
    m_gbmSurface.reset();
    m_oldGbmSurface.reset();
}

std::optional<QRegion> EglGbmLayer::startRendering()
{
    m_scanoutBuffer.reset();
    // dmabuf feedback
    if (!m_scanoutCandidate.attemptedThisFrame && m_scanoutCandidate.surface) {
        if (const auto feedback = m_scanoutCandidate.surface->dmabufFeedbackV1()) {
            feedback->setTranches({});
        }
        m_scanoutCandidate.surface = nullptr;
    }
    m_scanoutCandidate.attemptedThisFrame = false;

    // gbm surface
    if (doesGbmSurfaceFit(m_gbmSurface.data())) {
        m_oldGbmSurface.reset();
    } else {
        if (doesGbmSurfaceFit(m_oldGbmSurface.data())) {
            m_gbmSurface = m_oldGbmSurface;
        } else {
            if (!createGbmSurface()) {
                return std::optional<QRegion>();
            }
            // dmabuf might work with the new surface
            m_importMode = MultiGpuImportMode::Dmabuf;
        }
    }
    if (!m_gbmSurface->makeContextCurrent()) {
        return std::optional<QRegion>();
    }
    auto repaintRegion = m_gbmSurface->repaintRegion(m_displayDevice->renderGeometry());

    // shadow buffer
    if (doesShadowBufferFit(m_shadowBuffer.data())) {
        m_oldShadowBuffer.reset();
    } else {
        if (doesShadowBufferFit(m_oldShadowBuffer.data())) {
            m_shadowBuffer = m_oldShadowBuffer;
        } else {
            if (m_displayDevice->softwareTransforms() != DrmPlane::Transformations(DrmPlane::Transformation::Rotate0)) {
                const auto format = m_eglBackend->gbmFormatForDrmFormat(m_gbmSurface->format());
                m_shadowBuffer = QSharedPointer<ShadowBuffer>::create(m_displayDevice->sourceSize(), format);
                if (!m_shadowBuffer->isComplete()) {
                    return std::optional<QRegion>();
                }
            } else {
                m_shadowBuffer.reset();
            }
        }
    }

    GLRenderTarget::pushRenderTarget(m_gbmSurface->renderTarget());
    if (m_shadowBuffer) {
        // the blit after rendering will completely overwrite the back buffer anyways
        repaintRegion = QRegion();
        GLRenderTarget::pushRenderTarget(m_shadowBuffer->renderTarget());
    }

    return repaintRegion;
}

bool EglGbmLayer::endRendering(const QRegion &damagedRegion)
{
    if (m_shadowBuffer) {
        GLRenderTarget::popRenderTarget();
        m_shadowBuffer->render(m_displayDevice);
    }
    GLRenderTarget::popRenderTarget();
    const auto buffer = m_gbmSurface->swapBuffersForDrm(damagedRegion);
    if (buffer) {
        m_currentBuffer = buffer;
        m_currentDamage = damagedRegion;
    }
    return buffer;
}

QRegion EglGbmLayer::currentDamage() const
{
    return m_currentDamage;
}

QSharedPointer<DrmBuffer> EglGbmLayer::testBuffer()
{
    if (!m_currentBuffer || !doesGbmSurfaceFit(m_gbmSurface.data())) {
        if (doesGbmSurfaceFit(m_oldGbmSurface.data())) {
            // re-use old surface and buffer without rendering
            m_gbmSurface = m_oldGbmSurface;
            if (m_gbmSurface->currentBuffer()) {
                m_currentBuffer = m_gbmSurface->currentDrmBuffer();
                return m_currentBuffer;
            }
        }
        if (!renderTestBuffer() && m_importMode == MultiGpuImportMode::DumbBufferXrgb8888) {
            // try multi-gpu import again, this time with DRM_FORMAT_XRGB8888
            renderTestBuffer();
        }
    }
    return m_currentBuffer;
}

bool EglGbmLayer::renderTestBuffer()
{
    if (!startRendering()) {
        return false;
    }
    glClear(GL_COLOR_BUFFER_BIT);
    if (!endRendering(m_displayDevice->renderGeometry())) {
        return false;
    }
    return true;
}

bool EglGbmLayer::createGbmSurface()
{
    static bool modifiersEnvSet = false;
    static const bool modifiersEnv = qEnvironmentVariableIntValue("KWIN_DRM_USE_MODIFIERS", &modifiersEnvSet) != 0;

    auto format = m_eglBackend->chooseFormat(m_displayDevice);
    if (!format || m_importMode == MultiGpuImportMode::DumbBufferXrgb8888) {
        format = DRM_FORMAT_XRGB8888;
    }
    const auto modifiers = m_displayDevice->supportedModifiers(format.value());
    const auto size = m_displayDevice->bufferSize();
    const auto config = m_eglBackend->config(format.value());
    const bool allowModifiers = m_eglBackend->gpu()->addFB2ModifiersSupported() && m_displayDevice->gpu()->addFB2ModifiersSupported()
                                && ((m_eglBackend->gpu()->isNVidia() && !modifiersEnvSet) || (modifiersEnvSet && modifiersEnv));

    QSharedPointer<GbmSurface> gbmSurface;
#if HAVE_GBM_BO_GET_FD_FOR_PLANE
    if (!allowModifiers) {
#else
    // modifiers have to be disabled with multi-gpu if gbm_bo_get_fd_for_plane is not available
    if (!allowModifiers || m_displayDevice->gpu() != m_eglBackend->gpu()) {
#endif
        int flags = GBM_BO_USE_RENDERING;
        if (m_displayDevice->gpu() == m_eglBackend->gpu()) {
            flags |= GBM_BO_USE_SCANOUT;
        } else {
            flags |= GBM_BO_USE_LINEAR;
        }
        gbmSurface = QSharedPointer<GbmSurface>::create(m_eglBackend->gpu(), size, format.value(), flags, config);
    } else {
        gbmSurface = QSharedPointer<GbmSurface>::create(m_eglBackend->gpu(), size, format.value(), modifiers, config);
        if (!gbmSurface->isValid()) {
            // the egl / gbm implementation may reject the modifier list from another gpu
            // as a fallback use linear, to at least make CPU copy more efficient
            const QVector<uint64_t> linear = {DRM_FORMAT_MOD_LINEAR};
            gbmSurface = QSharedPointer<GbmSurface>::create(m_eglBackend->gpu(), size, format.value(), linear, config);
        }
    }
    if (!gbmSurface->isValid()) {
        return false;
    }
    m_oldGbmSurface = m_gbmSurface;
    m_gbmSurface = gbmSurface;
    return true;
}

bool EglGbmLayer::doesGbmSurfaceFit(GbmSurface *surf) const
{
    return surf && surf->size() == m_displayDevice->bufferSize()
        && m_displayDevice->isFormatSupported(surf->format())
        && (m_importMode != MultiGpuImportMode::DumbBufferXrgb8888 || surf->format() == DRM_FORMAT_XRGB8888)
        && (surf->modifiers().isEmpty() || m_displayDevice->supportedModifiers(surf->format()) == surf->modifiers());
}

bool EglGbmLayer::doesShadowBufferFit(ShadowBuffer *buffer) const
{
    if (m_displayDevice->softwareTransforms() != DrmPlane::Transformations(DrmPlane::Transformation::Rotate0)) {
        return buffer && buffer->texture()->size() == m_displayDevice->sourceSize() && buffer->drmFormat() == m_gbmSurface->format();
    } else {
        return buffer == nullptr;
    }
}

bool EglGbmLayer::doesSwapchainFit(DumbSwapchain *swapchain) const
{
    return swapchain && swapchain->size() == m_displayDevice->sourceSize() && swapchain->drmFormat() == m_gbmSurface->format();
}

QSharedPointer<GLTexture> EglGbmLayer::texture() const
{
    const auto createImage = [this](GbmBuffer *gbmBuffer) {
        EGLImageKHR image = eglCreateImageKHR(m_eglBackend->eglDisplay(), nullptr, EGL_NATIVE_PIXMAP_KHR, gbmBuffer->getBo(), nullptr);
        if (image == EGL_NO_IMAGE_KHR) {
            qCWarning(KWIN_DRM) << "Failed to record frame: Error creating EGLImageKHR - " << getEglErrorString();
            return QSharedPointer<EGLImageTexture>(nullptr);
        }
        return QSharedPointer<EGLImageTexture>::create(m_eglBackend->eglDisplay(), image, GL_RGBA8, m_displayDevice->sourceSize());
    };
    if (m_scanoutBuffer) {
        return createImage(dynamic_cast<GbmBuffer*>(m_scanoutBuffer.data()));
    }
    if (m_shadowBuffer) {
        return m_shadowBuffer->texture();
    }
    GbmBuffer *gbmBuffer = m_gbmSurface->currentBuffer().data();
    if (!gbmBuffer) {
        qCWarning(KWIN_DRM) << "Failed to record frame: No gbm buffer!";
        return nullptr;
    }
    return createImage(gbmBuffer);
}

QSharedPointer<DrmBuffer> EglGbmLayer::importBuffer()
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

QSharedPointer<DrmBuffer> EglGbmLayer::importDmabuf()
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
        importedBuffer = gbm_bo_import(m_displayDevice->gpu()->gbmDevice(), GBM_BO_IMPORT_FD_MODIFIER, &data, GBM_BO_USE_SCANOUT);
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
        importedBuffer = gbm_bo_import(m_displayDevice->gpu()->gbmDevice(), GBM_BO_IMPORT_FD_MODIFIER, &data, GBM_BO_USE_SCANOUT);
#if HAVE_GBM_BO_GET_FD_FOR_PLANE
    }
#endif
    if (!importedBuffer) {
        qCWarning(KWIN_DRM, "failed to import gbm_bo for multi-gpu usage: %s", strerror(errno));
        return nullptr;
    }
    const auto buffer = QSharedPointer<DrmGbmBuffer>::create(m_displayDevice->gpu(), importedBuffer, nullptr);
    return buffer->bufferId() ? buffer : nullptr;
}

QSharedPointer<DrmBuffer> EglGbmLayer::importWithCpu()
{
    if (doesSwapchainFit(m_importSwapchain.data())) {
        m_oldImportSwapchain.reset();
    } else {
        if (doesSwapchainFit(m_oldImportSwapchain.data())) {
            m_importSwapchain = m_oldImportSwapchain;
        } else {
            const auto swapchain = QSharedPointer<DumbSwapchain>::create(m_displayDevice->gpu(), m_displayDevice->sourceSize(), m_gbmSurface->format());
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

bool EglGbmLayer::scanout(SurfaceItem *surfaceItem)
{
    static bool valid;
    static const bool directScanoutDisabled = qEnvironmentVariableIntValue("KWIN_DRM_NO_DIRECT_SCANOUT", &valid) == 1 && valid;
    if (directScanoutDisabled) {
        return false;
    }

    SurfaceItemWayland *item = qobject_cast<SurfaceItemWayland *>(surfaceItem);
    if (!item || !item->surface()) {
        return false;
    }
    const auto buffer = qobject_cast<KWaylandServer::LinuxDmaBufV1ClientBuffer *>(item->surface()->buffer());
    if (!buffer || buffer->planes().isEmpty() || buffer->size() != m_displayDevice->sourceSize()) {
        return false;
    }

    if (m_scanoutCandidate.surface && m_scanoutCandidate.surface != item->surface() && m_scanoutCandidate.surface->dmabufFeedbackV1()) {
        m_scanoutCandidate.surface->dmabufFeedbackV1()->setTranches({});
    }
    m_scanoutCandidate.surface = item->surface();
    m_scanoutCandidate.attemptedThisFrame = true;

    if (!m_displayDevice->isFormatSupported(buffer->format())) {
        sendDmabufFeedback(buffer);
        return false;
    }
    const auto planes = buffer->planes();
    gbm_bo *importedBuffer;
    if (planes.first().modifier != DRM_FORMAT_MOD_INVALID || planes.first().offset > 0 || planes.count() > 1) {
        if (!m_displayDevice->gpu()->addFB2ModifiersSupported() || !m_displayDevice->supportedModifiers(buffer->format()).contains(planes.first().modifier)) {
            return false;
        }
        gbm_import_fd_modifier_data data = {};
        data.format = buffer->format();
        data.width = (uint32_t) buffer->size().width();
        data.height = (uint32_t) buffer->size().height();
        data.num_fds = planes.count();
        data.modifier = planes.first().modifier;
        for (int i = 0; i < planes.count(); i++) {
            data.fds[i] = planes[i].fd;
            data.offsets[i] = planes[i].offset;
            data.strides[i] = planes[i].stride;
        }
        importedBuffer = gbm_bo_import(m_displayDevice->gpu()->gbmDevice(), GBM_BO_IMPORT_FD_MODIFIER, &data, GBM_BO_USE_SCANOUT);
    } else {
        const auto &plane = planes.first();
        gbm_import_fd_data data = {};
        data.fd = plane.fd;
        data.width = (uint32_t) buffer->size().width();
        data.height = (uint32_t) buffer->size().height();
        data.stride = plane.stride;
        data.format = buffer->format();
        importedBuffer = gbm_bo_import(m_displayDevice->gpu()->gbmDevice(), GBM_BO_IMPORT_FD, &data, GBM_BO_USE_SCANOUT);
    }
    if (!importedBuffer) {
        sendDmabufFeedback(buffer);
        if (errno != EINVAL) {
            qCWarning(KWIN_DRM) << "Importing buffer for direct scanout failed:" << strerror(errno);
        }
        return false;
    }
    m_scanoutBuffer = QSharedPointer<DrmGbmBuffer>::create(m_displayDevice->gpu(), importedBuffer, buffer);
    if (!m_scanoutBuffer->bufferId()) {
        // buffer can't actually be scanned out. Mesa is supposed to prevent this from happening
        // in gbm_bo_import but apparently that doesn't always work
        m_scanoutBuffer.reset();
        sendDmabufFeedback(buffer);
        return false;
    }
    // damage tracking for screen casting
    QRegion damage;
    if (m_scanoutCandidate.surface == item->surface()) {
        QRegion trackedDamage = surfaceItem->damage();
        surfaceItem->resetDamage();
        for (const auto &rect : trackedDamage) {
            auto damageRect = QRect(rect);
            damageRect.translate(m_displayDevice->renderGeometry().topLeft());
            damage |= damageRect;
        }
    } else {
        damage = m_displayDevice->renderGeometry();
    }
    if (m_displayDevice->testScanout()) {
        m_currentBuffer = m_scanoutBuffer;
        m_currentDamage = damage;
        return true;
    } else {
        m_scanoutBuffer.reset();
        return false;
    }
}

void EglGbmLayer::sendDmabufFeedback(KWaylandServer::LinuxDmaBufV1ClientBuffer *failedBuffer)
{
    if (!m_scanoutCandidate.attemptedFormats[failedBuffer->format()].contains(failedBuffer->planes().first().modifier)) {
        m_scanoutCandidate.attemptedFormats[failedBuffer->format()] << failedBuffer->planes().first().modifier;
    }
    if (const auto &drmOutput = dynamic_cast<DrmOutput *>(m_displayDevice); drmOutput && m_scanoutCandidate.surface->dmabufFeedbackV1()) {
        QVector<KWaylandServer::LinuxDmaBufV1Feedback::Tranche> scanoutTranches;
        const auto &drmFormats = drmOutput->pipeline()->supportedFormats();
        const auto tranches = m_eglBackend->dmabuf()->tranches();
        for (const auto &tranche : tranches) {
            KWaylandServer::LinuxDmaBufV1Feedback::Tranche scanoutTranche;
            for (auto it = tranche.formatTable.constBegin(); it != tranche.formatTable.constEnd(); it++) {
                const uint32_t format = it.key();
                const auto trancheModifiers = it.value();
                const auto drmModifiers = drmFormats[format];
                for (const auto &mod : trancheModifiers) {
                    if (drmModifiers.contains(mod) && !m_scanoutCandidate.attemptedFormats[format].contains(mod)) {
                        scanoutTranche.formatTable[format] << mod;
                    }
                }
            }
            if (!scanoutTranche.formatTable.isEmpty()) {
                scanoutTranche.device = m_displayDevice->gpu()->deviceId();
                scanoutTranche.flags = KWaylandServer::LinuxDmaBufV1Feedback::TrancheFlag::Scanout;
                scanoutTranches << scanoutTranche;
            }
        }
        m_scanoutCandidate.surface->dmabufFeedbackV1()->setTranches(scanoutTranches);
    }
}

QSharedPointer<DrmBuffer> EglGbmLayer::currentBuffer() const
{
    return m_scanoutBuffer ? m_scanoutBuffer : m_currentBuffer;
}

int EglGbmLayer::bufferAge() const
{
    return m_gbmSurface ? m_gbmSurface->bufferAge() : 0;
}

EGLSurface EglGbmLayer::eglSurface() const
{
    return m_gbmSurface ? m_gbmSurface->eglSurface() : EGL_NO_SURFACE;
}

bool EglGbmLayer::hasDirectScanoutBuffer() const
{
    return m_scanoutBuffer != nullptr;
}

}
