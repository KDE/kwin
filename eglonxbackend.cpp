/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2010, 2012 Martin Gräßlin <mgraesslin@kde.org>

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
#include "eglonxbackend.h"
// kwin
#include "options.h"
#include "overlaywindow.h"
// kwin libs
#include <kwinglplatform.h>

namespace KWin
{

EglOnXBackend::EglOnXBackend()
    : OpenGLBackend()
    , surfaceHasSubPost(0)
{
    init();
    // Egl is always direct rendering
    setIsDirectRendering(true);
    // Egl is always double buffered
    setDoubleBuffer(true);
}

EglOnXBackend::~EglOnXBackend()
{
    cleanupGL();
    eglMakeCurrent(dpy, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    eglDestroyContext(dpy, ctx);
    eglDestroySurface(dpy, surface);
    eglTerminate(dpy);
    eglReleaseThread();
    if (overlayWindow()->window()) {
        overlayWindow()->destroy();
    }
}

void EglOnXBackend::init()
{
    if (!initRenderingContext()) {
        setFailed("Could not initialize rendering context");
        return;
    }

    initEGL();
    if (!hasGLExtension("EGL_KHR_image") &&
        (!hasGLExtension("EGL_KHR_image_base") ||
         !hasGLExtension("EGL_KHR_image_pixmap"))) {
        setFailed("Required support for binding pixmaps to EGLImages not found, disabling compositing");
        return;
    }
    GLPlatform *glPlatform = GLPlatform::instance();
    glPlatform->detect(EglPlatformInterface);
    glPlatform->printResults();
    initGL(EglPlatformInterface);
    if (!hasGLExtension("GL_OES_EGL_image")) {
        setFailed("Required extension GL_OES_EGL_image not found, disabling compositing");
        return;
    }

// TODO: activate once this is resolved. currently the explicit invocation seems pointless
#if 0
    // - internet rumors say: it doesn't work with TBDR
    // - eglSwapInterval has no impact on intel GMA chips
    has_waitSync = options->isGlVSync();
    if (has_waitSync) {
        has_waitSync = (eglSwapInterval(dpy, 1) == EGL_TRUE);
        if (!has_waitSync)
            kWarning(1212) << "Could not activate EGL v'sync on this system";
    }
    if (!has_waitSync)
        eglSwapInterval(dpy, 0); // deactivate syncing
#endif
}


bool EglOnXBackend::initRenderingContext()
{
    dpy = eglGetDisplay(display());
    if (dpy == EGL_NO_DISPLAY)
        return false;
    EGLint major, minor;
    if (eglInitialize(dpy, &major, &minor) == EGL_FALSE)
        return false;
#ifdef KWIN_HAVE_OPENGLES
    eglBindAPI(EGL_OPENGL_ES_API);
#else
    if (eglBindAPI(EGL_OPENGL_API) == EGL_FALSE) {
        kError(1212) << "bind OpenGL API failed";
        return false;
    }
#endif
    initBufferConfigs();
    if (!overlayWindow()->create()) {
        kError(1212) << "Could not get overlay window";
        return false;
    } else {
        overlayWindow()->setup(None);
    }
    surface = eglCreateWindowSurface(dpy, config, overlayWindow()->window(), 0);

    eglSurfaceAttrib(dpy, surface, EGL_SWAP_BEHAVIOR, EGL_BUFFER_PRESERVED);

    if (eglQuerySurface(dpy, surface, EGL_POST_SUB_BUFFER_SUPPORTED_NV, &surfaceHasSubPost) == EGL_FALSE) {
        kError(1212) << "query surface failed";
        return false;
    }

    const EGLint context_attribs[] = {
#ifdef KWIN_HAVE_OPENGLES
        EGL_CONTEXT_CLIENT_VERSION, 2,
#endif
        EGL_NONE
    };

    ctx = eglCreateContext(dpy, config, EGL_NO_CONTEXT, context_attribs);
    if (ctx == EGL_NO_CONTEXT) {
        kError(1212) << "Create Context failed";
        return false;
    }
    if (eglMakeCurrent(dpy, surface, surface, ctx) == EGL_FALSE) {
        kError(1212) << "Make Context Current failed";
        return false;
    }
    kDebug(1212) << "EGL version: " << major << "." << minor;
    EGLint error = eglGetError();
    if (error != EGL_SUCCESS) {
        kWarning(1212) << "Error occurred while creating context " << error;
        return false;
    }
    return true;
}

bool EglOnXBackend::initBufferConfigs()
{
    const EGLint config_attribs[] = {
        EGL_SURFACE_TYPE,         EGL_WINDOW_BIT|EGL_SWAP_BEHAVIOR_PRESERVED_BIT,
        EGL_RED_SIZE,             1,
        EGL_GREEN_SIZE,           1,
        EGL_BLUE_SIZE,            1,
        EGL_ALPHA_SIZE,           0,
#ifdef KWIN_HAVE_OPENGLES
        EGL_RENDERABLE_TYPE,      EGL_OPENGL_ES2_BIT,
#else
        EGL_RENDERABLE_TYPE,      EGL_OPENGL_BIT,
#endif
        EGL_CONFIG_CAVEAT,        EGL_NONE,
        EGL_NONE,
    };

    EGLint count;
    EGLConfig configs[1024];
    if (eglChooseConfig(dpy, config_attribs, configs, 1024, &count) == EGL_FALSE) {
        kError(1212) << "choose config failed";
        return false;
    }

    EGLint visualId = XVisualIDFromVisual((Visual*)QX11Info::appVisual());

    config = configs[0];
    for (int i = 0; i < count; i++) {
        EGLint val;
        if (eglGetConfigAttrib(dpy, configs[i], EGL_NATIVE_VISUAL_ID, &val) == EGL_FALSE) {
            kError(1212) << "egl get config attrib failed";
        }
        if (visualId == val) {
            config = configs[i];
            break;
        }
    }
    return true;
}

void EglOnXBackend::flushBuffer()
{
    if (lastMask() & Scene::PAINT_SCREEN_REGION && surfaceHasSubPost && eglPostSubBufferNV) {
        const QRect damageRect = lastDamage().boundingRect();

        eglPostSubBufferNV(dpy, surface, damageRect.left(), displayHeight() - damageRect.bottom() - 1, damageRect.width(), damageRect.height());
    } else {
        eglSwapBuffers(dpy, surface);
    }

    eglWaitGL();
    XFlush(display());
}

void EglOnXBackend::screenGeometryChanged(const QSize &size)
{
    Q_UNUSED(size)
    // no backend specific code needed
    // TODO: base implementation in OpenGLBackend
}

SceneOpenGL::TexturePrivate *EglOnXBackend::createBackendTexture(SceneOpenGL::Texture *texture)
{
    return new EglTexture(texture, this);
}

void EglOnXBackend::prepareRenderingFrame()
{
    if (!lastDamage().isEmpty())
        flushBuffer();
    startRenderTimer();
}

void EglOnXBackend::endRenderingFrame(int mask, const QRegion &damage)
{
    setLastDamage(damage);
    setLastMask(mask);
    glFlush();

    if (overlayWindow()->window())  // show the window only after the first pass,
        overlayWindow()->show();   // since that pass may take long
}

/************************************************
 * EglTexture
 ************************************************/

EglTexture::EglTexture(KWin::SceneOpenGL::Texture *texture, KWin::EglOnXBackend *backend)
    : SceneOpenGL::TexturePrivate()
    , q(texture)
    , m_backend(backend)
    , m_image(EGL_NO_IMAGE_KHR)
{
    m_target = GL_TEXTURE_2D;
}

EglTexture::~EglTexture()
{
    if (m_image != EGL_NO_IMAGE_KHR) {
        eglDestroyImageKHR(m_backend->dpy, m_image);
    }
}

OpenGLBackend *EglTexture::backend()
{
    return m_backend;
}

void EglTexture::findTarget()
{
    if (m_target != GL_TEXTURE_2D) {
        m_target = GL_TEXTURE_2D;
    }
}

bool EglTexture::loadTexture(const Pixmap &pix, const QSize &size, int depth)
{
    Q_UNUSED(depth)
    if (pix == None)
        return false;

    glGenTextures(1, &m_texture);
    q->setWrapMode(GL_CLAMP_TO_EDGE);
    q->setFilter(GL_LINEAR);
    q->bind();
    const EGLint attribs[] = {
        EGL_IMAGE_PRESERVED_KHR, EGL_TRUE,
        EGL_NONE
    };
    m_image = eglCreateImageKHR(m_backend->dpy, EGL_NO_CONTEXT, EGL_NATIVE_PIXMAP_KHR,
                                          (EGLClientBuffer)pix, attribs);

    if (EGL_NO_IMAGE_KHR == m_image) {
        kDebug(1212) << "failed to create egl image";
        q->unbind();
        q->discard();
        return false;
    }
    glEGLImageTargetTexture2DOES(GL_TEXTURE_2D, (GLeglImageOES)m_image);
    q->unbind();
    checkGLError("load texture");
    q->setYInverted(true);
    m_size = size;
    return true;
}

void KWin::EglTexture::onDamage()
{
    if (options->isGlStrictBinding()) {
        // This is just implemented to be consistent with
        // the example in mesa/demos/src/egl/opengles1/texture_from_pixmap.c
        eglWaitNative(EGL_CORE_NATIVE_ENGINE);
        glEGLImageTargetTexture2DOES(GL_TEXTURE_2D, (GLeglImageOES) m_image);
    }
    GLTexturePrivate::onDamage();
}

} // namespace
