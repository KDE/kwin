/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2015 Martin Gräßlin <mgraesslin@kde.org>
    SPDX-FileCopyrightText: 2020 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "eglplatformcontext.h"
#include "core/outputbackend.h"
#include "eglhelpers.h"
#include "internalwindow.h"
#include "offscreensurface.h"
#include "platformsupport/scenes/opengl/eglcontext.h"
#include "platformsupport/scenes/opengl/egldisplay.h"
#include "utils/egl_context_attribute_builder.h"
#include "window.h"

#include "logging.h"

#include <QOpenGLContext>
#include <QOpenGLFramebufferObject>

#include <private/qopenglcontext_p.h>

#include <memory>

namespace KWin
{
namespace QPA
{

EGLPlatformContext::EGLPlatformContext(QOpenGLContext *context, EglDisplay *display)
    : m_eglDisplay(display)
{
    create(context->format(), kwinApp()->outputBackend()->sceneEglGlobalShareContext());
}

EGLPlatformContext::~EGLPlatformContext() = default;

static EGLSurface eglSurfaceForPlatformSurface(QPlatformSurface *surface)
{
    if (surface->surface()->surfaceClass() == QSurface::Window) {
        return static_cast<Window *>(surface)->eglSurface();
    } else {
        return static_cast<OffscreenSurface *>(surface)->eglSurface();
    }
}

bool EGLPlatformContext::makeCurrent(QPlatformSurface *surface)
{
    const bool ok = m_eglContext->makeCurrent(eglSurfaceForPlatformSurface(surface));
    if (!ok) {
        qCWarning(KWIN_QPA, "eglMakeCurrent failed: %x", eglGetError());
        return false;
    }

    if (surface->surface()->surfaceClass() == QSurface::Window) {
        // QOpenGLContextPrivate::setCurrentContext will be called after this
        // method returns, but that's too late, as we need a current context in
        // order to bind the content framebuffer object.
        QOpenGLContextPrivate::setCurrentContext(context());

        Window *window = static_cast<Window *>(surface);
        window->bindContentFBO();
    }

    return true;
}

void EGLPlatformContext::doneCurrent()
{
    m_eglContext->doneCurrent();
}

bool EGLPlatformContext::isValid() const
{
    return m_eglContext != nullptr;
}

bool EGLPlatformContext::isSharing() const
{
    return false;
}

QSurfaceFormat EGLPlatformContext::format() const
{
    return m_format;
}

QFunctionPointer EGLPlatformContext::getProcAddress(const char *procName)
{
    return eglGetProcAddress(procName);
}

void EGLPlatformContext::swapBuffers(QPlatformSurface *surface)
{
    if (surface->surface()->surfaceClass() == QSurface::Window) {
        Window *window = static_cast<Window *>(surface);
        InternalWindow *internalWindow = window->internalWindow();
        if (!internalWindow) {
            return;
        }
        glFlush();
        auto fbo = window->swapFBO();
        window->bindContentFBO();
        internalWindow->present(fbo);
    }
}

GLuint EGLPlatformContext::defaultFramebufferObject(QPlatformSurface *surface) const
{
    if (Window *window = dynamic_cast<Window *>(surface)) {
        const auto &fbo = window->contentFBO();
        if (fbo) {
            return fbo->handle();
        }
        qCDebug(KWIN_QPA) << "No default framebuffer object for internal window";
    }

    return 0;
}

void EGLPlatformContext::create(const QSurfaceFormat &format, ::EGLContext shareContext)
{
    if (!eglBindAPI(isOpenGLES() ? EGL_OPENGL_ES_API : EGL_OPENGL_API)) {
        qCWarning(KWIN_QPA, "eglBindAPI failed: 0x%x", eglGetError());
        return;
    }

    m_config = configFromFormat(m_eglDisplay, format);
    if (m_config == EGL_NO_CONFIG_KHR) {
        qCWarning(KWIN_QPA) << "Could not find suitable EGLConfig for" << format;
        return;
    }

    m_format = formatFromConfig(m_eglDisplay, m_config);
    m_eglContext = EglContext::create(m_eglDisplay, m_config, shareContext);
    if (!m_eglContext) {
        qCWarning(KWIN_QPA) << "Failed to create EGL context";
        return;
    }
    updateFormatFromContext();
}

void EGLPlatformContext::updateFormatFromContext()
{
    const EGLSurface oldDrawSurface = eglGetCurrentSurface(EGL_DRAW);
    const EGLSurface oldReadSurface = eglGetCurrentSurface(EGL_READ);
    const ::EGLContext oldContext = eglGetCurrentContext();

    m_eglContext->makeCurrent();

    const char *version = reinterpret_cast<const char *>(glGetString(GL_VERSION));
    int major, minor;
    if (parseOpenGLVersion(version, major, minor)) {
        m_format.setMajorVersion(major);
        m_format.setMinorVersion(minor);
    } else {
        qCWarning(KWIN_QPA) << "Unrecognized OpenGL version:" << version;
    }

    GLint value;

    if (m_format.version() >= qMakePair(3, 0)) {
        glGetIntegerv(GL_CONTEXT_FLAGS, &value);
        if (value & GL_CONTEXT_FLAG_FORWARD_COMPATIBLE_BIT) {
            m_format.setOption(QSurfaceFormat::DeprecatedFunctions);
        }
        if (value & GL_CONTEXT_FLAG_DEBUG_BIT) {
            m_format.setOption(QSurfaceFormat::DebugContext);
        }
    } else {
        m_format.setOption(QSurfaceFormat::DeprecatedFunctions);
    }

    if (m_format.version() >= qMakePair(3, 2)) {
        glGetIntegerv(GL_CONTEXT_PROFILE_MASK, &value);
        if (value & GL_CONTEXT_CORE_PROFILE_BIT) {
            m_format.setProfile(QSurfaceFormat::CoreProfile);
        } else if (value & GL_CONTEXT_COMPATIBILITY_PROFILE_BIT) {
            m_format.setProfile(QSurfaceFormat::CompatibilityProfile);
        } else {
            m_format.setProfile(QSurfaceFormat::NoProfile);
        }
    } else {
        m_format.setProfile(QSurfaceFormat::NoProfile);
    }

    eglMakeCurrent(m_eglDisplay->handle(), oldDrawSurface, oldReadSurface, oldContext);
}

} // namespace QPA
} // namespace KWin
