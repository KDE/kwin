/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2015 Martin Gräßlin <mgraesslin@kde.org>
    SPDX-FileCopyrightText: 2020 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "eglplatformcontext.h"
#include "compositor.h"
#include "core/outputbackend.h"
#include "core/syncobjtimeline.h"
#include "internalwindow.h"
#include "logging.h"
#include "offscreensurface.h"
#include "opengl/eglcontext.h"
#include "opengl/egldisplay.h"
#include "opengl/eglnativefence.h"
#include "opengl/eglutils_p.h"
#include "opengl/glutils.h"
#include "swapchain.h"
#include "window.h"

namespace KWin
{
namespace QPA
{

EGLRenderTarget::EGLRenderTarget(GraphicsBuffer *buffer, std::unique_ptr<GLFramebuffer> fbo, std::shared_ptr<GLTexture> texture)
    : buffer(buffer)
    , fbo(std::move(fbo))
    , texture(std::move(texture))
{
}

EGLRenderTarget::~EGLRenderTarget()
{
    fbo.reset();
    texture.reset();
}

EGLPlatformContext::EGLPlatformContext(QOpenGLContext *context, const std::shared_ptr<EglContext> &kwinContext)
    : m_eglDisplay(kwinContext->displayObject())
    , m_eglContext(kwinContext)
{
    updateFormatFromContext();
}

EGLPlatformContext::~EGLPlatformContext()
{
}

bool EGLPlatformContext::makeCurrent(QPlatformSurface *surface)
{
    if (!isValid()) {
        return false;
    }
    const bool ok = m_eglContext->makeCurrent();
    if (!ok) {
        m_current = nullptr;
        m_eglContext.reset();
        return false;
    }

    if (surface->surface()->surfaceClass() == QSurface::Window) {
        Window *window = static_cast<Window *>(surface);
        Swapchain *swapchain = window->swapchain(m_eglContext, m_eglDisplay->nonExternalOnlySupportedDrmFormats());
        if (!swapchain) {
            return false;
        }

        m_currentSlot = swapchain->acquire();
        if (!m_currentSlot) {
            return false;
        }
        GraphicsBuffer *buffer = m_currentSlot->buffer();
        GLFramebuffer *fbo = m_eglContext->importDmaBufAsFbo(buffer);
        if (!fbo) {
            return false;
        }
        m_current = fbo;

        glBindFramebuffer(GL_FRAMEBUFFER, m_current->handle());
    }

    return true;
}

void EGLPlatformContext::doneCurrent()
{
    if (m_eglContext) {
        m_eglContext->doneCurrent();
    }
}

bool EGLPlatformContext::isValid() const
{
    return m_eglContext != nullptr && !m_markedInvalid;
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
        if (!internalWindow || !m_currentSlot) {
            return;
        }

        const auto currentSlot = std::exchange(m_currentSlot, nullptr);
        m_current = nullptr;

        EGLNativeFence fence(EglContext::currentContext()->displayObject());
        currentSlot->releasePoint()->addReleaseFence(fence.fileDescriptor());

        internalWindow->present(InternalWindowFrame{
            .buffer = currentSlot->buffer(),
            .syncFd = fence.takeFileDescriptor(),
            .bufferDamage = Rect(QPoint(0, 0), currentSlot->buffer()->size()),
            .bufferTransform = OutputTransform::FlipY,
        });
    }
}

GLuint EGLPlatformContext::defaultFramebufferObject(QPlatformSurface *surface) const
{
    if (surface->surface()->surfaceClass() == QSurface::Window) {
        if (m_current) {
            return m_current->handle();
        }
        qCDebug(KWIN_QPA) << "No default framebuffer object for internal window";
    }

    return 0;
}

void EGLPlatformContext::updateFormatFromContext()
{
    if (!m_eglContext->makeCurrent()) {
        return;
    }

    m_format.setRenderableType(QSurfaceFormat::OpenGLES);

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
        if (!(value & GL_CONTEXT_FLAG_FORWARD_COMPATIBLE_BIT)) {
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
}

EGLContext EGLPlatformContext::nativeContext() const
{
    return m_eglContext->handle();
}

EGLConfig EGLPlatformContext::config() const
{
    return m_eglContext->config();
}

EGLDisplay EGLPlatformContext::display() const
{
    return m_eglContext->displayObject()->handle();
}

void EGLPlatformContext::invalidateContext()
{
    m_markedInvalid = true;
    m_currentSlot.reset();
    m_eglContext.reset();
}

} // namespace QPA
} // namespace KWin
