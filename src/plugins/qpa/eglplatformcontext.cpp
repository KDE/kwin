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

EGLPlatformContext::EGLPlatformContext(QOpenGLContext *context, EGLDisplay display)
    : m_eglDisplay(display)
{
    create(context->format(), kwinApp()->outputBackend()->sceneEglGlobalShareContext());
}

EGLPlatformContext::~EGLPlatformContext()
{
    if (m_context != EGL_NO_CONTEXT) {
        eglDestroyContext(m_eglDisplay, m_context);
    }
}

EGLDisplay EGLPlatformContext::eglDisplay() const
{
    return m_eglDisplay;
}

EGLContext EGLPlatformContext::eglContext() const
{
    return m_context;
}

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
    const EGLSurface eglSurface = eglSurfaceForPlatformSurface(surface);

    const bool ok = eglMakeCurrent(eglDisplay(), eglSurface, eglSurface, eglContext());
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
    eglMakeCurrent(m_eglDisplay, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
}

bool EGLPlatformContext::isValid() const
{
    return m_context != EGL_NO_CONTEXT;
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

void EGLPlatformContext::create(const QSurfaceFormat &format, EGLContext shareContext)
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

    const QByteArray eglExtensions = eglQueryString(eglDisplay(), EGL_EXTENSIONS);
    const QList<QByteArray> extensions = eglExtensions.split(' ');
    const bool haveRobustness = extensions.contains(QByteArrayLiteral("EGL_EXT_create_context_robustness"));
    const bool haveCreateContext = extensions.contains(QByteArrayLiteral("EGL_KHR_create_context"));
    const bool haveContextPriority = extensions.contains(QByteArrayLiteral("EGL_IMG_context_priority"));

    std::vector<std::unique_ptr<AbstractOpenGLContextAttributeBuilder>> candidates;
    if (isOpenGLES()) {
        if (haveCreateContext && haveRobustness && haveContextPriority) {
            auto glesRobustPriority = std::make_unique<EglOpenGLESContextAttributeBuilder>();
            glesRobustPriority->setVersion(2);
            glesRobustPriority->setRobust(true);
            glesRobustPriority->setHighPriority(true);
            candidates.push_back(std::move(glesRobustPriority));
        }
        if (haveCreateContext && haveRobustness) {
            auto glesRobust = std::make_unique<EglOpenGLESContextAttributeBuilder>();
            glesRobust->setVersion(2);
            glesRobust->setRobust(true);
            candidates.push_back(std::move(glesRobust));
        }
        if (haveContextPriority) {
            auto glesPriority = std::make_unique<EglOpenGLESContextAttributeBuilder>();
            glesPriority->setVersion(2);
            glesPriority->setHighPriority(true);
            candidates.push_back(std::move(glesPriority));
        }
        auto gles = std::make_unique<EglOpenGLESContextAttributeBuilder>();
        gles->setVersion(2);
        candidates.push_back(std::move(gles));
    } else {
        // Try to create a 3.1 core context
        if (m_format.majorVersion() >= 3 && haveCreateContext) {
            if (haveRobustness && haveContextPriority) {
                auto robustCorePriority = std::make_unique<EglContextAttributeBuilder>();
                robustCorePriority->setVersion(m_format.majorVersion(), m_format.minorVersion());
                robustCorePriority->setRobust(true);
                robustCorePriority->setForwardCompatible(true);
                if (m_format.profile() == QSurfaceFormat::CoreProfile) {
                    robustCorePriority->setCoreProfile(true);
                } else if (m_format.profile() == QSurfaceFormat::CompatibilityProfile) {
                    robustCorePriority->setCompatibilityProfile(true);
                }
                robustCorePriority->setHighPriority(true);
                candidates.push_back(std::move(robustCorePriority));
            }
            if (haveRobustness) {
                auto robustCore = std::make_unique<EglContextAttributeBuilder>();
                robustCore->setVersion(m_format.majorVersion(), m_format.minorVersion());
                robustCore->setRobust(true);
                robustCore->setForwardCompatible(true);
                if (m_format.profile() == QSurfaceFormat::CoreProfile) {
                    robustCore->setCoreProfile(true);
                } else if (m_format.profile() == QSurfaceFormat::CompatibilityProfile) {
                    robustCore->setCompatibilityProfile(true);
                }
                candidates.push_back(std::move(robustCore));
            }
            if (haveContextPriority) {
                auto corePriority = std::make_unique<EglContextAttributeBuilder>();
                corePriority->setVersion(m_format.majorVersion(), m_format.minorVersion());
                corePriority->setForwardCompatible(true);
                if (m_format.profile() == QSurfaceFormat::CoreProfile) {
                    corePriority->setCoreProfile(true);
                } else if (m_format.profile() == QSurfaceFormat::CompatibilityProfile) {
                    corePriority->setCompatibilityProfile(true);
                }
                corePriority->setHighPriority(true);
                candidates.push_back(std::move(corePriority));
            }
            auto core = std::make_unique<EglContextAttributeBuilder>();
            core->setVersion(m_format.majorVersion(), m_format.minorVersion());
            core->setForwardCompatible(true);
            if (m_format.profile() == QSurfaceFormat::CoreProfile) {
                core->setCoreProfile(true);
            } else if (m_format.profile() == QSurfaceFormat::CompatibilityProfile) {
                core->setCompatibilityProfile(true);
            }
            candidates.push_back(std::move(core));
        }
        if (haveRobustness && haveCreateContext && haveContextPriority) {
            auto robustPriority = std::make_unique<EglContextAttributeBuilder>();
            robustPriority->setRobust(true);
            robustPriority->setHighPriority(true);
            candidates.push_back(std::move(robustPriority));
        }
        if (haveRobustness && haveCreateContext) {
            auto robust = std::make_unique<EglContextAttributeBuilder>();
            robust->setRobust(true);
            candidates.push_back(std::move(robust));
        }
        candidates.emplace_back(new EglContextAttributeBuilder);
    }

    EGLContext context = EGL_NO_CONTEXT;
    for (auto it = candidates.begin(); it != candidates.end(); it++) {
        const auto attribs = (*it)->build();
        context = eglCreateContext(eglDisplay(), m_config, shareContext, attribs.data());
        if (context != EGL_NO_CONTEXT) {
            qCDebug(KWIN_QPA) << "Created EGL context with attributes:" << (*it).get();
            break;
        }
    }

    if (context == EGL_NO_CONTEXT) {
        qCWarning(KWIN_QPA) << "Failed to create EGL context";
        return;
    }
    m_context = context;
    updateFormatFromContext();
}

void EGLPlatformContext::updateFormatFromContext()
{
    const EGLSurface oldDrawSurface = eglGetCurrentSurface(EGL_DRAW);
    const EGLSurface oldReadSurface = eglGetCurrentSurface(EGL_READ);
    const EGLContext oldContext = eglGetCurrentContext();

    eglMakeCurrent(m_eglDisplay, EGL_NO_SURFACE, EGL_NO_SURFACE, m_context);

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

    eglMakeCurrent(m_eglDisplay, oldDrawSurface, oldReadSurface, oldContext);
}

} // namespace QPA
} // namespace KWin
