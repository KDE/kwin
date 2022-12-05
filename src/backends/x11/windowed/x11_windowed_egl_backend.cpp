/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2015 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "x11_windowed_egl_backend.h"
// kwin
#include "basiceglsurfacetexture_internal.h"
#include "basiceglsurfacetexture_wayland.h"
#include "softwarevsyncmonitor.h"
#include "x11_windowed_backend.h"
#include "x11_windowed_output.h"
// kwin libs
#include <kwinglplatform.h>

namespace KWin
{

X11WindowedEglOutput::X11WindowedEglOutput(X11WindowedEglBackend *backend, X11WindowedOutput *output, EGLSurface surface)
    : m_eglSurface(surface)
    , m_output(output)
    , m_backend(backend)
{
}

X11WindowedEglOutput::~X11WindowedEglOutput()
{
    eglDestroySurface(m_backend->eglDisplay(), m_eglSurface);
}

void X11WindowedEglOutput::ensureFbo()
{
    if (!m_fbo || m_fbo->size() != m_output->pixelSize()) {
        m_fbo = std::make_unique<GLFramebuffer>(0, m_output->pixelSize());
    }
}

std::optional<OutputLayerBeginFrameInfo> X11WindowedEglOutput::beginFrame()
{
    eglMakeCurrent(m_backend->eglDisplay(), m_eglSurface, m_eglSurface, m_backend->context());
    ensureFbo();
    GLFramebuffer::pushFramebuffer(m_fbo.get());

    QRegion repaint = m_output->exposedArea() + m_output->rect();
    m_output->clearExposedArea();

    return OutputLayerBeginFrameInfo{
        .renderTarget = RenderTarget(m_fbo.get()),
        .repaint = repaint,
    };
}

bool X11WindowedEglOutput::endFrame(const QRegion &renderedRegion, const QRegion &damagedRegion)
{
    m_lastDamage = damagedRegion;
    GLFramebuffer::popFramebuffer();
    return true;
}

EGLSurface X11WindowedEglOutput::surface() const
{
    return m_eglSurface;
}

QRegion X11WindowedEglOutput::lastDamage() const
{
    return m_lastDamage;
}

X11WindowedEglBackend::X11WindowedEglBackend(X11WindowedBackend *backend)
    : EglOnXBackend(backend->connection(), backend->display(), backend->rootWindow())
    , m_backend(backend)
{
}

X11WindowedEglBackend::~X11WindowedEglBackend()
{
    cleanup();
}

void X11WindowedEglBackend::init()
{
    EglOnXBackend::init();

    if (!isFailed()) {
        initWayland();
    }
}

void X11WindowedEglBackend::cleanupSurfaces()
{
    m_outputs.clear();
}

bool X11WindowedEglBackend::createSurfaces()
{
    const auto &outputs = m_backend->outputs();
    for (const auto &output : outputs) {
        EGLSurface s = createSurface(m_backend->windowForScreen(output));
        if (s == EGL_NO_SURFACE) {
            return false;
        }
        m_outputs[output] = std::make_shared<X11WindowedEglOutput>(this, static_cast<X11WindowedOutput *>(output), s);
    }
    if (m_outputs.isEmpty()) {
        return false;
    }
    return true;
}

void X11WindowedEglBackend::present(Output *output)
{
    static_cast<X11WindowedOutput *>(output)->vsyncMonitor()->arm();

    const auto &renderOutput = m_outputs[output];
    presentSurface(renderOutput->surface(), renderOutput->lastDamage(), output->geometry());
}

void X11WindowedEglBackend::presentSurface(EGLSurface surface, const QRegion &damage, const QRect &screenGeometry)
{
    if (damage.isEmpty()) {
        return;
    }
    const bool fullRepaint = supportsBufferAge() || (damage == screenGeometry);

    if (fullRepaint || !havePostSubBuffer()) {
        // the entire screen changed, or we cannot do partial updates (which implies we enabled surface preservation)
        eglSwapBuffers(eglDisplay(), surface);
    } else {
        // a part of the screen changed, and we can use eglPostSubBufferNV to copy the updated area
        for (const QRect &r : damage) {
            eglPostSubBufferNV(eglDisplay(), surface, r.left(), screenGeometry.height() - r.bottom() - 1, r.width(), r.height());
        }
    }
}

OutputLayer *X11WindowedEglBackend::primaryLayer(Output *output)
{
    return m_outputs[output].get();
}

OutputLayer *X11WindowedEglBackend::cursorLayer(Output *output)
{
    return nullptr;
}

std::unique_ptr<SurfaceTexture> X11WindowedEglBackend::createSurfaceTextureWayland(SurfacePixmapWayland *pixmap)
{
    return std::make_unique<BasicEGLSurfaceTextureWayland>(this, pixmap);
}

std::unique_ptr<SurfaceTexture> X11WindowedEglBackend::createSurfaceTextureInternal(SurfacePixmapInternal *pixmap)
{
    return std::make_unique<BasicEGLSurfaceTextureInternal>(this, pixmap);
}

} // namespace
