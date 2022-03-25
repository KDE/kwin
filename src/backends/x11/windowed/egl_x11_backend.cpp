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
#include "renderoutput.h"
#include "screens.h"
#include "softwarevsyncmonitor.h"
#include "x11windowed_backend.h"
#include "x11windowed_output.h"
// kwin libs
#include <kwinglplatform.h>

namespace KWin
{

EglX11Output::EglX11Output(AbstractOutput *output, EglX11Backend *backend, EGLSurface surface)
    : m_backend(backend)
    , m_output(output)
    , m_eglSurface(surface)
    , m_renderTarget(new GLRenderTarget(0, output->pixelSize()))
{
}

EglX11Output::~EglX11Output()
{
    eglDestroySurface(m_backend->eglDisplay(), m_eglSurface);
}

std::optional<QRegion> EglX11Output::beginFrame(const QRect &geometry)
{
    eglMakeCurrent(m_backend->eglDisplay(), m_eglSurface, m_eglSurface, m_backend->context());
    GLRenderTarget::pushRenderTarget(m_renderTarget.data());
    return geometry;
}

void EglX11Output::endFrame(const QRegion &renderedRegion, const QRegion &damagedRegion)
{
    Q_UNUSED(damagedRegion)
    m_lastRenderedRegion = renderedRegion;
}

QRect EglX11Output::geometry() const
{
    return m_output->geometry();
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
        m_outputs[output] = QSharedPointer<EglX11Output>::create(output, this, s);
    }
    if (m_outputs.isEmpty()) {
        return false;
    }
    setSurface(m_outputs.first()->m_eglSurface);
    return true;
}

void EglX11Backend::present(AbstractOutput *output)
{
    static_cast<X11WindowedOutput *>(output)->vsyncMonitor()->arm();
    GLRenderTarget::popRenderTarget();

    presentSurface(m_outputs[output]->m_eglSurface, m_outputs[output]->m_lastRenderedRegion, output->geometry());
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

SurfaceTexture *EglX11Backend::createSurfaceTextureWayland(SurfacePixmapWayland *pixmap)
{
    return new BasicEGLSurfaceTextureWayland(this, pixmap);
}

SurfaceTexture *EglX11Backend::createSurfaceTextureInternal(SurfacePixmapInternal *pixmap)
{
    return new BasicEGLSurfaceTextureInternal(this, pixmap);
}

QVector<OutputLayer *> EglX11Backend::getLayers(RenderOutput *output)
{
    return {m_outputs[output->platformOutput()].get()};
}

} // namespace
