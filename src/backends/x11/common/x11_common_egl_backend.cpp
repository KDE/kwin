/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2010, 2012 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "x11_common_egl_backend.h"
// kwineffects
#include <kwinglutils.h>
// kwin
#include "core/outputbackend.h"
#include "main.h"
#include "utils/common.h"
#include "utils/xcbutils.h"
// X11
#include <X11/Xlib-xcb.h>
#include <fixx11h.h>

namespace KWin
{

EglOnXBackend::EglOnXBackend(xcb_connection_t *connection, Display *display, xcb_window_t rootWindow)
    : AbstractEglBackend()
    , surfaceHasSubPost(0)
    , m_connection(connection)
    , m_x11Display(display)
    , m_rootWindow(rootWindow)
{
    // Egl is always direct rendering
    setIsDirectRendering(true);
}

void EglOnXBackend::init()
{
    qputenv("EGL_PLATFORM", "x11");
    if (!initRenderingContext()) {
        setFailed(QStringLiteral("Could not initialize rendering context"));
        return;
    }

    initKWinGL();
    if (!hasExtension(QByteArrayLiteral("EGL_KHR_image")) && (!hasExtension(QByteArrayLiteral("EGL_KHR_image_base")) || !hasExtension(QByteArrayLiteral("EGL_KHR_image_pixmap")))) {
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

    if (surfaceHasSubPost) {
        qCDebug(KWIN_CORE) << "EGL implementation and surface support eglPostSubBufferNV, let's use it";

        // check if swap interval 1 is supported
        EGLint val;
        eglGetConfigAttrib(eglDisplay(), config(), EGL_MAX_SWAP_INTERVAL, &val);
        if (val >= 1) {
            if (eglSwapInterval(eglDisplay(), 1)) {
                qCDebug(KWIN_CORE) << "Enabled v-sync";
            }
        } else {
            qCWarning(KWIN_CORE) << "Cannot enable v-sync as max. swap interval is" << val;
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
}

bool EglOnXBackend::initRenderingContext()
{
    initClientExtensions();
    EGLDisplay dpy = kwinApp()->outputBackend()->sceneEglDisplay();

    // Use eglGetPlatformDisplayEXT() to get the display pointer
    // if the implementation supports it.
    if (dpy == EGL_NO_DISPLAY) {
        const bool havePlatformBase = hasClientExtension(QByteArrayLiteral("EGL_EXT_platform_base"));
        setHavePlatformBase(havePlatformBase);
        if (havePlatformBase) {
            // Make sure that the X11 platform is supported
            if (!hasClientExtension(QByteArrayLiteral("EGL_EXT_platform_x11")) && !hasClientExtension(QByteArrayLiteral("EGL_KHR_platform_x11"))) {
                qCWarning(KWIN_CORE) << "EGL_EXT_platform_base is supported, but neither EGL_EXT_platform_x11 nor EGL_KHR_platform_x11 is supported."
                                     << "Cannot create EGLDisplay on X11";
                return false;
            }

            dpy = eglGetPlatformDisplayEXT(EGL_PLATFORM_X11_EXT, m_x11Display, nullptr);
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

EGLSurface EglOnXBackend::createSurface(xcb_window_t window)
{
    if (window == XCB_WINDOW_NONE) {
        return EGL_NO_SURFACE;
    }

    // Window is 64 bits on a 64-bit architecture whereas xcb_window_t is always 32 bits.
    ::Window nativeWindow = window;

    EGLSurface surface = EGL_NO_SURFACE;
    if (havePlatformBase()) {
        // eglCreatePlatformWindowSurfaceEXT() expects a pointer to the Window.
        surface = eglCreatePlatformWindowSurfaceEXT(eglDisplay(), config(), (void *)&nativeWindow, nullptr);
    } else {
        // eglCreateWindowSurface() expects a Window, not a pointer to the Window. Use
        // a c style cast as there are (buggy) platforms where the size of the Window
        // type is not the same as the size of EGLNativeWindowType, reinterpret_cast<>()
        // may not compile.
        surface = eglCreateWindowSurface(eglDisplay(), config(), (EGLNativeWindowType)(uintptr_t)nativeWindow, nullptr);
    }

    return surface;
}

bool EglOnXBackend::initBufferConfigs()
{
    initBufferAge();
    const EGLint config_attribs[] = {
        EGL_SURFACE_TYPE,
        EGL_WINDOW_BIT | (supportsBufferAge() ? 0 : EGL_SWAP_BEHAVIOR_PRESERVED_BIT),
        EGL_RED_SIZE,
        1,
        EGL_GREEN_SIZE,
        1,
        EGL_BLUE_SIZE,
        1,
        EGL_ALPHA_SIZE,
        0,
        EGL_RENDERABLE_TYPE,
        isOpenGLES() ? EGL_OPENGL_ES2_BIT : EGL_OPENGL_BIT,
        EGL_CONFIG_CAVEAT,
        EGL_NONE,
        EGL_NONE,
    };

    EGLint count;
    EGLConfig configs[1024];
    if (eglChooseConfig(eglDisplay(), config_attribs, configs, 1024, &count) == EGL_FALSE) {
        qCCritical(KWIN_CORE) << "choose config failed";
        return false;
    }

    UniqueCPtr<xcb_get_window_attributes_reply_t> attribs(xcb_get_window_attributes_reply(m_connection,
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

bool EglOnXBackend::makeContextCurrent(const EGLSurface &surface)
{
    return eglMakeCurrent(eglDisplay(), surface, surface, context()) == EGL_TRUE;
}

} // namespace
