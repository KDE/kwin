/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2022 Xaver Hugl <xaver.hugl@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "drm_virtual_egl_layer.h"
#include "drm_abstract_output.h"
#include "drm_backend.h"
#include "drm_dumb_swapchain.h"
#include "drm_egl_backend.h"
#include "drm_gbm_swapchain.h"
#include "drm_gpu.h"
#include "drm_logging.h"
#include "drm_output.h"
#include "drm_pipeline.h"
#include "drm_virtual_output.h"
#include "egl_dmabuf.h"
#include "gbm_dmabuf.h"
#include "kwineglutils_p.h"
#include "scene/surfaceitem_wayland.h"
#include "wayland/linuxdmabufv1clientbuffer.h"
#include "wayland/surface_interface.h"

#include <QRegion>
#include <drm_fourcc.h>
#include <errno.h>
#include <gbm.h>
#include <unistd.h>

namespace KWin
{

VirtualEglGbmLayer::VirtualEglGbmLayer(EglGbmBackend *eglBackend, DrmVirtualOutput *output)
    : m_output(output)
    , m_eglBackend(eglBackend)
{
}

std::optional<OutputLayerBeginFrameInfo> VirtualEglGbmLayer::beginFrame()
{
    // gbm surface
    if (doesGbmSwapchainFit(m_gbmSwapchain.get())) {
        m_oldGbmSwapchain.reset();
    } else {
        if (doesGbmSwapchainFit(m_oldGbmSwapchain.get())) {
            m_gbmSwapchain = m_oldGbmSwapchain;
        } else {
            if (const auto swapchain = createGbmSwapchain()) {
                m_oldGbmSwapchain = m_gbmSwapchain;
                m_gbmSwapchain = swapchain;
            } else {
                return std::nullopt;
            }
        }
    }

    if (eglMakeCurrent(m_eglBackend->eglDisplay(), EGL_NO_SURFACE, EGL_NO_SURFACE, m_eglBackend->context()) != EGL_TRUE) {
        return std::nullopt;
    }
    const auto [buffer, repair] = m_gbmSwapchain->acquire();
    auto texture = m_eglBackend->importBufferObjectAsTexture(buffer->bo());
    if (!texture) {
        return std::nullopt;
    }
    texture->setContentTransform(TextureTransform::MirrorY);
    auto fbo = std::make_shared<GLFramebuffer>(texture.get());
    if (!fbo) {
        return std::nullopt;
    }
    m_currentBuffer = buffer;
    m_texture = texture;
    m_fbo = fbo;
    m_scanoutSurface.clear();
    return OutputLayerBeginFrameInfo{
        .renderTarget = RenderTarget(fbo.get()),
        .repaint = repair,
    };
}

bool VirtualEglGbmLayer::endFrame(const QRegion &renderedRegion, const QRegion &damagedRegion)
{
    glFlush();
    m_currentDamage = damagedRegion;
    m_gbmSwapchain->damage(damagedRegion);
    return true;
}

QRegion VirtualEglGbmLayer::currentDamage() const
{
    return m_currentDamage;
}

std::shared_ptr<GbmSwapchain> VirtualEglGbmLayer::createGbmSwapchain() const
{
    static bool modifiersEnvSet = false;
    static const bool modifiersEnv = qEnvironmentVariableIntValue("KWIN_DRM_USE_MODIFIERS", &modifiersEnvSet) != 0;
    const bool allowModifiers = !modifiersEnvSet || modifiersEnv;

    const auto tranches = m_eglBackend->dmabuf()->tranches();
    for (const auto &tranche : tranches) {
        for (auto it = tranche.formatTable.constBegin(); it != tranche.formatTable.constEnd(); it++) {
            const auto size = m_output->pixelSize();
            const auto format = it.key();
            const auto modifiers = it.value();

            if (allowModifiers && !modifiers.isEmpty()) {
                const auto ret = GbmSwapchain::createSwapchain(m_eglBackend->gpu(), size, format, modifiers);
                if (const auto surface = std::get_if<std::shared_ptr<GbmSwapchain>>(&ret)) {
                    return *surface;
                } else if (std::get<GbmSwapchain::Error>(ret) != GbmSwapchain::Error::ModifiersUnsupported) {
                    continue;
                }
            }
            const auto ret = GbmSwapchain::createSwapchain(m_eglBackend->gpu(), size, format, GBM_BO_USE_RENDERING);
            if (const auto surface = std::get_if<std::shared_ptr<GbmSwapchain>>(&ret)) {
                return *surface;
            }
        }
    }
    qCWarning(KWIN_DRM) << "couldn't create a gbm swapchain for a virtual output!";
    return nullptr;
}

bool VirtualEglGbmLayer::doesGbmSwapchainFit(GbmSwapchain *swapchain) const
{
    return swapchain && swapchain->size() == m_output->pixelSize();
}

std::shared_ptr<GLTexture> VirtualEglGbmLayer::texture() const
{
    if (m_scanoutSurface) {
        return m_eglBackend->importBufferObjectAsTexture(m_currentBuffer->bo());
    } else {
        return m_texture;
    }
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
    if (!buffer || buffer->size() != m_output->pixelSize()) {
        return false;
    }
    const auto scanoutBuffer = GbmBuffer::importBuffer(m_output->gpu(), buffer);
    if (!scanoutBuffer) {
        return false;
    }
    // damage tracking for screen casting
    m_currentDamage = m_scanoutSurface == item->surface() ? surfaceItem->damage() : infiniteRegion();
    surfaceItem->resetDamage();
    // ensure the pixmap is updated when direct scanout ends
    surfaceItem->destroyPixmap();
    m_scanoutSurface = item->surface();
    m_currentBuffer = scanoutBuffer;
    return true;
}

void VirtualEglGbmLayer::releaseBuffers()
{
    eglMakeCurrent(m_eglBackend->eglDisplay(), EGL_NO_SURFACE, EGL_NO_SURFACE, m_eglBackend->context());
    m_fbo.reset();
    m_texture.reset();
    m_gbmSwapchain.reset();
    m_oldGbmSwapchain.reset();
    m_currentBuffer.reset();
}

quint32 VirtualEglGbmLayer::format() const
{
    if (!m_gbmSwapchain) {
        return DRM_FORMAT_ARGB8888;
    }
    return m_gbmSwapchain->format();
}
}
