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
#include "xcbutils.h"
// kwin libs
#include <kwinglplatform.h>
// Qt
#include <QDebug>
#include <QOpenGLContext>
// system
#include <unistd.h>

namespace KWin
{

EglOnXBackend::EglOnXBackend()
    : OpenGLBackend()
    , m_overlayWindow(new OverlayWindow())
    , ctx(EGL_NO_CONTEXT)
    , surfaceHasSubPost(0)
    , m_bufferAge(0)
{
    init();
    // Egl is always direct rendering
    setIsDirectRendering(true);
}

EglOnXBackend::~EglOnXBackend()
{
    if (isFailed()) {
        m_overlayWindow->destroy();
    }
    cleanupGL();
    doneCurrent();
    eglDestroyContext(dpy, ctx);
    eglDestroySurface(dpy, surface);
    eglTerminate(dpy);
    eglReleaseThread();
    if (overlayWindow()->window()) {
        overlayWindow()->destroy();
    }
    delete m_overlayWindow;
}

static bool gs_tripleBufferUndetected = true;
static bool gs_tripleBufferNeedsDetection = false;

void EglOnXBackend::init()
{
    if (!initRenderingContext()) {
        setFailed(QStringLiteral("Could not initialize rendering context"));
        return;
    }

    initEGL();
    if (!hasGLExtension(QStringLiteral("EGL_KHR_image")) &&
        (!hasGLExtension(QStringLiteral("EGL_KHR_image_base")) ||
         !hasGLExtension(QStringLiteral("EGL_KHR_image_pixmap")))) {
        setFailed(QStringLiteral("Required support for binding pixmaps to EGLImages not found, disabling compositing"));
        return;
    }
    GLPlatform *glPlatform = GLPlatform::instance();
    glPlatform->detect(EglPlatformInterface);
    if (GLPlatform::instance()->driver() == Driver_Intel)
        options->setUnredirectFullscreen(false); // bug #252817
    options->setGlPreferBufferSwap(options->glPreferBufferSwap()); // resolve autosetting
    if (options->glPreferBufferSwap() == Options::AutoSwapStrategy)
        options->setGlPreferBufferSwap('e'); // for unknown drivers - should not happen
    glPlatform->printResults();
    initGL(EglPlatformInterface);
    if (!hasGLExtension(QStringLiteral("GL_OES_EGL_image"))) {
        setFailed(QStringLiteral("Required extension GL_OES_EGL_image not found, disabling compositing"));
        return;
    }

    // check for EGL_NV_post_sub_buffer and whether it can be used on the surface
    if (eglPostSubBufferNV) {
        if (eglQuerySurface(dpy, surface, EGL_POST_SUB_BUFFER_SUPPORTED_NV, &surfaceHasSubPost) == EGL_FALSE) {
            EGLint error = eglGetError();
            if (error != EGL_SUCCESS && error != EGL_BAD_ATTRIBUTE) {
                setFailed(QStringLiteral("query surface failed"));
                return;
            } else {
                surfaceHasSubPost = EGL_FALSE;
            }
        }
    }

    setSupportsBufferAge(false);

    if (hasGLExtension("EGL_EXT_buffer_age")) {
        const QByteArray useBufferAge = qgetenv("KWIN_USE_BUFFER_AGE");

        if (useBufferAge != "0")
            setSupportsBufferAge(true);
    }

    setSyncsToVBlank(false);
    setBlocksForRetrace(false);
    gs_tripleBufferNeedsDetection = false;
    m_swapProfiler.init();
    if (surfaceHasSubPost) {
        qDebug() << "EGL implementation and surface support eglPostSubBufferNV, let's use it";

        if (options->glPreferBufferSwap() != Options::NoSwapEncourage) {
            // check if swap interval 1 is supported
            EGLint val;
            eglGetConfigAttrib(dpy, config, EGL_MAX_SWAP_INTERVAL, &val);
            if (val >= 1) {
                if (eglSwapInterval(dpy, 1)) {
                    qDebug() << "Enabled v-sync";
                    setSyncsToVBlank(true);
                    const QByteArray tripleBuffer = qgetenv("KWIN_TRIPLE_BUFFER");
                    if (!tripleBuffer.isEmpty()) {
                        setBlocksForRetrace(qstrcmp(tripleBuffer, "0") == 0);
                        gs_tripleBufferUndetected = false;
                    }
                    gs_tripleBufferNeedsDetection = gs_tripleBufferUndetected;
                }
            } else {
                qWarning() << "Cannot enable v-sync as max. swap interval is" << val;
            }
        } else {
            // disable v-sync
            eglSwapInterval(dpy, 0);
        }
    } else {
        /* In the GLX backend, we fall back to using glCopyPixels if we have no extension providing support for partial screen updates.
         * However, that does not work in EGL - glCopyPixels with glDrawBuffer(GL_FRONT); does nothing.
         * Hence we need EGL to preserve the backbuffer for us, so that we can draw the partial updates on it and call
         * eglSwapBuffers() for each frame. eglSwapBuffers() then does the copy (no page flip possible in this mode),
         * which means it is slow and not synced to the v-blank. */
        qWarning() << "eglPostSubBufferNV not supported, have to enable buffer preservation - which breaks v-sync and performance";
        eglSurfaceAttrib(dpy, surface, EGL_SWAP_BEHAVIOR, EGL_BUFFER_PRESERVED);
    }
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
        qCritical() << "bind OpenGL API failed";
        return false;
    }
#endif

    initBufferConfigs();

    if (!overlayWindow()->create()) {
        qCritical() << "Could not get overlay window";
        return false;
    } else {
        overlayWindow()->setup(None);
    }

    surface = eglCreateWindowSurface(dpy, config, overlayWindow()->window(), 0);

#ifdef KWIN_HAVE_OPENGLES
    const EGLint context_attribs[] = {
        EGL_CONTEXT_CLIENT_VERSION, 2,
        EGL_NONE
    };

    ctx = eglCreateContext(dpy, config, EGL_NO_CONTEXT, context_attribs);
#else
    const EGLint context_attribs_31_core[] = {
        EGL_CONTEXT_MAJOR_VERSION_KHR, 3,
        EGL_CONTEXT_MINOR_VERSION_KHR, 1,
        EGL_NONE
    };

    const EGLint context_attribs_legacy[] = {
        EGL_NONE
    };

    const QByteArray eglExtensions = eglQueryString(dpy, EGL_EXTENSIONS);
    const QList<QByteArray> extensions = eglExtensions.split(' ');

    // Try to create a 3.1 core context
    if (options->glCoreProfile() && extensions.contains("EGL_KHR_create_context"))
        ctx = eglCreateContext(dpy, config, EGL_NO_CONTEXT, context_attribs_31_core);

    if (ctx == EGL_NO_CONTEXT)
        ctx = eglCreateContext(dpy, config, EGL_NO_CONTEXT, context_attribs_legacy);
#endif

    if (ctx == EGL_NO_CONTEXT) {
        qCritical() << "Create Context failed";
        return false;
    }

    if (eglMakeCurrent(dpy, surface, surface, ctx) == EGL_FALSE) {
        qCritical() << "Make Context Current failed";
        return false;
    }

    qDebug() << "EGL version: " << major << "." << minor;

    EGLint error = eglGetError();
    if (error != EGL_SUCCESS) {
        qWarning() << "Error occurred while creating context " << error;
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
        qCritical() << "choose config failed";
        return false;
    }

    Xcb::WindowAttributes attribs(rootWindow());
    if (!attribs) {
        qCritical() << "Failed to get window attributes of root window";
        return false;
    }

    config = configs[0];
    for (int i = 0; i < count; i++) {
        EGLint val;
        if (eglGetConfigAttrib(dpy, configs[i], EGL_NATIVE_VISUAL_ID, &val) == EGL_FALSE) {
            qCritical() << "egl get config attrib failed";
        }
        if (uint32_t(val) == attribs->visual) {
            config = configs[i];
            break;
        }
    }
    return true;
}

void EglOnXBackend::present()
{
    if (lastDamage().isEmpty())
        return;

    if (supportsBufferAge()) {
        eglSwapBuffers(dpy, surface);
        eglQuerySurface(dpy, surface, EGL_BUFFER_AGE_EXT, &m_bufferAge);
        setLastDamage(QRegion());
        return;
    }

    const QRegion displayRegion(0, 0, displayWidth(), displayHeight());
    const bool fullRepaint = (lastDamage() == displayRegion);

    if (fullRepaint || !surfaceHasSubPost) {
        if (gs_tripleBufferNeedsDetection) {
            eglWaitGL();
            m_swapProfiler.begin();
        }
        // the entire screen changed, or we cannot do partial updates (which implies we enabled surface preservation)
        eglSwapBuffers(dpy, surface);
        if (gs_tripleBufferNeedsDetection) {
            eglWaitGL();
            if (char result = m_swapProfiler.end()) {
                gs_tripleBufferUndetected = gs_tripleBufferNeedsDetection = false;
                if (result == 'd' && GLPlatform::instance()->driver() == Driver_NVidia) {
                    // TODO this is a workaround, we should get __GL_YIELD set before libGL checks it
                    if (qstrcmp(qgetenv("__GL_YIELD"), "USLEEP")) {
                        options->setGlPreferBufferSwap(0);
                        eglSwapInterval(dpy, 0);
                        qWarning() << "\nIt seems you are using the nvidia driver without triple buffering\n"
                                          "You must export __GL_YIELD=\"USLEEP\" to prevent large CPU overhead on synced swaps\n"
                                          "Preferably, enable the TripleBuffer Option in the xorg.conf Device\n"
                                          "For this reason, the tearing prevention has been disabled.\n"
                                          "See https://bugs.kde.org/show_bug.cgi?id=322060\n";
                    }
                }
                setBlocksForRetrace(result == 'd');
            }
        }
    } else {
        // a part of the screen changed, and we can use eglPostSubBufferNV to copy the updated area
        foreach (const QRect & r, lastDamage().rects()) {
            eglPostSubBufferNV(dpy, surface, r.left(), displayHeight() - r.bottom() - 1, r.width(), r.height());
        }
    }

    setLastDamage(QRegion());
    eglWaitGL();
    xcb_flush(connection());
}

void EglOnXBackend::screenGeometryChanged(const QSize &size)
{
    Q_UNUSED(size)

    // TODO: base implementation in OpenGLBackend

    // The back buffer contents are now undefined
    m_bufferAge = 0;
}

SceneOpenGL::TexturePrivate *EglOnXBackend::createBackendTexture(SceneOpenGL::Texture *texture)
{
    return new EglTexture(texture, this);
}

QRegion EglOnXBackend::prepareRenderingFrame()
{
    QRegion repaint;

    if (gs_tripleBufferNeedsDetection) {
        // the composite timer floors the repaint frequency. This can pollute our triple buffering
        // detection because the glXSwapBuffers call for the new frame has to wait until the pending
        // one scanned out.
        // So we compensate for that by waiting an extra milisecond to give the driver the chance to
        // fllush the buffer queue
        usleep(1000);
    }

    present();

    if (supportsBufferAge())
        repaint = accumulatedDamageHistory(m_bufferAge);

    startRenderTimer();
    eglWaitNative(EGL_CORE_NATIVE_ENGINE);

    return repaint;
}

void EglOnXBackend::endRenderingFrame(const QRegion &renderedRegion, const QRegion &damagedRegion)
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

    if (overlayWindow()->window())  // show the window only after the first pass,
        overlayWindow()->show();   // since that pass may take long

    // Save the damaged region to history
    if (supportsBufferAge())
        addToDamageHistory(damagedRegion);
}

bool EglOnXBackend::makeCurrent()
{
    if (QOpenGLContext *context = QOpenGLContext::currentContext()) {
        // Workaround to tell Qt that no QOpenGLContext is current
        context->doneCurrent();
    }
    const bool current = eglMakeCurrent(dpy, surface, surface, ctx);
    return current;
}

void EglOnXBackend::doneCurrent()
{
    eglMakeCurrent(dpy, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
}

bool EglOnXBackend::usesOverlayWindow() const
{
    return true;
}

OverlayWindow* EglOnXBackend::overlayWindow()
{
    return m_overlayWindow;
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
        qDebug() << "failed to create egl image";
        q->unbind();
        q->discard();
        return false;
    }
    glEGLImageTargetTexture2DOES(GL_TEXTURE_2D, (GLeglImageOES)m_image);
    q->unbind();
    checkGLError("load texture");
    q->setYInverted(true);
    m_size = size;
    updateMatrix();
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
