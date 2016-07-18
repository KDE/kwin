
/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2013 Martin Gräßlin <mgraesslin@kde.org>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*********************************************************************/
#define WL_EGL_PLATFORM 1
#include "egl_wayland_backend.h"
// kwin
#include "composite.h"
#include "logging.h"
#include "options.h"
#include "wayland_backend.h"
#include "wayland_server.h"
#include <KWayland/Client/surface.h>
// kwin libs
#include <kwinglplatform.h>
// KDE
#include <KWayland/Server/buffer_interface.h>
#include <KWayland/Server/display.h>
// Qt
#include <QOpenGLContext>

namespace KWin
{

EglWaylandBackend::EglWaylandBackend(Wayland::WaylandBackend *b)
    : QObject(NULL)
    , AbstractEglBackend()
    , m_bufferAge(0)
    , m_wayland(b)
    , m_overlay(NULL)
{
    if (!m_wayland) {
        setFailed("Wayland Backend has not been created");
        return;
    }
    qCDebug(KWIN_WAYLAND_BACKEND) << "Connected to Wayland display?" << (m_wayland->display() ? "yes" : "no" );
    if (!m_wayland->display()) {
        setFailed("Could not connect to Wayland compositor");
        return;
    }
    connect(m_wayland, SIGNAL(shellSurfaceSizeChanged(QSize)), SLOT(overlaySizeChanged(QSize)));
    // Egl is always direct rendering
    setIsDirectRendering(true);
}

EglWaylandBackend::~EglWaylandBackend()
{
    cleanup();
    if (m_overlay) {
        wl_egl_window_destroy(m_overlay);
    }
}

bool EglWaylandBackend::initializeEgl()
{
    initClientExtensions();
    EGLDisplay display = m_wayland->sceneEglDisplay();

    // Use eglGetPlatformDisplayEXT() to get the display pointer
    // if the implementation supports it.
    if (display == EGL_NO_DISPLAY) {
        m_havePlatformBase = hasClientExtension(QByteArrayLiteral("EGL_EXT_platform_base"));
        if (m_havePlatformBase) {
            // Make sure that the wayland platform is supported
            if (!hasClientExtension(QByteArrayLiteral("EGL_EXT_platform_wayland")))
                return false;

            display = eglGetPlatformDisplayEXT(EGL_PLATFORM_WAYLAND_EXT, m_wayland->display(), nullptr);
        } else {
            display = eglGetDisplay(m_wayland->display());
        }
    }

    if (display == EGL_NO_DISPLAY)
        return false;
    setEglDisplay(display);
    return initEglAPI();
}

void EglWaylandBackend::init()
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

bool EglWaylandBackend::initRenderingContext()
{
    initBufferConfigs();

    if (!createContext()) {
        return false;
    }

    if (!m_wayland->surface()) {
        return false;
    }

    const QSize &size = m_wayland->shellSurfaceSize();
    auto s = m_wayland->surface();
    connect(s, &KWayland::Client::Surface::frameRendered, Compositor::self(), &Compositor::bufferSwapComplete);
    m_overlay = wl_egl_window_create(*s, size.width(), size.height());
    if (!m_overlay) {
        qCCritical(KWIN_WAYLAND_BACKEND) << "Creating Wayland Egl window failed";
        return false;
    }

    EGLSurface surface = EGL_NO_SURFACE;
    if (m_havePlatformBase)
        surface = eglCreatePlatformWindowSurfaceEXT(eglDisplay(), config(), (void *) m_overlay, nullptr);
    else
        surface = eglCreateWindowSurface(eglDisplay(), config(), m_overlay, nullptr);

    if (surface == EGL_NO_SURFACE) {
        qCCritical(KWIN_WAYLAND_BACKEND) << "Create Window Surface failed";
        return false;
    }
    setSurface(surface);

    return makeContextCurrent();
}

bool EglWaylandBackend::makeContextCurrent()
{
    if (eglMakeCurrent(eglDisplay(), surface(), surface(), context()) == EGL_FALSE) {
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

bool EglWaylandBackend::initBufferConfigs()
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

void EglWaylandBackend::present()
{
    m_wayland->surface()->setupFrameCallback();
    Compositor::self()->aboutToSwapBuffers();

    if (supportsBufferAge()) {
        eglSwapBuffers(eglDisplay(), surface());
        eglQuerySurface(eglDisplay(), surface(), EGL_BUFFER_AGE_EXT, &m_bufferAge);
        setLastDamage(QRegion());
        return;
    } else {
        eglSwapBuffers(eglDisplay(), surface());
        setLastDamage(QRegion());
    }
}

void EglWaylandBackend::screenGeometryChanged(const QSize &size)
{
    Q_UNUSED(size)
    // no backend specific code needed
    // TODO: base implementation in OpenGLBackend

    // The back buffer contents are now undefined
    m_bufferAge = 0;
}

SceneOpenGL::TexturePrivate *EglWaylandBackend::createBackendTexture(SceneOpenGL::Texture *texture)
{
    return new EglWaylandTexture(texture, this);
}

QRegion EglWaylandBackend::prepareRenderingFrame()
{
    if (!lastDamage().isEmpty())
        present();
    QRegion repaint;
    if (supportsBufferAge())
        repaint = accumulatedDamageHistory(m_bufferAge);
    eglWaitNative(EGL_CORE_NATIVE_ENGINE);
    startRenderTimer();
    return repaint;
}

void EglWaylandBackend::endRenderingFrame(const QRegion &renderedRegion, const QRegion &damagedRegion)
{
    if (damagedRegion.isEmpty()) {
        setLastDamage(QRegion());

        // If the damaged region of a window is fully occluded, the only
        // rendering done, if any, will have been to repair a reused back
        // buffer, making it identical to the front buffer.
        //
        // In this case we won't post the back buffer. Instead we'll just
        // set the buffer age to 1, so the repaired regions won't be
        // rendered again in the next frame.
        if (!renderedRegion.isEmpty())
            glFlush();

        m_bufferAge = 1;
        return;
    }

    setLastDamage(renderedRegion);

    if (!blocksForRetrace()) {
        // This also sets lastDamage to empty which prevents the frame from
        // being posted again when prepareRenderingFrame() is called.
        present();
    } else {
        // Make sure that the GPU begins processing the command stream
        // now and not the next time prepareRenderingFrame() is called.
        glFlush();
    }

    // Save the damaged region to history
    if (supportsBufferAge())
        addToDamageHistory(damagedRegion);
}

void EglWaylandBackend::overlaySizeChanged(const QSize &size)
{
    wl_egl_window_resize(m_overlay, size.width(), size.height(), 0, 0);
}

bool EglWaylandBackend::usesOverlayWindow() const
{
    return false;
}

/************************************************
 * EglTexture
 ************************************************/

EglWaylandTexture::EglWaylandTexture(KWin::SceneOpenGL::Texture *texture, KWin::EglWaylandBackend *backend)
    : AbstractEglTexture(texture, backend)
{
}

EglWaylandTexture::~EglWaylandTexture() = default;

} // namespace
