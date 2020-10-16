/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2010, 2012 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "eglonxbackend.h"
// kwin
#include "main.h"
#include "options.h"
#include "overlaywindow.h"
#include "platform.h"
#include "scene.h"
#include "screens.h"
#include "xcbutils.h"
#include "texture.h"
// kwin libs
#include <kwinglplatform.h>
#include <kwinglutils.h>
// Qt
#include <QLoggingCategory>
#include <QDebug>
#include <QOpenGLContext>
// system
#include <unistd.h>

Q_LOGGING_CATEGORY(KWIN_CORE, "kwin_core", QtCriticalMsg)

namespace KWin
{

EglOnXBackend::EglOnXBackend(Display *display)
    : AbstractEglBackend()
    , m_overlayWindow(kwinApp()->platform()->createOverlayWindow())
    , surfaceHasSubPost(0)
    , m_bufferAge(0)
    , m_usesOverlayWindow(true)
    , m_connection(connection())
    , m_x11Display(display)
    , m_rootWindow(rootWindow())
    , m_x11ScreenNumber(kwinApp()->x11ScreenNumber())
{
    // Egl is always direct rendering
    setIsDirectRendering(true);
}

EglOnXBackend::EglOnXBackend(xcb_connection_t *connection, Display *display, xcb_window_t rootWindow, int screenNumber, xcb_window_t renderingWindow)
    : AbstractEglBackend()
    , m_overlayWindow(nullptr)
    , surfaceHasSubPost(0)
    , m_bufferAge(0)
    , m_usesOverlayWindow(false)
    , m_connection(connection)
    , m_x11Display(display)
    , m_rootWindow(rootWindow)
    , m_x11ScreenNumber(screenNumber)
    , m_renderingWindow(renderingWindow)
{
    // Egl is always direct rendering
    setIsDirectRendering(true);
}

static bool gs_tripleBufferUndetected = true;
static bool gs_tripleBufferNeedsDetection = false;

EglOnXBackend::~EglOnXBackend()
{
    if (isFailed() && m_overlayWindow) {
        m_overlayWindow->destroy();
    }
    cleanup();

    gs_tripleBufferUndetected = true;
    gs_tripleBufferNeedsDetection = false;

    if (m_overlayWindow) {
        if (overlayWindow()->window()) {
            overlayWindow()->destroy();
        }
        delete m_overlayWindow;
    }
}

void EglOnXBackend::init()
{
    qputenv("EGL_PLATFORM", "x11");
    if (!initRenderingContext()) {
        setFailed(QStringLiteral("Could not initialize rendering context"));
        return;
    }

    initKWinGL();
    if (!hasExtension(QByteArrayLiteral("EGL_KHR_image")) &&
        (!hasExtension(QByteArrayLiteral("EGL_KHR_image_base")) ||
         !hasExtension(QByteArrayLiteral("EGL_KHR_image_pixmap")))) {
        setFailed(QStringLiteral("Required support for binding pixmaps to EGLImages not found, disabling compositing"));
        return;
    }
    if (!hasGLExtension(QByteArrayLiteral("GL_OES_EGL_image"))) {
        setFailed(QStringLiteral("Required extension GL_OES_EGL_image not found, disabling compositing"));
        return;
    }

    // check for EGL_NV_post_sub_buffer and whether it can be used on the surface
    if (hasExtension(QByteArrayLiteral("EGL_NV_post_sub_buffer"))) {
        if (eglQuerySurface(eglDisplay(), surface(), EGL_POST_SUB_BUFFER_SUPPORTED_NV, &surfaceHasSubPost) == EGL_FALSE) {
            EGLint error = eglGetError();
            if (error != EGL_SUCCESS && error != EGL_BAD_ATTRIBUTE) {
                setFailed(QStringLiteral("query surface failed"));
                return;
            } else {
                surfaceHasSubPost = EGL_FALSE;
            }
        }
    }

    setSyncsToVBlank(false);
    setBlocksForRetrace(false);
    gs_tripleBufferNeedsDetection = false;
    m_swapProfiler.init();
    if (surfaceHasSubPost) {
        qCDebug(KWIN_CORE) << "EGL implementation and surface support eglPostSubBufferNV, let's use it";

        if (options->glPreferBufferSwap() != Options::NoSwapEncourage) {
            // check if swap interval 1 is supported
            EGLint val;
            eglGetConfigAttrib(eglDisplay(), config(), EGL_MAX_SWAP_INTERVAL, &val);
            if (val >= 1) {
                if (eglSwapInterval(eglDisplay(), 1)) {
                    qCDebug(KWIN_CORE) << "Enabled v-sync";
                    setSyncsToVBlank(true);
                    const QByteArray tripleBuffer = qgetenv("KWIN_TRIPLE_BUFFER");
                    if (!tripleBuffer.isEmpty()) {
                        setBlocksForRetrace(qstrcmp(tripleBuffer, "0") == 0);
                        gs_tripleBufferUndetected = false;
                    }
                    gs_tripleBufferNeedsDetection = gs_tripleBufferUndetected;
                }
            } else {
                qCWarning(KWIN_CORE) << "Cannot enable v-sync as max. swap interval is" << val;
            }
        } else {
            // disable v-sync
            eglSwapInterval(eglDisplay(), 0);
        }
    } else {
        /* In the GLX backend, we fall back to using glCopyPixels if we have no extension providing support for partial screen updates.
         * However, that does not work in EGL - glCopyPixels with glDrawBuffer(GL_FRONT); does nothing.
         * Hence we need EGL to preserve the backbuffer for us, so that we can draw the partial updates on it and call
         * eglSwapBuffers() for each frame. eglSwapBuffers() then does the copy (no page flip possible in this mode),
         * which means it is slow and not synced to the v-blank. */
        qCWarning(KWIN_CORE) << "eglPostSubBufferNV not supported, have to enable buffer preservation - which breaks v-sync and performance";
        eglSurfaceAttrib(eglDisplay(), surface(), EGL_SWAP_BEHAVIOR, EGL_BUFFER_PRESERVED);
    }

    initWayland();
}

bool EglOnXBackend::initRenderingContext()
{
    initClientExtensions();
    EGLDisplay dpy = kwinApp()->platform()->sceneEglDisplay();

    // Use eglGetPlatformDisplayEXT() to get the display pointer
    // if the implementation supports it.
    if (dpy == EGL_NO_DISPLAY) {
        const bool havePlatformBase = hasClientExtension(QByteArrayLiteral("EGL_EXT_platform_base"));
        setHavePlatformBase(havePlatformBase);
        if (havePlatformBase) {
            // Make sure that the X11 platform is supported
            if (!hasClientExtension(QByteArrayLiteral("EGL_EXT_platform_x11")) &&
                !hasClientExtension(QByteArrayLiteral("EGL_KHR_platform_x11"))) {
                qCWarning(KWIN_CORE) << "EGL_EXT_platform_base is supported, but neither EGL_EXT_platform_x11 nor EGL_KHR_platform_x11 is supported."
                                     << "Cannot create EGLDisplay on X11";
                return false;
            }

            const int attribs[] = {
                EGL_PLATFORM_X11_SCREEN_EXT, m_x11ScreenNumber,
                EGL_NONE
            };

            dpy = eglGetPlatformDisplayEXT(EGL_PLATFORM_X11_EXT, m_x11Display, attribs);
        } else {
            dpy = eglGetDisplay(m_x11Display);
        }
    }

    if (dpy == EGL_NO_DISPLAY) {
        qCWarning(KWIN_CORE) << "Failed to get the EGLDisplay";
        return false;
    }
    setEglDisplay(dpy);
    initEglAPI();

    initBufferConfigs();

    if (m_usesOverlayWindow) {
        if (!overlayWindow()->create()) {
            qCCritical(KWIN_CORE) << "Could not get overlay window";
            return false;
        } else {
            overlayWindow()->setup(None);
        }
    }
    if (!createSurfaces()) {
        qCCritical(KWIN_CORE) << "Creating egl surface failed";
        return false;
    }

    if (!createContext()) {
        qCCritical(KWIN_CORE) << "Create OpenGL context failed";
        return false;
    }

    if (!makeContextCurrent(surface())) {
        qCCritical(KWIN_CORE) << "Make Context Current failed";
        return false;
    }

    EGLint error = eglGetError();
    if (error != EGL_SUCCESS) {
        qCWarning(KWIN_CORE) << "Error occurred while creating context " << error;
        return false;
    }

    return true;
}

bool EglOnXBackend::createSurfaces()
{
    xcb_window_t window = XCB_WINDOW_NONE;
    if (m_overlayWindow) {
        window = m_overlayWindow->window();
    } else if (m_renderingWindow) {
        window = m_renderingWindow;
    }

    EGLSurface surface = createSurface(window);

    if (surface == EGL_NO_SURFACE) {
        return false;
    }
    setSurface(surface);
    return true;
}

EGLSurface EglOnXBackend::createSurface(xcb_window_t window)
{
    if (window == XCB_WINDOW_NONE) {
        return EGL_NO_SURFACE;
    }

    EGLSurface surface = EGL_NO_SURFACE;
    if (havePlatformBase()) {
        // Note: Window is 64 bits on a 64-bit architecture whereas xcb_window_t is
        //       always 32 bits. eglCreatePlatformWindowSurfaceEXT() expects the
        //       native_window parameter to be pointer to a Window, so this variable
        //       cannot be an xcb_window_t.
        surface = eglCreatePlatformWindowSurfaceEXT(eglDisplay(), config(), (void *) &window, nullptr);
    } else {
        surface = eglCreateWindowSurface(eglDisplay(), config(), window, nullptr);
    }

    return surface;
}

bool EglOnXBackend::initBufferConfigs()
{
    initBufferAge();
    const EGLint config_attribs[] = {
        EGL_SURFACE_TYPE,         EGL_WINDOW_BIT | (supportsBufferAge() ? 0 : EGL_SWAP_BEHAVIOR_PRESERVED_BIT),
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
    if (eglChooseConfig(eglDisplay(), config_attribs, configs, 1024, &count) == EGL_FALSE) {
        qCCritical(KWIN_CORE) << "choose config failed";
        return false;
    }

    ScopedCPointer<xcb_get_window_attributes_reply_t> attribs(xcb_get_window_attributes_reply(m_connection,
                                                                                              xcb_get_window_attributes_unchecked(m_connection, m_rootWindow),
                                                                                              nullptr));
    if (!attribs) {
        qCCritical(KWIN_CORE) << "Failed to get window attributes of root window";
        return false;
    }

    setConfig(configs[0]);
    for (int i = 0; i < count; i++) {
        EGLint val;
        if (eglGetConfigAttrib(eglDisplay(), configs[i], EGL_NATIVE_VISUAL_ID, &val) == EGL_FALSE) {
            qCCritical(KWIN_CORE) << "egl get config attrib failed";
        }
        if (uint32_t(val) == attribs->visual) {
            setConfig(configs[i]);
            break;
        }
    }
    return true;
}

void EglOnXBackend::present()
{
    if (lastDamage().isEmpty())
        return;

    presentSurface(surface(), lastDamage(), screens()->geometry());

    setLastDamage(QRegion());
    if (!supportsBufferAge()) {
        eglWaitGL();
        xcb_flush(m_connection);
    }
}

void EglOnXBackend::presentSurface(EGLSurface surface, const QRegion &damage, const QRect &screenGeometry)
{
    if (damage.isEmpty()) {
        return;
    }
    const bool fullRepaint = supportsBufferAge() || (damage == screenGeometry);

    if (fullRepaint || !surfaceHasSubPost) {
        if (gs_tripleBufferNeedsDetection) {
            eglWaitGL();
            m_swapProfiler.begin();
        }
        // the entire screen changed, or we cannot do partial updates (which implies we enabled surface preservation)
        eglSwapBuffers(eglDisplay(), surface);
        if (gs_tripleBufferNeedsDetection) {
            eglWaitGL();
            if (char result = m_swapProfiler.end()) {
                gs_tripleBufferUndetected = gs_tripleBufferNeedsDetection = false;
                if (result == 'd' && GLPlatform::instance()->driver() == Driver_NVidia) {
                    // TODO this is a workaround, we should get __GL_YIELD set before libGL checks it
                    if (qstrcmp(qgetenv("__GL_YIELD"), "USLEEP")) {
                        options->setGlPreferBufferSwap(0);
                        eglSwapInterval(eglDisplay(), 0);
                        result = 0; // hint proper behavior
                        qCWarning(KWIN_CORE) << "\nIt seems you are using the nvidia driver without triple buffering\n"
                                          "You must export __GL_YIELD=\"USLEEP\" to prevent large CPU overhead on synced swaps\n"
                                          "Preferably, enable the TripleBuffer Option in the xorg.conf Device\n"
                                          "For this reason, the tearing prevention has been disabled.\n"
                                          "See https://bugs.kde.org/show_bug.cgi?id=322060\n";
                    }
                }
                setBlocksForRetrace(result == 'd');
            }
        }
        if (supportsBufferAge()) {
            eglQuerySurface(eglDisplay(), surface, EGL_BUFFER_AGE_EXT, &m_bufferAge);
        }
    } else {
        // a part of the screen changed, and we can use eglPostSubBufferNV to copy the updated area
        for (const QRect &r : damage) {
            eglPostSubBufferNV(eglDisplay(), surface, r.left(), screenGeometry.height() - r.bottom() - 1, r.width(), r.height());
        }
    }
}

void EglOnXBackend::screenGeometryChanged(const QSize &size)
{
    Q_UNUSED(size)

    // TODO: base implementation in OpenGLBackend

    // The back buffer contents are now undefined
    m_bufferAge = 0;
}

SceneOpenGLTexturePrivate *EglOnXBackend::createBackendTexture(SceneOpenGLTexture *texture)
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

    if (m_overlayWindow && overlayWindow()->window())  // show the window only after the first pass,
        overlayWindow()->show();   // since that pass may take long

    // Save the damaged region to history
    if (supportsBufferAge())
        addToDamageHistory(damagedRegion);
}

bool EglOnXBackend::usesOverlayWindow() const
{
    return m_usesOverlayWindow;
}

OverlayWindow* EglOnXBackend::overlayWindow() const
{
    return m_overlayWindow;
}

bool EglOnXBackend::makeContextCurrent(const EGLSurface &surface)
{
    return eglMakeCurrent(eglDisplay(), surface, surface, context()) == EGL_TRUE;
}

/************************************************
 * EglTexture
 ************************************************/

EglTexture::EglTexture(KWin::SceneOpenGLTexture *texture, KWin::EglOnXBackend *backend)
    : AbstractEglTexture(texture, backend)
    , m_backend(backend)
{
}

EglTexture::~EglTexture() = default;

bool EglTexture::loadTexture(WindowPixmap *pixmap)
{
    // first try the Wayland enabled loading
    if (AbstractEglTexture::loadTexture(pixmap)) {
        return true;
    }
    // did not succeed, try on X11
    return loadTexture(pixmap->pixmap(), pixmap->toplevel()->bufferGeometry().size());
}

bool EglTexture::loadTexture(xcb_pixmap_t pix, const QSize &size)
{
    if (!m_backend->isX11TextureFromPixmapSupported()) {
        return false;
    }

    if (pix == XCB_NONE)
        return false;

    glGenTextures(1, &m_texture);
    auto q = texture();
    q->setWrapMode(GL_CLAMP_TO_EDGE);
    q->setFilter(GL_LINEAR);
    q->bind();
    const EGLint attribs[] = {
        EGL_IMAGE_PRESERVED_KHR, EGL_TRUE,
        EGL_NONE
    };
    setImage(eglCreateImageKHR(m_backend->eglDisplay(), EGL_NO_CONTEXT, EGL_NATIVE_PIXMAP_KHR,
                               (EGLClientBuffer)pix, attribs));

    if (EGL_NO_IMAGE_KHR == image()) {
        qCDebug(KWIN_CORE) << "failed to create egl image";
        q->unbind();
        q->discard();
        return false;
    }
    glEGLImageTargetTexture2DOES(GL_TEXTURE_2D, (GLeglImageOES)image());
    q->unbind();
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
        glEGLImageTargetTexture2DOES(GL_TEXTURE_2D, (GLeglImageOES) image());
    }
    GLTexturePrivate::onDamage();
}

} // namespace
