/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2015 Martin Gräßlin <mgraesslin@kde.org>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*********************************************************************/
#include "egl_hwcomposer_backend.h"
#include "hwcomposer_backend.h"
#include "logging.h"

namespace KWin
{

EglHwcomposerBackend::EglHwcomposerBackend(HwcomposerBackend *backend)
    : AbstractEglBackend()
    , m_backend(backend)
{
    // EGL is always direct rendering
    setIsDirectRendering(true);
    setSyncsToVBlank(true);
    setBlocksForRetrace(true);
}

EglHwcomposerBackend::~EglHwcomposerBackend()
{
    cleanup();
}

bool EglHwcomposerBackend::initializeEgl()
{
    // cannot use initClientExtensions as that crashes in libhybris
    qputenv("EGL_PLATFORM", QByteArrayLiteral("hwcomposer"));
    EGLDisplay display = m_backend->sceneEglDisplay();

    if (display == EGL_NO_DISPLAY) {
        display = eglGetDisplay(nullptr);
    }
    if (display == EGL_NO_DISPLAY) {
        return false;
    }
    setEglDisplay(display);
    return initEglAPI();
}

void EglHwcomposerBackend::init()
{
    if (!initializeEgl()) {
        setFailed("Failed to initialize egl");
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

bool EglHwcomposerBackend::initBufferConfigs()
{
    const EGLint config_attribs[] = {
        EGL_RED_SIZE,             8,
        EGL_GREEN_SIZE,           8,
        EGL_BLUE_SIZE,            8,
        EGL_ALPHA_SIZE,           0,
        EGL_RENDERABLE_TYPE,      EGL_OPENGL_ES2_BIT,
        EGL_NONE,
    };

    EGLint count;
    EGLConfig configs[1024];
    if (eglChooseConfig(eglDisplay(), config_attribs, configs, 1, &count) == EGL_FALSE) {
        qCCritical(KWIN_HWCOMPOSER) << "choose config failed";
        return false;
    }
    if (count != 1) {
        qCCritical(KWIN_HWCOMPOSER) << "choose config did not return a config" << count;
        return false;
    }
    setConfig(configs[0]);

    return true;
}

bool EglHwcomposerBackend::initRenderingContext()
{
    if (!initBufferConfigs()) {
        return false;
    }

    if (!createContext()) {
        return false;
    }

    m_nativeSurface = m_backend->createSurface();
    EGLSurface surface = eglCreateWindowSurface(eglDisplay(), config(), (EGLNativeWindowType)static_cast<ANativeWindow*>(m_nativeSurface), nullptr);
    if (surface == EGL_NO_SURFACE) {
        qCCritical(KWIN_HWCOMPOSER) << "Create surface failed";
        return false;
    }
    setSurface(surface);

    return makeContextCurrent();
}

bool EglHwcomposerBackend::makeContextCurrent()
{
    if (eglMakeCurrent(eglDisplay(), surface(), surface(), context()) == EGL_FALSE) {
        qCCritical(KWIN_HWCOMPOSER) << "Make Context Current failed";
        return false;
    }

    EGLint error = eglGetError();
    if (error != EGL_SUCCESS) {
        qCWarning(KWIN_HWCOMPOSER) << "Error occurred while creating context " << error;
        return false;
    }
    return true;
}

void EglHwcomposerBackend::present()
{
    if (lastDamage().isEmpty()) {
        return;
    }

    eglSwapBuffers(eglDisplay(), surface());
    setLastDamage(QRegion());
}

void EglHwcomposerBackend::screenGeometryChanged(const QSize &size)
{
    Q_UNUSED(size)
}

QRegion EglHwcomposerBackend::prepareRenderingFrame()
{
    present();

    // TODO: buffer age?
    startRenderTimer();
    // triggers always a full repaint
    return QRegion(QRect(QPoint(0, 0), m_backend->size()));
}

void EglHwcomposerBackend::endRenderingFrame(const QRegion &renderedRegion, const QRegion &damagedRegion)
{
    Q_UNUSED(damagedRegion)
    setLastDamage(renderedRegion);
}

SceneOpenGL::TexturePrivate *EglHwcomposerBackend::createBackendTexture(SceneOpenGL::Texture *texture)
{
    return new EglHwcomposerTexture(texture, this);
}

bool EglHwcomposerBackend::usesOverlayWindow() const
{
    return false;
}

EglHwcomposerTexture::EglHwcomposerTexture(SceneOpenGL::Texture *texture, EglHwcomposerBackend *backend)
    : AbstractEglTexture(texture, backend)
{
}

EglHwcomposerTexture::~EglHwcomposerTexture() = default;

}
