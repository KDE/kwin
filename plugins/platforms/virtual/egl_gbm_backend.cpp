/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2015 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "egl_gbm_backend.h"
// kwin
#include "composite.h"
#include "virtual_backend.h"
#include "options.h"
#include "screens.h"
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

EglGbmBackend::EglGbmBackend(VirtualBackend *b)
    : AbstractEglBackend()
    , m_backend(b)
{
    // Egl is always direct rendering
    setIsDirectRendering(true);
}

EglGbmBackend::~EglGbmBackend()
{
    while (GLRenderTarget::isRenderTargetBound()) {
        GLRenderTarget::popRenderTarget();
    }
    delete m_fbo;
    delete m_backBuffer;
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

    if (display == EGL_NO_DISPLAY)
        return false;
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

    m_backBuffer = new GLTexture(GL_RGB8, screens()->size().width(), screens()->size().height());
    m_fbo = new GLRenderTarget(*m_backBuffer);
    if (!m_fbo->valid()) {
        setFailed("Could not create framebuffer object");
        return;
    }
    GLRenderTarget::pushRenderTarget(m_fbo);
    if (!m_fbo->isRenderTargetBound()) {
        setFailed("Failed to bind framebuffer object");
        return;
    }
    if (checkGLError("Init")) {
        setFailed("Error during init of EglGbmBackend");
        return;
    }

    setSupportsBufferAge(false);
    initWayland();
}

bool EglGbmBackend::initRenderingContext()
{
    initBufferConfigs();

    if (!supportsSurfacelessContext()) {
        qCWarning(KWIN_VIRTUAL) << "EGL_KHR_surfaceless_context extension is unavailable";
        return false;
    }

    if (!createContext()) {
        return false;
    }

    return makeCurrent();
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
    if (eglChooseConfig(eglDisplay(), config_attribs, configs, 1, &count) == EGL_FALSE) {
        return false;
    }
    if (count != 1) {
        return false;
    }
    setConfig(configs[0]);

    return true;
}

void EglGbmBackend::present()
{
    Compositor::self()->aboutToSwapBuffers();

    eglSwapBuffers(eglDisplay(), surface());
    setLastDamage(QRegion());

    Compositor::self()->bufferSwapComplete();
}

void EglGbmBackend::screenGeometryChanged(const QSize &size)
{
    Q_UNUSED(size)
    // TODO, create new buffer?
}

SceneOpenGLTexturePrivate *EglGbmBackend::createBackendTexture(SceneOpenGLTexture *texture)
{
    return new EglGbmTexture(texture, this);
}

QRegion EglGbmBackend::prepareRenderingFrame()
{
    if (!lastDamage().isEmpty()) {
        present();
    }
    startRenderTimer();
    if (!GLRenderTarget::isRenderTargetBound()) {
        GLRenderTarget::pushRenderTarget(m_fbo);
    }
    return QRegion(0, 0, screens()->size().width(), screens()->size().height());
}

static void convertFromGLImage(QImage &img, int w, int h)
{
    // from QtOpenGL/qgl.cpp
    // SPDX-FileCopyrightText: 2010 Nokia Corporation and /or its subsidiary(-ies)
    // see https://github.com/qt/qtbase/blob/dev/src/opengl/qgl.cpp
    if (QSysInfo::ByteOrder == QSysInfo::BigEndian) {
        // OpenGL gives RGBA; Qt wants ARGB
        uint *p = reinterpret_cast<uint *>(img.bits());
        uint *end = p + w * h;
        while (p < end) {
            uint a = *p << 24;
            *p = (*p >> 8) | a;
            p++;
        }
    } else {
        // OpenGL gives ABGR (i.e. RGBA backwards); Qt wants ARGB
        for (int y = 0; y < h; y++) {
            uint *q = reinterpret_cast<uint*>(img.scanLine(y));
            for (int x = 0; x < w; ++x) {
                const uint pixel = *q;
                *q = ((pixel << 16) & 0xff0000) | ((pixel >> 16) & 0xff)
                     | (pixel & 0xff00ff00);

                q++;
            }
        }

    }
    img = img.mirrored();
}

void EglGbmBackend::endRenderingFrame(const QRegion &renderedRegion, const QRegion &damagedRegion)
{
    Q_UNUSED(damagedRegion)
    glFlush();
    if (m_backend->saveFrames()) {
        QImage img = QImage(QSize(m_backBuffer->width(), m_backBuffer->height()), QImage::Format_ARGB32);
        glReadnPixels(0, 0, m_backBuffer->width(), m_backBuffer->height(), GL_RGBA, GL_UNSIGNED_BYTE, img.sizeInBytes(), (GLvoid*)img.bits());
        convertFromGLImage(img, m_backBuffer->width(), m_backBuffer->height());
        img.save(QStringLiteral("%1/%2.png").arg(m_backend->saveFrames()).arg(QString::number(m_frameCounter++)));
    }
    GLRenderTarget::popRenderTarget();
    setLastDamage(renderedRegion);
}

bool EglGbmBackend::usesOverlayWindow() const
{
    return false;
}

/************************************************
 * EglTexture
 ************************************************/

EglGbmTexture::EglGbmTexture(KWin::SceneOpenGLTexture *texture, EglGbmBackend *backend)
    : AbstractEglTexture(texture, backend)
{
}

EglGbmTexture::~EglGbmTexture() = default;

} // namespace
