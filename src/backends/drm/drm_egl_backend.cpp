/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2015 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "drm_egl_backend.h"
#include "platformsupport/scenes/opengl/basiceglsurfacetexture_wayland.h"
// kwin
#include "drm_abstract_output.h"
#include "drm_backend.h"
#include "drm_egl_cursor_layer.h"
#include "drm_egl_layer.h"
#include "drm_gpu.h"
#include "drm_logging.h"
#include "drm_pipeline.h"
#include "drm_virtual_egl_layer.h"
#include "kwineglutils_p.h"
// system
#include <drm_fourcc.h>
#include <errno.h>
#include <gbm.h>
#include <unistd.h>

namespace KWin
{

EglGbmBackend::EglGbmBackend(DrmBackend *drmBackend)
    : AbstractEglBackend(drmBackend->primaryGpu()->deviceId())
    , m_backend(drmBackend)
{
    drmBackend->setRenderBackend(this);
    setIsDirectRendering(true);
    connect(m_backend, &DrmBackend::gpuRemoved, this, [this](DrmGpu *gpu) {
        m_contexts.erase(gpu->eglDisplay());
    });
}

EglGbmBackend::~EglGbmBackend()
{
    m_backend->releaseBuffers();
    m_contexts.clear();
    cleanup();
    m_backend->setRenderBackend(nullptr);
}

bool EglGbmBackend::initializeEgl()
{
    initClientExtensions();
    auto display = m_backend->primaryGpu()->eglDisplay();

    // Use eglGetPlatformDisplayEXT() to get the display pointer
    // if the implementation supports it.
    if (!display) {
        display = createEglDisplay(m_backend->primaryGpu());
        if (!display) {
            return false;
        }
    }
    setEglDisplay(display);
    return true;
}

EglDisplay *EglGbmBackend::createEglDisplay(DrmGpu *gpu) const
{
    if (!gpu->gbmDevice()) {
        return nullptr;
    }

    for (const QByteArray &extension : {QByteArrayLiteral("EGL_EXT_platform_base"), QByteArrayLiteral("EGL_KHR_platform_gbm")}) {
        if (!hasClientExtension(extension)) {
            qCWarning(KWIN_DRM) << extension << "client extension is not supported by the platform";
            return nullptr;
        }
    }

    gpu->setEglDisplay(EglDisplay::create(eglGetPlatformDisplayEXT(EGL_PLATFORM_GBM_KHR, gpu->gbmDevice(), nullptr)));
    return gpu->eglDisplay();
}

void EglGbmBackend::init()
{
    if (!initializeEgl()) {
        setFailed("Could not initialize egl");
        return;
    }

    if (!initRenderingContext()) {
        setFailed("Could not initialize rendering context");
        return;
    }
    initKWinGL();
    initWayland();
}

bool EglGbmBackend::initRenderingContext()
{
    return createContext(EGL_NO_CONFIG_KHR) && makeCurrent();
}

EglDisplay *EglGbmBackend::displayForGpu(DrmGpu *gpu)
{
    if (gpu == m_backend->primaryGpu()) {
        return eglDisplayObject();
    }
    auto display = gpu->eglDisplay();
    if (!display) {
        display = createEglDisplay(gpu);
    }
    return display;
}

EglContext *EglGbmBackend::contextForGpu(DrmGpu *gpu)
{
    if (gpu == m_backend->primaryGpu()) {
        return contextObject();
    }
    auto display = gpu->eglDisplay();
    if (!display) {
        display = createEglDisplay(gpu);
        if (!display) {
            return nullptr;
        }
    }
    auto &context = m_contexts[display];
    if (!context) {
        context = EglContext::create(display, EGL_NO_CONFIG_KHR, EGL_NO_CONTEXT);
    }
    return context.get();
}

std::unique_ptr<SurfaceTexture> EglGbmBackend::createSurfaceTextureWayland(SurfacePixmap *pixmap)
{
    return std::make_unique<BasicEGLSurfaceTextureWayland>(this, pixmap);
}

GraphicsBufferAllocator *EglGbmBackend::graphicsBufferAllocator() const
{
    return gpu()->graphicsBufferAllocator();
}

void EglGbmBackend::present(Output *output)
{
    static_cast<DrmAbstractOutput *>(output)->present();
}

OutputLayer *EglGbmBackend::primaryLayer(Output *output)
{
    return static_cast<DrmAbstractOutput *>(output)->primaryLayer();
}

OutputLayer *EglGbmBackend::cursorLayer(Output *output)
{
    return static_cast<DrmAbstractOutput *>(output)->cursorLayer();
}

std::pair<std::shared_ptr<KWin::GLTexture>, ColorDescription> EglGbmBackend::textureForOutput(Output *output) const
{
    const auto drmOutput = static_cast<DrmAbstractOutput *>(output);
    const auto layer = static_cast<EglGbmLayer *>(drmOutput->primaryLayer());
    return std::make_pair(layer->texture(), layer->colorDescription());
}

bool EglGbmBackend::prefer10bpc() const
{
    static bool ok = false;
    static const int preferred = qEnvironmentVariableIntValue("KWIN_DRM_PREFER_COLOR_DEPTH", &ok);
    return !ok || preferred == 30;
}

std::shared_ptr<DrmPipelineLayer> EglGbmBackend::createPrimaryLayer(DrmPipeline *pipeline)
{
    return std::make_shared<EglGbmLayer>(this, pipeline);
}

std::shared_ptr<DrmOverlayLayer> EglGbmBackend::createCursorLayer(DrmPipeline *pipeline)
{
    return std::make_shared<EglGbmCursorLayer>(this, pipeline);
}

std::shared_ptr<DrmOutputLayer> EglGbmBackend::createLayer(DrmVirtualOutput *output)
{
    return std::make_shared<VirtualEglGbmLayer>(this, output);
}

DrmGpu *EglGbmBackend::gpu() const
{
    return m_backend->primaryGpu();
}

} // namespace KWin

#include "moc_drm_egl_backend.cpp"
