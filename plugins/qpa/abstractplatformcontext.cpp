/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2015 Martin Gräßlin <mgraesslin@kde.org>

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
#include "abstractplatformcontext.h"
#include "integration.h"
#include <logging.h>

namespace KWin
{

namespace QPA
{

static bool isOpenGLES()
{
    if (qstrcmp(qgetenv("KWIN_COMPOSE"), "O2ES") == 0) {
        return true;
    }
    return QOpenGLContext::openGLModuleType() == QOpenGLContext::LibGLES;
}

static EGLConfig configFromGLFormat(EGLDisplay dpy, const QSurfaceFormat &format)
{
#define SIZE( __buffer__ ) format.__buffer__##BufferSize() > 0 ? format.__buffer__##BufferSize() : 0
    // not setting samples as QtQuick doesn't need it
    const EGLint config_attribs[] = {
        EGL_SURFACE_TYPE,         EGL_WINDOW_BIT,
        EGL_RED_SIZE,             SIZE(red),
        EGL_GREEN_SIZE,           SIZE(green),
        EGL_BLUE_SIZE,            SIZE(blue),
        EGL_ALPHA_SIZE,           SIZE(alpha),
        EGL_DEPTH_SIZE,           SIZE(depth),
        EGL_STENCIL_SIZE,         SIZE(stencil),
        EGL_RENDERABLE_TYPE,      isOpenGLES() ? EGL_OPENGL_ES2_BIT : EGL_OPENGL_BIT,
        EGL_NONE,
    };
    qCDebug(KWIN_QPA) << "Trying to find a format with: rgba/depth/stencil" << (SIZE(red)) << (SIZE(green)) <<( SIZE(blue)) << (SIZE(alpha)) << (SIZE(depth)) << (SIZE(stencil));
#undef SIZE

    EGLint count;
    EGLConfig configs[1024];
    if (eglChooseConfig(dpy, config_attribs, configs, 1, &count) == EGL_FALSE) {
        qCWarning(KWIN_QPA) << "eglChooseConfig failed";
        return 0;
    }
    if (count != 1) {
        qCWarning(KWIN_QPA) << "eglChooseConfig did not return any configs";
        return 0;
    }
    return configs[0];
}

static QSurfaceFormat formatFromConfig(EGLDisplay dpy, EGLConfig config)
{
    QSurfaceFormat format;
    EGLint value = 0;
#define HELPER(__egl__, __qt__) \
    eglGetConfigAttrib(dpy, config, EGL_##__egl__, &value); \
    format.set##__qt__(value); \
    value = 0;

#define BUFFER_HELPER(__eglColor__, __color__) \
    HELPER(__eglColor__##_SIZE, __color__##BufferSize)

    BUFFER_HELPER(RED, Red)
    BUFFER_HELPER(GREEN, Green)
    BUFFER_HELPER(BLUE, Blue)
    BUFFER_HELPER(ALPHA, Alpha)
    BUFFER_HELPER(STENCIL, Stencil)
    BUFFER_HELPER(DEPTH, Depth)
#undef BUFFER_HELPER
    HELPER(SAMPLES, Samples)
#undef HELPER
    format.setRenderableType(isOpenGLES() ? QSurfaceFormat::OpenGLES : QSurfaceFormat::OpenGL);
    format.setStereo(false);

    return format;
}

AbstractPlatformContext::AbstractPlatformContext(QOpenGLContext *context, Integration *integration, EGLDisplay display)
    : QPlatformOpenGLContext()
    , m_integration(integration)
    , m_eglDisplay(display)
    , m_config(configFromGLFormat(m_eglDisplay, context->format()))
    , m_format(formatFromConfig(m_eglDisplay, m_config))
{
}

AbstractPlatformContext::~AbstractPlatformContext()
{
    if (m_context != EGL_NO_CONTEXT) {
        eglDestroyContext(m_eglDisplay, m_context);
    }
}

void AbstractPlatformContext::doneCurrent()
{
    eglMakeCurrent(m_eglDisplay, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
}

QSurfaceFormat AbstractPlatformContext::format() const
{
    return m_format;
}

#if (QT_VERSION < QT_VERSION_CHECK(5, 7, 0))
QFunctionPointer AbstractPlatformContext::getProcAddress(const QByteArray &procName)
{
    return eglGetProcAddress(procName.constData());
}
#else
QFunctionPointer AbstractPlatformContext::getProcAddress(const char *procName)
{
    return eglGetProcAddress(procName);
}
#endif

bool AbstractPlatformContext::isValid() const
{
    return m_context != EGL_NO_CONTEXT;
}

bool AbstractPlatformContext::bindApi()
{
    if (eglBindAPI(isOpenGLES() ? EGL_OPENGL_ES_API : EGL_OPENGL_API) == EGL_FALSE) {
        qCWarning(KWIN_QPA) << "eglBindAPI failed";
        return false;
    }
    return true;
}

void AbstractPlatformContext::createContext(EGLContext shareContext)
{
    const QByteArray eglExtensions = eglQueryString(eglDisplay(), EGL_EXTENSIONS);
    const QList<QByteArray> extensions = eglExtensions.split(' ');
    const bool haveRobustness = extensions.contains(QByteArrayLiteral("EGL_EXT_create_context_robustness"));
    const bool haveCreateContext = extensions.contains(QByteArrayLiteral("EGL_KHR_create_context"));

    EGLContext context = EGL_NO_CONTEXT;
    if (isOpenGLES()) {
        if (haveCreateContext && haveRobustness) {
            const EGLint context_attribs[] = {
                EGL_CONTEXT_CLIENT_VERSION,                         2,
                EGL_CONTEXT_OPENGL_ROBUST_ACCESS_EXT,               EGL_TRUE,
                EGL_CONTEXT_OPENGL_RESET_NOTIFICATION_STRATEGY_EXT, EGL_LOSE_CONTEXT_ON_RESET_EXT,
                EGL_NONE
            };
            context = eglCreateContext(eglDisplay(), config(), shareContext, context_attribs);
        }
        if (context == EGL_NO_CONTEXT) {
            const EGLint context_attribs[] = {
                EGL_CONTEXT_CLIENT_VERSION, 2,
                EGL_NONE
            };

            context = eglCreateContext(eglDisplay(), config(), shareContext, context_attribs);
        }
    } else {
        // Try to create a 3.1 core context
        if (m_format.majorVersion() >= 3 && haveCreateContext) {
            if (haveRobustness) {
                const int attribs[] = {
                    EGL_CONTEXT_MAJOR_VERSION_KHR,                      m_format.majorVersion(),
                    EGL_CONTEXT_MINOR_VERSION_KHR,                      m_format.minorVersion(),
                    EGL_CONTEXT_OPENGL_RESET_NOTIFICATION_STRATEGY_KHR, EGL_LOSE_CONTEXT_ON_RESET_KHR,
                    EGL_CONTEXT_FLAGS_KHR,                              EGL_CONTEXT_OPENGL_ROBUST_ACCESS_BIT_KHR | EGL_CONTEXT_OPENGL_FORWARD_COMPATIBLE_BIT_KHR,
                    EGL_CONTEXT_OPENGL_PROFILE_MASK_KHR,                m_format.profile() == QSurfaceFormat::CoreProfile ? EGL_CONTEXT_OPENGL_CORE_PROFILE_BIT_KHR : EGL_CONTEXT_OPENGL_COMPATIBILITY_PROFILE_BIT_KHR,
                    EGL_NONE
                };
                context = eglCreateContext(eglDisplay(), config(), shareContext, attribs);
            }
            if (context == EGL_NO_CONTEXT) {
                // try without robustness
                const EGLint attribs[] = {
                    EGL_CONTEXT_MAJOR_VERSION_KHR, m_format.majorVersion(),
                    EGL_CONTEXT_MINOR_VERSION_KHR, m_format.minorVersion(),
                    EGL_CONTEXT_FLAGS_KHR,         EGL_CONTEXT_OPENGL_FORWARD_COMPATIBLE_BIT_KHR,
                    EGL_CONTEXT_OPENGL_PROFILE_MASK_KHR, m_format.profile() == QSurfaceFormat::CoreProfile ? EGL_CONTEXT_OPENGL_CORE_PROFILE_BIT_KHR : EGL_CONTEXT_OPENGL_COMPATIBILITY_PROFILE_BIT_KHR,
                    EGL_NONE
                };
                context = eglCreateContext(eglDisplay(), config(), shareContext, attribs);
            }
        }

        if (context == EGL_NO_CONTEXT && haveRobustness && haveCreateContext) {
            const int attribs[] = {
                EGL_CONTEXT_FLAGS_KHR, EGL_CONTEXT_OPENGL_ROBUST_ACCESS_BIT_KHR,
                EGL_CONTEXT_OPENGL_RESET_NOTIFICATION_STRATEGY_KHR, EGL_LOSE_CONTEXT_ON_RESET_KHR,
                EGL_NONE
            };
            context = eglCreateContext(eglDisplay(), config(), shareContext, attribs);
        }
        if (context == EGL_NO_CONTEXT) {
            // and last but not least: try without robustness
            const EGLint attribs[] = {
                EGL_NONE
            };
            context = eglCreateContext(eglDisplay(), config(), shareContext, attribs);
        }
    }

    if (context == EGL_NO_CONTEXT) {
        qCWarning(KWIN_QPA) << "Failed to create EGL context";
        return;
    }
    m_context = context;
}

}
}
