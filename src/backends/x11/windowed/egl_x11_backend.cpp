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
    for (auto it = m_surfaces.begin(); it != m_surfaces.end(); ++it) {
        eglDestroySurface(eglDisplay(), *it);
    }
}

bool EglX11Backend::createSurfaces()
{
    const auto &outputs = m_backend->outputs();
    for (const auto &output : outputs) {
        EGLSurface s = createSurface(m_backend->windowForScreen(output));
        if (s == EGL_NO_SURFACE) {
            return false;
        }
        m_surfaces.insert(output, s);
    }
    if (m_surfaces.isEmpty()) {
        return false;
    }
    setSurface(m_surfaces.first());
    return true;
}

QRegion EglX11Backend::beginFrame(AbstractOutput *output)
{
    makeContextCurrent(m_surfaces[output]);
    setupViewport(output);
    return output->geometry();
}

void EglX11Backend::setupViewport(AbstractOutput *output)
{
    const QSize size = output->pixelSize() * output->scale();
    glViewport(0, 0, size.width(), size.height());
}

void EglX11Backend::endFrame(AbstractOutput *output, const QRegion &renderedRegion, const QRegion &damagedRegion)
{
    Q_UNUSED(damagedRegion)

    static_cast<X11WindowedOutput *>(output)->vsyncMonitor()->arm();

    presentSurface(m_surfaces[output], renderedRegion, output->geometry());
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

} // namespace
