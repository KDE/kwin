
#include "egldisplay.h"
#include "utils/common.h"

#include <QOpenGLContext>

namespace KWin
{

static bool isOpenGLES()
{
    if (qstrcmp(qgetenv("KWIN_COMPOSE"), "O2ES") == 0) {
        return true;
    }
    return QOpenGLContext::openGLModuleType() == QOpenGLContext::LibGLES;
}

bool KWinEglDisplay::init(EGLDisplay display)
{
    m_display = display;
    EGLint major, minor;
    if (eglInitialize(m_display, &major, &minor) == EGL_FALSE) {
        qCWarning(KWIN_OPENGL) << "eglInitialize failed";
        EGLint error = eglGetError();
        if (error != EGL_SUCCESS) {
            qCWarning(KWIN_OPENGL) << "Error during eglInitialize " << error;
        }
        return false;
    }
    EGLint error = eglGetError();
    if (error != EGL_SUCCESS) {
        qCWarning(KWIN_OPENGL) << "Error during eglInitialize " << error;
        return false;
    }
    qCDebug(KWIN_OPENGL) << "Egl Initialize succeeded";

    if (eglBindAPI(isOpenGLES() ? EGL_OPENGL_ES_API : EGL_OPENGL_API) == EGL_FALSE) {
        qCCritical(KWIN_OPENGL) << "bind OpenGL API failed";
        return false;
    }
    qCDebug(KWIN_OPENGL) << "EGL version: " << major << "." << minor;

    m_extensions = QByteArray(eglQueryString(m_display, EGL_EXTENSIONS)).split(' ');

    const QByteArray requiredExtensions[] = {
        QByteArrayLiteral("EGL_KHR_no_config_context"),
        QByteArrayLiteral("EGL_KHR_surfaceless_context"),
    };
    for (const QByteArray &extensionName : requiredExtensions) {
        if (!m_extensions.contains(extensionName)) {
            qCWarning(KWIN_OPENGL) << extensionName << "extension is unsupported";
            return false;
        }
    }

    m_supportsBufferAge = hasExtension(QByteArrayLiteral("EGL_EXT_buffer_age")) && qgetenv("KWIN_USE_BUFFER_AGE") != "0";
    m_supportsPartialUpdate = hasExtension(QByteArrayLiteral("EGL_KHR_partial_update")) && qgetenv("KWIN_USE_PARTIAL_UPDATE") != "0";
    m_supportsSwapBuffersWithDamage = hasExtension(QByteArrayLiteral("EGL_EXT_swap_buffers_with_damage"));

    return true;
}

QList<QByteArray> KWinEglDisplay::extensions() const
{
    return m_extensions;
}

EGLDisplay KWinEglDisplay::display() const
{
    return m_display;
}

bool KWinEglDisplay::isValid() const
{
    return m_display != EGL_NO_DISPLAY;
}

bool KWinEglDisplay::hasExtension(const QByteArray &name) const
{
    return m_extensions.contains(name);
}

bool KWinEglDisplay::supportsBufferAge() const
{
    return m_supportsBufferAge;
}

bool KWinEglDisplay::supportsPartialUpdate() const
{
    return m_supportsPartialUpdate;
}

bool KWinEglDisplay::supportsSwapBuffersWithDamage() const
{
    return m_supportsSwapBuffersWithDamage;
}
}
