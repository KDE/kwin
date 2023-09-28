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
        const bool hasMesaGBM = hasClientExtension(QByteArrayLiteral("EGL_MESA_platform_gbm"));
        const bool hasKHRGBM = hasClientExtension(QByteArrayLiteral("EGL_KHR_platform_gbm"));
        const GLenum platform = hasMesaGBM ? EGL_PLATFORM_GBM_MESA : EGL_PLATFORM_GBM_KHR;

        if (!hasClientExtension(QByteArrayLiteral("EGL_EXT_platform_base")) || (!hasMesaGBM && !hasKHRGBM)) {
            setFailed("Missing one or more extensions between EGL_EXT_platform_base, "
                      "EGL_MESA_platform_gbm, EGL_KHR_platform_gbm");
            return false;
        }

        if (!m_backend->primaryGpu()->gbmDevice()) {
            setFailed("Could not create gbm device");
            return false;
        }

        display = eglGetPlatformDisplayEXT(platform, m_backend->primaryGpu()->gbmDevice(), nullptr);
        m_backend->primaryGpu()->setEglDisplay(display);
    }

    if (display == EGL_NO_DISPLAY) {
        return false;
    }
    setEglDisplay(display);
    return initEglAPI();
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
    initBufferAge();
    initKWinGL();
    initWayland();
}

bool EglGbmBackend::initRenderingContext()
{
    if (!initBufferConfigs()) {
        return false;
    }
    if (!createContext() || !makeCurrent()) {
        return false;
    }
    return true;
}

bool EglGbmBackend::initBufferConfigs()
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
    if (!eglChooseConfig(eglDisplay(), config_attribs, configs,
                         sizeof(configs) / sizeof(EGLConfig),
                         &count)) {
        qCCritical(KWIN_DRM) << "eglChooseConfig failed:" << getEglErrorString();
        return false;
    }

    setConfig(EGL_NO_CONFIG_KHR);

    // Loop through all configs, choosing the first one that has suitable format.
    for (EGLint i = 0; i < count; i++) {
        EGLint gbmFormat;
        eglGetConfigAttrib(eglDisplay(), configs[i], EGL_NATIVE_VISUAL_ID, &gbmFormat);

        GbmFormat format;
        format.drmFormat = gbmFormat;
        EGLint red, green, blue;
        // Query number of bits for color channel
        eglGetConfigAttrib(eglDisplay(), configs[i], EGL_RED_SIZE, &red);
        eglGetConfigAttrib(eglDisplay(), configs[i], EGL_GREEN_SIZE, &green);
        eglGetConfigAttrib(eglDisplay(), configs[i], EGL_BLUE_SIZE, &blue);
        eglGetConfigAttrib(eglDisplay(), configs[i], EGL_ALPHA_SIZE, &format.alphaSize);
        format.bpp = red + green + blue;
        if (m_formats.contains(gbmFormat)) {
            continue;
        }
        m_formats[gbmFormat] = format;
        m_configs[format.drmFormat] = configs[i];
    }
    if (!m_formats.isEmpty()) {
        return true;
    }

    qCCritical(KWIN_DRM, "Choosing EGL config did not return a supported config. There were %u configs", count);
    for (EGLint i = 0; i < count; i++) {
        EGLint gbmFormat, blueSize, redSize, greenSize, alphaSize;
        eglGetConfigAttrib(eglDisplay(), configs[i], EGL_NATIVE_VISUAL_ID, &gbmFormat);
        eglGetConfigAttrib(eglDisplay(), configs[i], EGL_RED_SIZE, &redSize);
        eglGetConfigAttrib(eglDisplay(), configs[i], EGL_GREEN_SIZE, &greenSize);
        eglGetConfigAttrib(eglDisplay(), configs[i], EGL_BLUE_SIZE, &blueSize);
        eglGetConfigAttrib(eglDisplay(), configs[i], EGL_ALPHA_SIZE, &alphaSize);
        gbm_format_name_desc name;
        gbm_format_get_name(gbmFormat, &name);
        qCCritical(KWIN_DRM, "EGL config %d has format %s with %d,%d,%d,%d bits for r,g,b,a", i, name.name, redSize, greenSize, blueSize, alphaSize);
    }
    return false;
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
    if (const auto virtualLayer = dynamic_cast<VirtualEglGbmLayer *>(drmOutput->primaryLayer())) {
        return virtualLayer->texture();
    }
    return static_cast<EglGbmLayer *>(drmOutput->primaryLayer())->texture();
}

std::optional<GbmFormat> EglGbmBackend::gbmFormatForDrmFormat(uint32_t format) const
{
    // TODO use a hardcoded lookup table where needed instead?
    const auto it = m_formats.constFind(format);
    return it == m_formats.constEnd() ? std::optional<GbmFormat>() : *it;
}

bool EglGbmBackend::prefer10bpc() const
{
    static bool ok = false;
    static const int preferred = qEnvironmentVariableIntValue("KWIN_DRM_PREFER_COLOR_DEPTH", &ok);
    return !ok || preferred == 30;
}

EGLConfig EglGbmBackend::config(uint32_t format) const
{
    return m_configs.value(format, EGL_NO_CONFIG_KHR);
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

bool operator==(const GbmFormat &lhs, const GbmFormat &rhs)
{
    return lhs.drmFormat == rhs.drmFormat;
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

} // namespace KWin
