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
    for (int i = 0; i < screens()->count(); ++i) {
        EGLSurface s = createSurface(m_backend->windowForScreen(i));
        if (s == EGL_NO_SURFACE) {
            return false;
        }
        m_surfaces << s;
    }
    if (m_surfaces.isEmpty()) {
        return false;
    }
    setSurface(m_surfaces.first());
    return true;
}

QRegion EglX11Backend::beginFrame(int screenId)
{
    makeContextCurrent(m_surfaces.at(screenId));
    setupViewport(screenId);
    return screens()->geometry(screenId);
}

void EglX11Backend::setupViewport(int screenId)
{
    qreal scale = screens()->scale(screenId);
    const QSize size = screens()->geometry(screenId).size() * scale;
    glViewport(0, 0, size.width(), size.height());
}

void EglX11Backend::endFrame(int screenId, const QRegion &renderedRegion, const QRegion &damagedRegion)
{
    Q_UNUSED(damagedRegion)

    X11WindowedOutput *output = static_cast<X11WindowedOutput *>(kwinApp()->platform()->findOutput(screenId));
    output->vsyncMonitor()->arm();

    const QRect &outputGeometry = screens()->geometry(screenId);
    presentSurface(m_surfaces.at(screenId), renderedRegion, outputGeometry);
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

PlatformSurfaceTexture *EglX11Backend::createPlatformSurfaceTextureWayland(SurfacePixmapWayland *pixmap)
{
    return new BasicEGLSurfaceTextureWayland(this, pixmap);
}

PlatformSurfaceTexture *EglX11Backend::createPlatformSurfaceTextureInternal(SurfacePixmapInternal *pixmap)
{
    return new BasicEGLSurfaceTextureInternal(this, pixmap);
}

} // namespace
