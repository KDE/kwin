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
#include "opengl/eglcontext.h"
#include "opengl/egldisplay.h"
#include "opengl/glutils.h"
#include "swapchain.h"
#include "window.h"

#include "logging.h"

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

EGLPlatformContext::EGLPlatformContext(QOpenGLContext *context, EglDisplay *display)
    : m_eglDisplay(display)
{
    create(context->format(), kwinApp()->outputBackend()->sceneEglGlobalShareContext());
}

EGLPlatformContext::~EGLPlatformContext()
{
    if (!m_eglContext) {
        return;
    }
    if (!m_renderTargets.empty() || !m_zombieRenderTargets.empty()) {
        m_eglContext->makeCurrent();
        m_renderTargets.clear();
        m_zombieRenderTargets.clear();
    }
}

bool EGLPlatformContext::makeCurrent(QPlatformSurface *surface)
{
    if (!m_eglContext) {
        return false;
    }
    const bool ok = m_eglContext->makeCurrent();
    if (!ok) {
        qCWarning(KWIN_QPA, "eglMakeCurrent failed: %x", eglGetError());
        return false;
    }
    if (m_eglContext->checkGraphicsResetStatus() != GL_NO_ERROR) {
        m_renderTargets.clear();
        m_zombieRenderTargets.clear();
        m_eglContext.reset();
        return false;
    }

    m_zombieRenderTargets.clear();

    if (surface->surface()->surfaceClass() == QSurface::Window) {
        Window *window = static_cast<Window *>(surface);
        Swapchain *swapchain = window->swapchain(m_eglContext, m_eglDisplay->nonExternalOnlySupportedDrmFormats());
        if (!swapchain) {
            return false;
        }

        GraphicsBuffer *buffer = swapchain->acquire();
        if (!buffer) {
            return false;
        }

        auto it = m_renderTargets.find(buffer);
        if (it != m_renderTargets.end()) {
            m_current = it->second;
        } else {
            std::shared_ptr<GLTexture> texture = m_eglContext->importDmaBufAsTexture(*buffer->dmabufAttributes());
            if (!texture) {
                return false;
            }

            std::unique_ptr<GLFramebuffer> fbo = std::make_unique<GLFramebuffer>(texture.get(), GLFramebuffer::CombinedDepthStencil);
            if (!fbo->valid()) {
                return false;
            }

            auto target = std::make_shared<EGLRenderTarget>(buffer, std::move(fbo), std::move(texture));
            m_renderTargets[buffer] = target;
            QObject::connect(buffer, &QObject::destroyed, this, [this, buffer]() {
                if (auto it = m_renderTargets.find(buffer); it != m_renderTargets.end()) {
                    m_zombieRenderTargets.push_back(std::move(it->second));
                    m_renderTargets.erase(it);
                }
            });

            m_current = target;
        }

        glBindFramebuffer(GL_FRAMEBUFFER, m_current->fbo->handle());
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

        glFlush(); // We need to flush pending rendering commands manually

        internalWindow->present(InternalWindowFrame{
            .buffer = m_current->buffer,
            .bufferDamage = QRect(QPoint(0, 0), m_current->buffer->size()),
            .bufferTransform = OutputTransform::FlipY,
        });

        m_current.reset();
    }
}

GLuint EGLPlatformContext::defaultFramebufferObject(QPlatformSurface *surface) const
{
    if (surface->surface()->surfaceClass() == QSurface::Window) {
        if (m_current) {
            return m_current->fbo->handle();
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

    eglMakeCurrent(m_eglDisplay->handle(), oldDrawSurface, oldReadSurface, oldContext);
}

} // namespace QPA
} // namespace KWin
