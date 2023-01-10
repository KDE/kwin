/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2015 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "drm_egl_backend.h"
#include "basiceglsurfacetexture_internal.h"
#include "basiceglsurfacetexture_wayland.h"
// kwin
#include "core/renderloop_p.h"
#include "drm_abstract_output.h"
#include "drm_backend.h"
#include "drm_buffer_gbm.h"
#include "drm_dumb_swapchain.h"
#include "drm_egl_cursor_layer.h"
#include "drm_egl_layer.h"
#include "drm_gbm_surface.h"
#include "drm_gpu.h"
#include "drm_logging.h"
#include "drm_output.h"
#include "drm_pipeline.h"
#include "drm_shadow_buffer.h"
#include "drm_virtual_egl_layer.h"
#include "egl_dmabuf.h"
#include "gbm_dmabuf.h"
#include "kwineglutils_p.h"
#include "linux_dmabuf.h"
#include "options.h"
#include "scene/surfaceitem_wayland.h"
#include "wayland/clientconnection.h"
#include "wayland/linuxdmabufv1clientbuffer.h"
#include "wayland/surface_interface.h"
// kwin libs
#include <kwineglimagetexture.h>
#include <kwinglplatform.h>
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

    const auto &gpus = drmBackend->gpus();
    for (const auto &gpu : gpus) {
        addGpu(gpu.get());
    }
    connect(drmBackend, &DrmBackend::gpuAdded, this, &EglGbmBackend::addGpu);
    connect(drmBackend, &DrmBackend::gpuRemoved, this, &EglGbmBackend::removeGpu);
}

EglGbmBackend::~EglGbmBackend()
{
    m_backend->releaseBuffers();
    cleanup();
    m_backend->setRenderBackend(nullptr);
}

bool EglGbmBackend::initializeEgl()
{
    initClientExtensions();
    EGLDisplay display = m_backend->primaryGpu()->eglDisplay();

    // Use eglGetPlatformDisplayEXT() to get the display pointer
    // if the implementation supports it.
    if (display == EGL_NO_DISPLAY) {
        display = createDisplay(m_backend->primaryGpu());
        m_backend->primaryGpu()->setEglDisplay(display);
    }

    if (display == EGL_NO_DISPLAY) {
        return false;
    }
    return initEglAPI(display);
}

EGLDisplay EglGbmBackend::createDisplay(DrmGpu *gpu) const
{
    const bool hasMesaGBM = hasClientExtension(QByteArrayLiteral("EGL_MESA_platform_gbm"));
    const bool hasKHRGBM = hasClientExtension(QByteArrayLiteral("EGL_KHR_platform_gbm"));
    const GLenum platform = hasMesaGBM ? EGL_PLATFORM_GBM_MESA : EGL_PLATFORM_GBM_KHR;

    if (!hasClientExtension(QByteArrayLiteral("EGL_EXT_platform_base")) || (!hasMesaGBM && !hasKHRGBM)) {
        qCWarning(KWIN_DRM, "Missing one or more extensions between EGL_EXT_platform_base, EGL_MESA_platform_gbm, EGL_KHR_platform_gbm");
        return EGL_NO_DISPLAY;
    }
    if (!m_backend->primaryGpu()->gbmDevice()) {
        qCWarning(KWIN_DRM, "Could not create gbm device");
        return EGL_NO_DISPLAY;
    }
    return eglGetPlatformDisplayEXT(platform, m_backend->primaryGpu()->gbmDevice(), nullptr);
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
    const auto configs = queryBufferConfigs(eglDisplay());
    if (configs.empty()) {
        return false;
    }
    m_configs[gpu()] = configs;
    if (!createContext(EGL_NO_CONFIG_KHR) || !makeCurrent()) {
        return false;
    }
    return true;
}

QHash<uint32_t, EGLConfig> EglGbmBackend::queryBufferConfigs(EGLDisplay display) const
{
    const EGLint config_attribs[] = {
        EGL_SURFACE_TYPE,
        EGL_WINDOW_BIT,
        EGL_RED_SIZE,
        1,
        EGL_GREEN_SIZE,
        1,
        EGL_BLUE_SIZE,
        1,
        EGL_ALPHA_SIZE,
        0,
        EGL_RENDERABLE_TYPE,
        isOpenGLES() ? EGL_OPENGL_ES2_BIT : EGL_OPENGL_BIT,
        EGL_CONFIG_CAVEAT,
        EGL_NONE,
        EGL_NONE,
    };

    EGLint count;
    EGLConfig configs[1024];
    if (!eglChooseConfig(display, config_attribs, configs,
                         sizeof(configs) / sizeof(EGLConfig),
                         &count)) {
        qCCritical(KWIN_DRM) << "eglChooseConfig failed:" << getEglErrorString();
        return {};
    }
    QHash<uint32_t, EGLConfig> configMap;
    for (EGLint i = 0; i < count; i++) {
        EGLint drmFormat;
        eglGetConfigAttrib(eglDisplay(), configs[i], EGL_NATIVE_VISUAL_ID, &drmFormat);
        configMap[drmFormat] = configs[i];
    }
    if (!configMap.empty()) {
        return configMap;
    }

    qCCritical(KWIN_DRM, "Choosing EGL config did not return a supported config. There were %u configs", count);
    for (EGLint i = 0; i < count; i++) {
        EGLint gbmFormat, blueSize, redSize, greenSize, alphaSize;
        eglGetConfigAttrib(display, configs[i], EGL_NATIVE_VISUAL_ID, &gbmFormat);
        eglGetConfigAttrib(display, configs[i], EGL_RED_SIZE, &redSize);
        eglGetConfigAttrib(display, configs[i], EGL_GREEN_SIZE, &greenSize);
        eglGetConfigAttrib(display, configs[i], EGL_BLUE_SIZE, &blueSize);
        eglGetConfigAttrib(display, configs[i], EGL_ALPHA_SIZE, &alphaSize);
        gbm_format_name_desc name;
        gbm_format_get_name(gbmFormat, &name);
        qCCritical(KWIN_DRM, "EGL config %d has format %s with %d,%d,%d,%d bits for r,g,b,a", i, name.name, redSize, greenSize, blueSize, alphaSize);
    }
    return {};
}

std::unique_ptr<SurfaceTexture> EglGbmBackend::createSurfaceTextureInternal(SurfacePixmapInternal *pixmap)
{
    return std::make_unique<BasicEGLSurfaceTextureInternal>(this, pixmap);
}

std::unique_ptr<SurfaceTexture> EglGbmBackend::createSurfaceTextureWayland(SurfacePixmapWayland *pixmap)
{
    return std::make_unique<BasicEGLSurfaceTextureWayland>(this, pixmap);
}

void EglGbmBackend::present(Output *output)
{
    static_cast<DrmAbstractOutput *>(output)->present();
}

OutputLayer *EglGbmBackend::primaryLayer(Output *output)
{
    return static_cast<DrmAbstractOutput *>(output)->primaryLayer();
}

std::shared_ptr<GLTexture> EglGbmBackend::textureForOutput(Output *output) const
{
    const auto drmOutput = static_cast<DrmAbstractOutput *>(output);
    return static_cast<EglGbmLayer *>(drmOutput->primaryLayer())->texture();
}

bool EglGbmBackend::prefer10bpc() const
{
    static bool ok = false;
    static const int preferred = qEnvironmentVariableIntValue("KWIN_DRM_PREFER_COLOR_DEPTH", &ok);
    return !ok || preferred == 30;
}

EGLConfig EglGbmBackend::config(DrmGpu *gpu, uint32_t format) const
{
    return m_configs[gpu].value(format, EGL_NO_CONFIG_KHR);
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

EGLImageKHR EglGbmBackend::importBufferObjectAsImage(gbm_bo *bo)
{
    return importDmaBufAsImage(dmaBufAttributesForBo(bo));
}

std::shared_ptr<GLTexture> EglGbmBackend::importBufferObjectAsTexture(gbm_bo *bo)
{
    EGLImageKHR image = importBufferObjectAsImage(bo);
    if (image != EGL_NO_IMAGE_KHR) {
        return std::make_shared<EGLImageTexture>(eglDisplay(), image, GL_RGBA8, QSize(gbm_bo_get_width(bo), gbm_bo_get_height(bo)));
    } else {
        qCWarning(KWIN_DRM) << "Failed to record frame: Error creating EGLImageKHR - " << getEglErrorString();
        return nullptr;
    }
}

void EglGbmBackend::addGpu(DrmGpu *gpu)
{
    EGLDisplay eglDisplay = createDisplay(gpu);
    gpu->setEglDisplay(eglDisplay);
    auto display = std::make_unique<KWinEglDisplay>();
    if (!display->init(eglDisplay)) {
        return;
    }
    auto context = std::make_unique<KWinEglContext>();
    if (!context->init(display.get(), EGL_NO_CONFIG_KHR, EGL_NO_CONTEXT)) {
        return;
    }
    const auto configs = queryBufferConfigs(eglDisplay);
    if (configs.empty()) {
        return;
    }
    m_displays[gpu] = std::move(display);
    m_contexts[gpu] = std::move(context);
    m_configs[gpu] = configs;
}

void EglGbmBackend::removeGpu(DrmGpu *gpu)
{
    m_contexts.erase(gpu);
    m_displays.erase(gpu);
    m_configs.remove(gpu);
}

KWinEglContext *EglGbmBackend::contextObject(DrmGpu *gpu)
{
    if (gpu == m_backend->primaryGpu()) {
        return AbstractEglBackend::contextObject();
    }
    const auto it = m_contexts.find(gpu);
    return it == m_contexts.end() ? nullptr : it->second.get();
}

} // namespace KWin
