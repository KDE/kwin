/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2015 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "egl_x11_backend.h"
// kwin
#include "basiceglsurfacetexture_internal.h"
#include "basiceglsurfacetexture_wayland.h"
#include "main.h"
#include "screens.h"
#include "softwarevsyncmonitor.h"
#include "x11windowed_backend.h"
#include "x11windowed_output.h"
// kwin libs
#include <kwinglplatform.h>

namespace KWin
{

EglX11Output::EglX11Output(EglX11Backend *backend, Output *output, EGLSurface surface)
    : m_eglSurface(surface)
    , m_fbo(new GLFramebuffer(0, output->pixelSize()))
    , m_output(output)
    , m_backend(backend)
{
}

EglX11Output::~EglX11Output()
{
    eglDestroySurface(m_backend->eglDisplay(), m_eglSurface);
}

OutputLayerBeginFrameInfo EglX11Output::beginFrame()
{
    eglMakeCurrent(m_backend->eglDisplay(), m_eglSurface, m_eglSurface, m_backend->context());
    GLFramebuffer::pushFramebuffer(m_fbo.data());
    return OutputLayerBeginFrameInfo{
        .renderTarget = RenderTarget(m_fbo.data()),
        .repaint = m_output->rect(),
    };
}

bool EglX11Output::endFrame(const QRegion &renderedRegion, const QRegion &damagedRegion)
{
    Q_UNUSED(renderedRegion)
    m_lastDamage = damagedRegion;
    GLFramebuffer::popFramebuffer();
    return true;
}

EGLSurface EglX11Output::surface() const
{
    return m_eglSurface;
}

QRegion EglX11Output::lastDamage() const
{
    return m_lastDamage;
}

EglX11Backend::EglX11Backend(X11WindowedBackend *backend)
    : EglOnXBackend(backend->connection(), backend->display(), backend->rootWindow(), backend->screenNumer(), XCB_WINDOW_NONE)
    , m_backend(backend)
{
}

EglX11Backend::~EglX11Backend() = default;

void EglX11Backend::init()
{
    EglOnXBackend::init();

    if (!isFailed()) {
        initWayland();
    }
}

void EglX11Backend::cleanupSurfaces()
{
    m_outputs.clear();
}

bool EglX11Backend::createSurfaces()
{
    const auto &outputs = m_backend->outputs();
    for (const auto &output : outputs) {
        EGLSurface s = createSurface(m_backend->windowForScreen(output));
        if (s == EGL_NO_SURFACE) {
            return false;
        }
        m_outputs[output] = QSharedPointer<EglX11Output>::create(this, output, s);
    }
    if (m_outputs.isEmpty()) {
        return false;
    }
    setSurface(m_outputs.first()->surface());
    return true;
}

void EglX11Backend::present(Output *output)
{
    static_cast<X11WindowedOutput *>(output)->vsyncMonitor()->arm();

    const auto &renderOutput = m_outputs[output];
    presentSurface(renderOutput->surface(), renderOutput->lastDamage(), output->geometry());
}

void EglX11Backend::presentSurface(EGLSurface surface, const QRegion &damage, const QRect &screenGeometry)
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

OutputLayer *EglX11Backend::primaryLayer(Output *output)
{
    return m_outputs[output].get();
}

SurfaceTexture *EglX11Backend::createSurfaceTextureWayland(SurfacePixmapWayland *pixmap)
{
    return new BasicEGLSurfaceTextureWayland(this, pixmap);
}

SurfaceTexture *EglX11Backend::createSurfaceTextureInternal(SurfacePixmapInternal *pixmap)
{
    return new BasicEGLSurfaceTextureInternal(this, pixmap);
}

} // namespace
