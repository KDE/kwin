/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2022 Xaver Hugl <xaver.hugl@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "virtual_egl_gbm_layer.h"
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
#include "drm_virtual_output.h"
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

VirtualEglGbmLayer::VirtualEglGbmLayer(EglGbmBackend *eglBackend, DrmVirtualOutput *output)
    : m_output(output)
    , m_eglBackend(eglBackend)
{
    connect(eglBackend, &EglGbmBackend::aboutToBeDestroyed, this, &VirtualEglGbmLayer::destroyResources);
}

void VirtualEglGbmLayer::destroyResources()
{
    m_gbmSurface.reset();
    m_oldGbmSurface.reset();
}

void VirtualEglGbmLayer::aboutToStartPainting(const QRegion &damagedRegion)
{
    if (m_gbmSurface && m_gbmSurface->bufferAge() > 0 && !damagedRegion.isEmpty() && m_eglBackend->supportsPartialUpdate()) {
        const QRegion region = damagedRegion & m_output->geometry();

        QVector<EGLint> rects = m_output->regionToRects(region);
        const bool correct = eglSetDamageRegionKHR(m_eglBackend->eglDisplay(), m_gbmSurface->eglSurface(), rects.data(), rects.count() / 4);
        if (!correct) {
            qCWarning(KWIN_DRM) << "eglSetDamageRegionKHR failed:" << getEglErrorString();
        }
    }
}

std::optional<QRegion> VirtualEglGbmLayer::startRendering()
{
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
        }
    }
    if (!m_gbmSurface->makeContextCurrent()) {
        return std::optional<QRegion>();
    }
    GLRenderTarget::pushRenderTarget(m_gbmSurface->renderTarget());
    return m_gbmSurface->repaintRegion(m_output->geometry());
}

bool VirtualEglGbmLayer::endRendering(const QRegion &damagedRegion)
{
    GLRenderTarget::popRenderTarget();
    const auto buffer = m_gbmSurface->swapBuffers(damagedRegion.intersected(m_output->geometry()));
    if (buffer) {
        m_currentBuffer = buffer;
        m_currentDamage = damagedRegion;
    }
    return buffer;
}

QRegion VirtualEglGbmLayer::currentDamage() const
{
    return m_currentDamage;
}

bool VirtualEglGbmLayer::createGbmSurface()
{
    static bool modifiersEnvSet = false;
    static const bool modifiersEnv = qEnvironmentVariableIntValue("KWIN_DRM_USE_MODIFIERS", &modifiersEnvSet) != 0;

    const auto tranches = m_eglBackend->dmabuf()->tranches();
    for (const auto &tranche : tranches) {
        for (auto it = tranche.formatTable.constBegin(); it != tranche.formatTable.constEnd(); it++) {
            const auto size = m_output->pixelSize();
            const auto config = m_eglBackend->config(it.key());
            const auto modifiers = it.value();
            const bool allowModifiers = m_eglBackend->gpu()->addFB2ModifiersSupported() && ((m_eglBackend->gpu()->isNVidia() && !modifiersEnvSet) || (modifiersEnvSet && modifiersEnv));

            QSharedPointer<GbmSurface> gbmSurface;
            if (!allowModifiers) {
                gbmSurface = QSharedPointer<GbmSurface>::create(m_eglBackend->gpu(), size, it.key(), GBM_BO_USE_RENDERING, config);
            } else {
                gbmSurface = QSharedPointer<GbmSurface>::create(m_eglBackend->gpu(), size, it.key(), it.value(), config);
            }
            if (!gbmSurface->isValid()) {
                continue;
            }
            m_oldGbmSurface = m_gbmSurface;
            m_gbmSurface = gbmSurface;
            return true;
        }
    }
    return false;
}

bool VirtualEglGbmLayer::doesGbmSurfaceFit(GbmSurface *surf) const
{
    return surf && surf->size() == m_output->pixelSize();
}

QSharedPointer<GLTexture> VirtualEglGbmLayer::texture() const
{
    GbmBuffer *gbmBuffer = m_gbmSurface->currentBuffer().get();
    if (!gbmBuffer) {
        qCWarning(KWIN_DRM) << "Failed to record frame: No gbm buffer!";
        return nullptr;
    }
    EGLImageKHR image = eglCreateImageKHR(m_eglBackend->eglDisplay(), nullptr, EGL_NATIVE_PIXMAP_KHR, gbmBuffer->getBo(), nullptr);
    if (image == EGL_NO_IMAGE_KHR) {
        qCWarning(KWIN_DRM) << "Failed to record frame: Error creating EGLImageKHR - " << glGetError();
        return nullptr;
    }
    return QSharedPointer<EGLImageTexture>::create(m_eglBackend->eglDisplay(), image, GL_RGBA8, m_gbmSurface->size());
}

bool VirtualEglGbmLayer::scanout(SurfaceItem *surfaceItem)
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
    if (!buffer || buffer->planes().isEmpty() || buffer->size() != m_output->pixelSize()) {
        return false;
    }
    const auto scanoutBuffer = QSharedPointer<GbmBuffer>::create(m_output->gpu(), buffer);
    if (!scanoutBuffer->getBo()) {
        return false;
    }
    // damage tracking for screen casting
    QRegion damage;
    if (m_scanoutSurface == item->surface()) {
        QRegion trackedDamage = surfaceItem->damage();
        surfaceItem->resetDamage();
        for (const auto &rect : trackedDamage) {
            auto damageRect = QRect(rect);
            damageRect.translate(m_output->geometry().topLeft());
            damage |= damageRect;
        }
    } else {
        damage = m_output->geometry();
    }
    m_scanoutSurface = item->surface();
    m_currentBuffer = scanoutBuffer;
    m_currentDamage = damage;
    return true;
}

}
