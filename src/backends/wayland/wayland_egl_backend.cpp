/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2019 Roman Gilg <subdiff@gmail.com>
    SPDX-FileCopyrightText: 2013 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#define WL_EGL_PLATFORM 1

#include "wayland_egl_backend.h"
#include "basiceglsurfacetexture_internal.h"
#include "basiceglsurfacetexture_wayland.h"

#include "wayland_backend.h"
#include "wayland_display.h"
#include "wayland_logging.h"
#include "wayland_output.h"

#include <fcntl.h>
#include <unistd.h>

// kwin libs
#include <kwinglplatform.h>
#include <kwinglutils.h>

// KDE
#include <KWayland/Client/surface.h>

// Qt
#include <QFile>
#include <QOpenGLContext>

#include <cmath>
#include <drm_fourcc.h>
#include <gbm.h>

namespace KWin
{
namespace Wayland
{

static QVector<EGLint> regionToRects(const QRegion &region, Output *output)
{
    const int height = output->modeSize().height();
    const QMatrix4x4 matrix = WaylandOutput::logicalToNativeMatrix(output->rect(),
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

WaylandEglOutput::WaylandEglOutput(WaylandOutput *output, WaylandEglBackend *backend)
    : m_waylandOutput(output)
    , m_backend(backend)
{
}

bool WaylandEglOutput::init()
{
    auto surface = m_waylandOutput->surface();
    const QSize nativeSize = m_waylandOutput->geometry().size() * m_waylandOutput->scale();
    auto overlay = wl_egl_window_create(*surface, nativeSize.width(), nativeSize.height());
    if (!overlay) {
        qCCritical(KWIN_WAYLAND_BACKEND) << "Creating Wayland Egl window failed";
        return false;
    }
    m_overlay = overlay;
    m_fbo = std::make_unique<GLFramebuffer>(0, nativeSize);

    EGLSurface eglSurface = EGL_NO_SURFACE;
    if (m_backend->havePlatformBase()) {
        eglSurface = eglCreatePlatformWindowSurfaceEXT(m_backend->eglDisplay(), m_backend->config(), (void *)overlay, nullptr);
    } else {
        eglSurface = eglCreateWindowSurface(m_backend->eglDisplay(), m_backend->config(), overlay, nullptr);
    }
    if (eglSurface == EGL_NO_SURFACE) {
        qCCritical(KWIN_WAYLAND_BACKEND) << "Create Window Surface failed";
        return false;
    }
    m_eglSurface = eglSurface;

    return true;
}

WaylandEglOutput::~WaylandEglOutput()
{
    wl_egl_window_destroy(m_overlay);
}

GLFramebuffer *WaylandEglOutput::fbo() const
{
    return m_fbo.get();
}

bool WaylandEglOutput::makeContextCurrent() const
{
    if (m_eglSurface == EGL_NO_SURFACE) {
        return false;
    }
    if (eglMakeCurrent(m_backend->eglDisplay(), m_eglSurface, m_eglSurface, m_backend->context()) == EGL_FALSE) {
        qCCritical(KWIN_WAYLAND_BACKEND) << "Make Context Current failed";
        return false;
    }
    EGLint error = eglGetError();
    if (error != EGL_SUCCESS) {
        qCWarning(KWIN_WAYLAND_BACKEND) << "Error occurred while creating context " << error;
        return false;
    }
    return true;
}

std::optional<OutputLayerBeginFrameInfo> WaylandEglOutput::beginFrame()
{
    const QSize nativeSize = m_waylandOutput->pixelSize();
    if (!m_fbo || m_fbo->size() != nativeSize) {
        m_fbo = std::make_unique<GLFramebuffer>(0, nativeSize);
        m_bufferAge = 0;
        wl_egl_window_resize(m_overlay, nativeSize.width(), nativeSize.height(), 0, 0);
    }

    eglWaitNative(EGL_CORE_NATIVE_ENGINE);
    makeContextCurrent();

    QRegion repair;
    if (m_backend->supportsBufferAge()) {
        repair = m_damageJournal.accumulate(m_bufferAge, infiniteRegion());
    }

    GLFramebuffer::pushFramebuffer(m_fbo.get());
    return OutputLayerBeginFrameInfo{
        .renderTarget = RenderTarget(m_fbo.get()),
        .repaint = repair,
    };
}

bool WaylandEglOutput::endFrame(const QRegion &renderedRegion, const QRegion &damagedRegion)
{
    m_damageJournal.add(damagedRegion);
    GLFramebuffer::popFramebuffer();
    return true;
}

void WaylandEglOutput::aboutToStartPainting(const QRegion &damage)
{
    if (m_bufferAge > 0 && !damage.isEmpty() && m_backend->supportsPartialUpdate()) {
        QVector<EGLint> rects = regionToRects(damage, m_waylandOutput);
        const bool correct = eglSetDamageRegionKHR(m_backend->eglDisplay(), m_eglSurface,
                                                   rects.data(), rects.count() / 4);
        if (!correct) {
            qCWarning(KWIN_WAYLAND_BACKEND) << "failed eglSetDamageRegionKHR" << eglGetError();
        }
    }
}

void WaylandEglOutput::present()
{
    m_waylandOutput->surface()->setupFrameCallback();
    m_waylandOutput->surface()->setScale(std::ceil(m_waylandOutput->scale()));
    Q_EMIT m_waylandOutput->outputChange(m_damageJournal.lastDamage());

    if (m_backend->supportsSwapBuffersWithDamage()) {
        QVector<EGLint> rects = regionToRects(m_damageJournal.lastDamage(), m_waylandOutput);
        if (!eglSwapBuffersWithDamageEXT(m_backend->eglDisplay(), m_eglSurface,
                                         rects.data(), rects.count() / 4)) {
            qCCritical(KWIN_WAYLAND_BACKEND, "eglSwapBuffersWithDamage() failed: %x", eglGetError());
        }
    } else {
        if (!eglSwapBuffers(m_backend->eglDisplay(), m_eglSurface)) {
            qCCritical(KWIN_WAYLAND_BACKEND, "eglSwapBuffers() failed: %x", eglGetError());
        }
    }

    if (m_backend->supportsBufferAge()) {
        eglQuerySurface(m_backend->eglDisplay(), m_eglSurface, EGL_BUFFER_AGE_EXT, &m_bufferAge);
    }
}

WaylandEglBackend::WaylandEglBackend(WaylandBackend *b)
    : AbstractEglBackend()
    , m_backend(b)
{
    // Egl is always direct rendering
    setIsDirectRendering(true);

    connect(m_backend, &WaylandBackend::outputAdded, this, &WaylandEglBackend::createEglWaylandOutput);
    connect(m_backend, &WaylandBackend::outputRemoved, this, [this](Output *output) {
        auto it = std::find_if(m_outputs.begin(), m_outputs.end(), [output](const auto &o) {
            return o->m_waylandOutput == output;
        });
        if (it == m_outputs.end()) {
            return;
        }
        m_outputs.erase(it);
    });

    b->setEglBackend(this);
}

WaylandEglBackend::~WaylandEglBackend()
{
    cleanup();
}

void WaylandEglBackend::cleanupSurfaces()
{
    m_outputs.clear();
}

bool WaylandEglBackend::createEglWaylandOutput(Output *waylandOutput)
{
    const auto output = std::make_shared<WaylandEglOutput>(static_cast<WaylandOutput *>(waylandOutput), this);
    if (!output->init()) {
        return false;
    }
    m_outputs.insert(waylandOutput, output);
    return true;
}

bool WaylandEglBackend::initializeEgl()
{
    initClientExtensions();
    EGLDisplay display = m_backend->sceneEglDisplay();

    // Use eglGetPlatformDisplayEXT() to get the display pointer
    // if the implementation supports it.
    if (display == EGL_NO_DISPLAY) {
        m_havePlatformBase = hasClientExtension(QByteArrayLiteral("EGL_EXT_platform_base"));
        if (m_havePlatformBase) {
            // Make sure that the wayland platform is supported
            if (!hasClientExtension(QByteArrayLiteral("EGL_EXT_platform_wayland"))) {
                return false;
            }

            display = eglGetPlatformDisplayEXT(EGL_PLATFORM_WAYLAND_EXT, m_backend->display()->nativeDisplay(), nullptr);
        } else {
            display = eglGetDisplay(m_backend->display()->nativeDisplay());
        }
    }

    if (display == EGL_NO_DISPLAY) {
        return false;
    }
    setEglDisplay(display);
    return initEglAPI();
}

void WaylandEglBackend::init()
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
    initBufferAge();
    initWayland();
}

bool WaylandEglBackend::initRenderingContext()
{
    initBufferConfigs();

    if (!createContext()) {
        return false;
    }

    auto waylandOutputs = m_backend->waylandOutputs();

    // we only allow to start with at least one output
    if (waylandOutputs.isEmpty()) {
        return false;
    }

    for (auto *out : waylandOutputs) {
        if (!createEglWaylandOutput(out)) {
            return false;
        }
    }

    if (m_outputs.isEmpty()) {
        qCCritical(KWIN_WAYLAND_BACKEND) << "Create Window Surfaces failed";
        return false;
    }

    const auto &firstOutput = m_outputs.first();
    return firstOutput->makeContextCurrent();
}

bool WaylandEglBackend::initBufferConfigs()
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
    if (eglChooseConfig(eglDisplay(), config_attribs, configs, 1, &count) == EGL_FALSE) {
        qCCritical(KWIN_WAYLAND_BACKEND) << "choose config failed";
        return false;
    }
    if (count != 1) {
        qCCritical(KWIN_WAYLAND_BACKEND) << "choose config did not return a config" << count;
        return false;
    }
    setConfig(configs[0]);

    return true;
}

std::shared_ptr<KWin::GLTexture> WaylandEglBackend::textureForOutput(KWin::Output *output) const
{
    auto texture = std::make_unique<GLTexture>(GL_RGBA8, output->pixelSize());
    GLFramebuffer::pushFramebuffer(m_outputs[output]->fbo());
    GLFramebuffer renderTarget(texture.get());
    renderTarget.blitFromFramebuffer(QRect(0, texture->height(), texture->width(), -texture->height()));
    GLFramebuffer::popFramebuffer();
    return texture;
}

std::unique_ptr<SurfaceTexture> WaylandEglBackend::createSurfaceTextureInternal(SurfacePixmapInternal *pixmap)
{
    return std::make_unique<BasicEGLSurfaceTextureInternal>(this, pixmap);
}

std::unique_ptr<SurfaceTexture> WaylandEglBackend::createSurfaceTextureWayland(SurfacePixmapWayland *pixmap)
{
    return std::make_unique<BasicEGLSurfaceTextureWayland>(this, pixmap);
}

void WaylandEglBackend::present(Output *output)
{
    m_outputs[output]->present();
}

OutputLayer *WaylandEglBackend::primaryLayer(Output *output)
{
    return m_outputs[output].get();
}

}
}
