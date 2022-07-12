/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2015 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "egl_gbm_backend.h"
// kwin
#include "basiceglsurfacetexture_internal.h"
#include "basiceglsurfacetexture_wayland.h"
#include "composite.h"
#include "options.h"
#include "softwarevsyncmonitor.h"
#include "virtual_backend.h"
#include "virtual_output.h"
#include <logging.h>
// kwin libs
#include <kwinglplatform.h>
#include <kwinglutils.h>
// Qt
#include <QOpenGLContext>

#ifndef EGL_PLATFORM_SURFACELESS_MESA
#define EGL_PLATFORM_SURFACELESS_MESA 0x31DD
#endif

namespace KWin
{

VirtualOutputLayer::VirtualOutputLayer(Output *output, EglGbmBackend *backend)
    : m_backend(backend)
    , m_output(output)
{
}

VirtualOutputLayer::~VirtualOutputLayer() = default;

GLTexture *VirtualOutputLayer::texture() const
{
    return m_texture.get();
}

OutputLayerBeginFrameInfo VirtualOutputLayer::beginFrame()
{
    m_backend->makeCurrent();

    const QSize nativeSize = m_output->geometry().size() * m_output->scale();
    if (!m_texture || m_texture->size() != nativeSize) {
        m_fbo.reset();
        m_texture = std::make_unique<GLTexture>(GL_RGB8, nativeSize);
        m_fbo = std::make_unique<GLFramebuffer>(m_texture.get());
    }

    GLFramebuffer::pushFramebuffer(m_fbo.get());
    return OutputLayerBeginFrameInfo{
        .renderTarget = RenderTarget(m_fbo.get()),
        .repaint = infiniteRegion(),
    };
}

bool VirtualOutputLayer::endFrame(const QRegion &renderedRegion, const QRegion &damagedRegion)
{
    Q_UNUSED(renderedRegion)
    Q_UNUSED(damagedRegion)
    GLFramebuffer::popFramebuffer();
    return true;
}

EglGbmBackend::EglGbmBackend(VirtualBackend *b)
    : AbstractEglBackend()
    , m_backend(b)
{
    // Egl is always direct rendering
    setIsDirectRendering(true);
}

EglGbmBackend::~EglGbmBackend()
{
    m_outputs.clear();
    cleanup();
}

bool EglGbmBackend::initializeEgl()
{
    initClientExtensions();
    EGLDisplay display = m_backend->sceneEglDisplay();

    // Use eglGetPlatformDisplayEXT() to get the display pointer
    // if the implementation supports it.
    if (display == EGL_NO_DISPLAY) {
        // first try surfaceless
        if (hasClientExtension(QByteArrayLiteral("EGL_MESA_platform_surfaceless"))) {
            display = eglGetPlatformDisplayEXT(EGL_PLATFORM_SURFACELESS_MESA, EGL_DEFAULT_DISPLAY, nullptr);
        } else {
            qCWarning(KWIN_VIRTUAL) << "Extension EGL_MESA_platform_surfaceless not available";
        }
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

    initKWinGL();
    if (checkGLError("Init")) {
        setFailed("Error during init of EglGbmBackend");
        return;
    }

    setSupportsBufferAge(false);
    initWayland();

    const auto outputs = m_backend->enabledOutputs();
    for (Output *output : outputs) {
        addOutput(output);
    }

    connect(m_backend, &VirtualBackend::outputEnabled, this, &EglGbmBackend::addOutput);
    connect(m_backend, &VirtualBackend::outputDisabled, this, &EglGbmBackend::removeOutput);
}

bool EglGbmBackend::initRenderingContext()
{
    initBufferConfigs();

    if (!createContext()) {
        return false;
    }

    return makeCurrent();
}

void EglGbmBackend::addOutput(Output *output)
{
    makeCurrent();
    m_outputs[output] = std::make_unique<VirtualOutputLayer>(output, this);
}

void EglGbmBackend::removeOutput(Output *output)
{
    makeCurrent();
    m_outputs.erase(output);
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
    if (eglChooseConfig(eglDisplay(), config_attribs, configs, 1, &count) == EGL_FALSE) {
        return false;
    }
    if (count != 1) {
        return false;
    }
    setConfig(configs[0]);

    return true;
}

std::unique_ptr<SurfaceTexture> EglGbmBackend::createSurfaceTextureInternal(SurfacePixmapInternal *pixmap)
{
    return std::make_unique<BasicEGLSurfaceTextureInternal>(this, pixmap);
}

std::unique_ptr<SurfaceTexture> EglGbmBackend::createSurfaceTextureWayland(SurfacePixmapWayland *pixmap)
{
    return std::make_unique<BasicEGLSurfaceTextureWayland>(this, pixmap);
}

OutputLayer *EglGbmBackend::primaryLayer(Output *output)
{
    return m_outputs[output].get();
}

void EglGbmBackend::present(Output *output)
{
    glFlush();

    static_cast<VirtualOutput *>(output)->vsyncMonitor()->arm();

    if (m_backend->saveFrames()) {
        const std::unique_ptr<VirtualOutputLayer> &layer = m_outputs[output];
        layer->texture()->toImage().save(QStringLiteral("%1/%2.png").arg(m_backend->saveFrames()).arg(QString::number(m_frameCounter++)));
    }

    eglSwapBuffers(eglDisplay(), surface());
}

} // namespace
