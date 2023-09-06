/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2006-2007 Rivo Laks <rivolaks@hot.ee>
    SPDX-FileCopyrightText: 2010, 2011 Martin Gräßlin <mgraesslin@kde.org>
    SPDX-FileCopyrightText: 2023 Xaver Hugl <xaver.hugl@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "libkwineffects/glutils.h"
#include "glplatform.h"
#include "gltexture_p.h"
#include "logging_p.h"

namespace KWin
{

static QList<QByteArray> glExtensions;

// Functions

static void initDebugOutput()
{
    const bool have_KHR_debug = hasGLExtension(QByteArrayLiteral("GL_KHR_debug"));
    const bool have_ARB_debug = hasGLExtension(QByteArrayLiteral("GL_ARB_debug_output"));
    if (!have_KHR_debug && !have_ARB_debug) {
        return;
    }

    if (!have_ARB_debug) {
        // if we don't have ARB debug, but only KHR debug we need to verify whether the context is a debug context
        // it should work without as well, but empirical tests show: no it doesn't
        if (GLPlatform::instance()->isGLES()) {
            if (!hasGLVersion(3, 2)) {
                // empirical data shows extension doesn't work
                return;
            }
        } else if (!hasGLVersion(3, 0)) {
            return;
        }
        // can only be queried with either OpenGL >= 3.0 or OpenGL ES of at least 3.1
        GLint value = 0;
        glGetIntegerv(GL_CONTEXT_FLAGS, &value);
        if (!(value & GL_CONTEXT_FLAG_DEBUG_BIT)) {
            return;
        }
    }

    // Set the callback function
    auto callback = [](GLenum source, GLenum type, GLuint id,
                       GLenum severity, GLsizei length,
                       const GLchar *message,
                       const GLvoid *userParam) {
        while (length && std::isspace(message[length - 1])) {
            --length;
        }

        switch (type) {
        case GL_DEBUG_TYPE_ERROR:
        case GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR:
            qCWarning(LIBKWINGLUTILS, "%#x: %.*s", id, length, message);
            break;

        case GL_DEBUG_TYPE_OTHER:
        case GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR:
        case GL_DEBUG_TYPE_PORTABILITY:
        case GL_DEBUG_TYPE_PERFORMANCE:
        default:
            qCDebug(LIBKWINGLUTILS, "%#x: %.*s", id, length, message);
            break;
        }
    };

    glDebugMessageCallback(callback, nullptr);

    // This state exists only in GL_KHR_debug
    if (have_KHR_debug) {
        glEnable(GL_DEBUG_OUTPUT);
    }

    if (qEnvironmentVariableIntValue("KWIN_GL_DEBUG")) {
        // Enable all debug messages
        glDebugMessageControl(GL_DONT_CARE, GL_DONT_CARE, GL_DONT_CARE, 0, nullptr, GL_TRUE);
        // Insert a test message
        const QByteArray message = QByteArrayLiteral("OpenGL debug output initialized");
        glDebugMessageInsert(GL_DEBUG_SOURCE_APPLICATION, GL_DEBUG_TYPE_OTHER, 0,
                             GL_DEBUG_SEVERITY_LOW, message.length(), message.constData());
    } else {
        // Only enable error messages
        glDebugMessageControl(GL_DONT_CARE, GL_DEBUG_TYPE_ERROR, GL_DONT_CARE, 0, nullptr, GL_TRUE);
        glDebugMessageControl(GL_DONT_CARE, GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR, GL_DONT_CARE, 0, nullptr, GL_TRUE);
    }
}

void initGL(const std::function<resolveFuncPtr(const char *)> &resolveFunction)
{
    // Get list of supported OpenGL extensions
    if (hasGLVersion(3, 0)) {
        int count;
        glGetIntegerv(GL_NUM_EXTENSIONS, &count);

        for (int i = 0; i < count; i++) {
            const QByteArray name = (const char *)glGetStringi(GL_EXTENSIONS, i);
            glExtensions << name;
        }
    } else {
        glExtensions = QByteArray((const char *)glGetString(GL_EXTENSIONS)).split(' ');
    }

    // handle OpenGL extensions functions
    glResolveFunctions(resolveFunction);

    initDebugOutput();

    GLTexturePrivate::initStatic();
    GLFramebuffer::initStatic();
    GLVertexBuffer::initStatic();
}

void cleanupGL()
{
    ShaderManager::cleanup();
    GLTexturePrivate::cleanup();
    GLFramebuffer::cleanup();
    GLVertexBuffer::cleanup();
    GLPlatform::cleanup();

    glExtensions.clear();
}

bool hasGLVersion(int major, int minor, int release)
{
    return GLPlatform::instance()->glVersion() >= Version(major, minor, release);
}

bool hasGLExtension(const QByteArray &extension)
{
    return glExtensions.contains(extension);
}

QList<QByteArray> openGLExtensions()
{
    return glExtensions;
}

static QString formatGLError(GLenum err)
{
    switch (err) {
    case GL_NO_ERROR:
        return QStringLiteral("GL_NO_ERROR");
    case GL_INVALID_ENUM:
        return QStringLiteral("GL_INVALID_ENUM");
    case GL_INVALID_VALUE:
        return QStringLiteral("GL_INVALID_VALUE");
    case GL_INVALID_OPERATION:
        return QStringLiteral("GL_INVALID_OPERATION");
    case GL_STACK_OVERFLOW:
        return QStringLiteral("GL_STACK_OVERFLOW");
    case GL_STACK_UNDERFLOW:
        return QStringLiteral("GL_STACK_UNDERFLOW");
    case GL_OUT_OF_MEMORY:
        return QStringLiteral("GL_OUT_OF_MEMORY");
    default:
        return QLatin1String("0x") + QString::number(err, 16);
    }
}

bool checkGLError(const char *txt)
{
    GLenum err = glGetError();
    if (err == GL_CONTEXT_LOST) {
        qCWarning(LIBKWINGLUTILS) << "GL error: context lost";
        return true;
    }
    bool hasError = false;
    while (err != GL_NO_ERROR) {
        qCWarning(LIBKWINGLUTILS) << "GL error (" << txt << "): " << formatGLError(err);
        hasError = true;
        err = glGetError();
        if (err == GL_CONTEXT_LOST) {
            qCWarning(LIBKWINGLUTILS) << "GL error: context lost";
            break;
        }
    }
    return hasError;
}

} // namespace
