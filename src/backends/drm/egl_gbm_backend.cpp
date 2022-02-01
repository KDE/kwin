/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2015 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "egl_gbm_backend.h"
#include "basiceglsurfacetexture_internal.h"
#include "basiceglsurfacetexture_wayland.h"
// kwin
#include "composite.h"
#include "drm_backend.h"
#include "drm_buffer_gbm.h"
#include "drm_output.h"
#include "gbm_surface.h"
#include "logging.h"
#include "options.h"
#include "renderloop_p.h"
#include "screens.h"
#include "surfaceitem_wayland.h"
#include "drm_gpu.h"
#include "linux_dmabuf.h"
#include "dumb_swapchain.h"
#include "kwineglutils_p.h"
#include "shadowbuffer.h"
#include "drm_pipeline.h"
#include "drm_abstract_output.h"
#include "egl_dmabuf.h"
#include "egl_gbm_layer.h"
// kwin libs
#include <kwinglplatform.h>
#include <kwineglimagetexture.h>
// system
#include <gbm.h>
#include <unistd.h>
#include <errno.h>
#include <drm_fourcc.h>
// kwayland server
#include "KWaylandServer/surface_interface.h"
#include "KWaylandServer/linuxdmabufv1clientbuffer.h"
#include "KWaylandServer/clientconnection.h"

namespace KWin
{

EglGbmBackend::EglGbmBackend(DrmBackend *drmBackend)
    : AbstractEglBackend(drmBackend->primaryGpu()->deviceId())
    , m_backend(drmBackend)
{
    drmBackend->primaryGpu()->setEglBackend(this);
    connect(m_backend, &DrmBackend::outputEnabled, this, &EglGbmBackend::addOutput);
    connect(m_backend, &DrmBackend::outputDisabled, this, &EglGbmBackend::removeOutput);
    setIsDirectRendering(true);
}

EglGbmBackend::~EglGbmBackend()
{
    cleanup();
}

void EglGbmBackend::cleanupSurfaces()
{
    m_surfaces.clear();
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

        if (!hasClientExtension(QByteArrayLiteral("EGL_EXT_platform_base")) ||
                (!hasMesaGBM && !hasKHRGBM)) {
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
    const auto outputs = m_backend->outputs();
    for (const auto &output : outputs) {
        addOutput(output);
    }
    return true;
}

void EglGbmBackend::addOutput(AbstractOutput *output) {
    auto drmOutput = static_cast<DrmAbstractOutput*>(output);
    m_surfaces.insert(drmOutput, QSharedPointer<EglGbmLayer>::create(m_backend->primaryGpu(), drmOutput));
}

void EglGbmBackend::removeOutput(AbstractOutput *output)
{
    m_surfaces.remove(output);
}

bool EglGbmBackend::initBufferConfigs()
{
    const EGLint config_attribs[] = {
        EGL_SURFACE_TYPE,         EGL_WINDOW_BIT,
        EGL_RED_SIZE,             1,
        EGL_GREEN_SIZE,           1,
        EGL_BLUE_SIZE,            1,
        EGL_ALPHA_SIZE,           0,
        EGL_RENDERABLE_TYPE,      isOpenGLES() ? EGL_OPENGL_ES2_BIT : EGL_OPENGL_BIT,
        EGL_CONFIG_CAVEAT,        EGL_NONE,
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
        // Query number of bits for color channel
        eglGetConfigAttrib(eglDisplay(), configs[i], EGL_RED_SIZE, &format.redSize);
        eglGetConfigAttrib(eglDisplay(), configs[i], EGL_GREEN_SIZE, &format.greenSize);
        eglGetConfigAttrib(eglDisplay(), configs[i], EGL_BLUE_SIZE, &format.blueSize);
        eglGetConfigAttrib(eglDisplay(), configs[i], EGL_ALPHA_SIZE, &format.alphaSize);

        if (m_formats.contains(format)) {
            continue;
        }
        m_formats << format;
        m_configs[format.drmFormat] = configs[i];
    }

    QVector<int> colorDepthOrder = {30, 24};
    bool ok = false;
    const int preferred = qEnvironmentVariableIntValue("KWIN_DRM_PREFER_COLOR_DEPTH", &ok);
    if (ok) {
        colorDepthOrder.prepend(preferred);
    }

    std::sort(m_formats.begin(), m_formats.end(), [&colorDepthOrder](const auto &lhs, const auto &rhs) {
        const int ls = lhs.redSize + lhs.greenSize + lhs.blueSize;
        const int rs = rhs.redSize + rhs.greenSize + rhs.blueSize;
        if (ls == rs) {
            return lhs.alphaSize < rhs.alphaSize;
        } else {
            for (const int &d : qAsConst(colorDepthOrder)) {
                if (ls == d) {
                    return true;
                } else if (rs == d) {
                    return false;
                }
            }
            return ls > rs;
        }
    });
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
        qCCritical(KWIN_DRM, "EGL config %d has format %s with %d,%d,%d,%d bits for r,g,b,a",  i, name.name, redSize, greenSize, blueSize, alphaSize);
    }
    return false;
}

static QVector<EGLint> regionToRects(const QRegion &region, DrmAbstractOutput *output)
{
    const int height = output->sourceSize().height();

    const QMatrix4x4 matrix = DrmOutput::logicalToNativeMatrix(output->geometry(),
                                                               output->scale(),
                                                               output->transform());

    QVector<EGLint> rects;
    rects.reserve(region.rectCount() * 4);
    for (const QRect &_rect : region) {
        const QRect rect = matrix.mapRect(_rect);

        rects << rect.left();
        rects << height - (rect.y() + rect.height());
        rects << rect.width();
        rects << rect.height();
    }
    return rects;
}

void EglGbmBackend::aboutToStartPainting(AbstractOutput *output, const QRegion &damagedRegion)
{
    Q_ASSERT_X(output, "aboutToStartPainting", "not using per screen rendering");
    Q_ASSERT(m_surfaces.contains(output));
    const auto &surface = m_surfaces[output];
    if (surface->bufferAge() > 0 && !damagedRegion.isEmpty() && supportsPartialUpdate()) {
        const QRegion region = damagedRegion & output->geometry();

        QVector<EGLint> rects = regionToRects(region, static_cast<DrmAbstractOutput*>(output));
        const bool correct = eglSetDamageRegionKHR(eglDisplay(), surface->eglSurface(), rects.data(), rects.count()/4);
        if (!correct) {
            qCWarning(KWIN_DRM) << "eglSetDamageRegionKHR failed:" << getEglErrorString();
        }
    }
}

SurfaceTexture *EglGbmBackend::createSurfaceTextureInternal(SurfacePixmapInternal *pixmap)
{
    return new BasicEGLSurfaceTextureInternal(this, pixmap);
}

SurfaceTexture *EglGbmBackend::createSurfaceTextureWayland(SurfacePixmapWayland *pixmap)
{
    return new BasicEGLSurfaceTextureWayland(this, pixmap);
}

QRegion EglGbmBackend::beginFrame(AbstractOutput *output)
{
    Q_ASSERT(m_surfaces.contains(output));
    return m_surfaces[output]->startRendering().value_or(QRegion());
}

void EglGbmBackend::endFrame(AbstractOutput *output, const QRegion &renderedRegion,
                             const QRegion &damagedRegion)
{
    Q_ASSERT(m_surfaces.contains(output));
    Q_UNUSED(renderedRegion)

    m_surfaces[output]->endRendering(damagedRegion);
    static_cast<DrmAbstractOutput*>(output)->present(m_surfaces[output]->currentBuffer(), damagedRegion & output->geometry());
}

bool EglGbmBackend::scanout(AbstractOutput *output, SurfaceItem *surfaceItem)
{
    return m_surfaces[output]->scanout(surfaceItem);
}

QSharedPointer<DrmBuffer> EglGbmBackend::testBuffer(DrmAbstractOutput *output)
{
    return m_surfaces[output]->testBuffer();
}

QSharedPointer<GLTexture> EglGbmBackend::textureForOutput(AbstractOutput *output) const
{
    Q_ASSERT(m_surfaces.contains(output));
    return m_surfaces[output]->texture();
}

bool EglGbmBackend::directScanoutAllowed(AbstractOutput *output) const
{
    return !output->usesSoftwareCursor() && !output->directScanoutInhibited();
}

GbmFormat EglGbmBackend::gbmFormatForDrmFormat(uint32_t format) const
{
    // TODO use a hardcoded lookup table where needed instead?
    const auto it = std::find_if(m_formats.begin(), m_formats.end(), [format](const auto &gbmFormat) {
        return gbmFormat.drmFormat == format;
    });
    if (it == m_formats.end()) {
        return GbmFormat {
            .drmFormat = DRM_FORMAT_XRGB8888,
            .redSize = 8,
            .greenSize = 8,
            .blueSize = 8,
            .alphaSize = 0,
        };
    } else {
        return *it;
    }
}

std::optional<uint32_t> EglGbmBackend::chooseFormat(DrmAbstractOutput *output) const
{
    // formats are already sorted by order of preference
    std::optional<uint32_t> fallback;
    for (const auto &format : qAsConst(m_formats)) {
        if (output->isFormatSupported(format.drmFormat)) {
            int bpc = std::max(format.redSize, std::max(format.greenSize, format.blueSize));
            if (bpc <= output->maxBpc() && !fallback.has_value()) {
                fallback = format.drmFormat;
            } else {
                return format.drmFormat;
            }
        }
    }
    return fallback;
}

bool EglGbmBackend::prefer10bpc() const
{
    static bool ok = false;
    static const int preferred = qEnvironmentVariableIntValue("KWIN_DRM_PREFER_COLOR_DEPTH", &ok);
    return !ok || preferred == 30;
}

EGLConfig EglGbmBackend::config(uint32_t format) const
{
    return m_configs[format];
}

bool operator==(const GbmFormat &lhs, const GbmFormat &rhs)
{
    return lhs.drmFormat == rhs.drmFormat;
}

}
