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
#include "abstract_egl_backend.h"
#include "options.h"
#include "platform.h"
#include "wayland_server.h"
#include <KWayland/Server/buffer_interface.h>
#include <KWayland/Server/display.h>
#include <KWayland/Server/surface_interface.h>
// kwin libs
#include <kwinglplatform.h>
// Qt
#include <QOpenGLContext>
#include <QOpenGLFramebufferObject>

namespace KWin
{

typedef GLboolean(*eglBindWaylandDisplayWL_func)(EGLDisplay dpy, wl_display *display);
typedef GLboolean(*eglUnbindWaylandDisplayWL_func)(EGLDisplay dpy, wl_display *display);
typedef GLboolean(*eglQueryWaylandBufferWL_func)(EGLDisplay dpy, struct wl_resource *buffer, EGLint attribute, EGLint *value);
eglBindWaylandDisplayWL_func eglBindWaylandDisplayWL = nullptr;
eglUnbindWaylandDisplayWL_func eglUnbindWaylandDisplayWL = nullptr;
eglQueryWaylandBufferWL_func eglQueryWaylandBufferWL = nullptr;

#ifndef EGL_WAYLAND_BUFFER_WL
#define EGL_WAYLAND_BUFFER_WL                   0x31D5
#endif
#ifndef EGL_WAYLAND_PLANE_WL
#define EGL_WAYLAND_PLANE_WL                    0x31D6
#endif
#ifndef EGL_WAYLAND_Y_INVERTED_WL
#define EGL_WAYLAND_Y_INVERTED_WL               0x31DB
#endif

AbstractEglBackend::AbstractEglBackend()
    : OpenGLBackend()
{
}

AbstractEglBackend::~AbstractEglBackend() = default;

void AbstractEglBackend::unbindWaylandDisplay()
{
    auto display = kwinApp()->platform()->sceneEglDisplay();
    if (eglUnbindWaylandDisplayWL && display != EGL_NO_DISPLAY) {
        eglUnbindWaylandDisplayWL(display, *(WaylandServer::self()->display()));
    }
}

void AbstractEglBackend::cleanup()
{
    cleanupGL();
    doneCurrent();
    eglDestroyContext(m_display, m_context);
    cleanupSurfaces();
    eglReleaseThread();
}

void AbstractEglBackend::cleanupSurfaces()
{
    if (m_surface != EGL_NO_SURFACE) {
        eglDestroySurface(m_display, m_surface);
    }
}

bool AbstractEglBackend::initEglAPI()
{
    EGLint major, minor;
    if (eglInitialize(m_display, &major, &minor) == EGL_FALSE) {
        qCWarning(KWIN_CORE) << "eglInitialize failed";
        EGLint error = eglGetError();
        if (error != EGL_SUCCESS) {
            qCWarning(KWIN_CORE) << "Error during eglInitialize " << error;
        }
        return false;
    }
    EGLint error = eglGetError();
    if (error != EGL_SUCCESS) {
        qCWarning(KWIN_CORE) << "Error during eglInitialize " << error;
        return false;
    }
    qCDebug(KWIN_CORE) << "Egl Initialize succeeded";

    if (eglBindAPI(isOpenGLES() ? EGL_OPENGL_ES_API : EGL_OPENGL_API) == EGL_FALSE) {
        qCCritical(KWIN_CORE) << "bind OpenGL API failed";
        return false;
    }
    qCDebug(KWIN_CORE) << "EGL version: " << major << "." << minor;
    return true;
}

typedef void (*eglFuncPtr)();
static eglFuncPtr getProcAddress(const char* name)
{
    return eglGetProcAddress(name);
}

void AbstractEglBackend::initKWinGL()
{
    initEGL();
    GLPlatform *glPlatform = GLPlatform::instance();
    glPlatform->detect(EglPlatformInterface);
    options->setGlPreferBufferSwap(options->glPreferBufferSwap()); // resolve autosetting
    if (options->glPreferBufferSwap() == Options::AutoSwapStrategy)
        options->setGlPreferBufferSwap('e'); // for unknown drivers - should not happen
    glPlatform->printResults();
    initGL(&getProcAddress);
}

void AbstractEglBackend::initBufferAge()
{
    setSupportsBufferAge(false);

    if (hasGLExtension(QByteArrayLiteral("EGL_EXT_buffer_age"))) {
        const QByteArray useBufferAge = qgetenv("KWIN_USE_BUFFER_AGE");

        if (useBufferAge != "0")
            setSupportsBufferAge(true);
    }
}

void AbstractEglBackend::initWayland()
{
    if (!WaylandServer::self()) {
        return;
    }
    if (hasGLExtension(QByteArrayLiteral("EGL_WL_bind_wayland_display"))) {
        eglBindWaylandDisplayWL = (eglBindWaylandDisplayWL_func)eglGetProcAddress("eglBindWaylandDisplayWL");
        eglUnbindWaylandDisplayWL = (eglUnbindWaylandDisplayWL_func)eglGetProcAddress("eglUnbindWaylandDisplayWL");
        eglQueryWaylandBufferWL = (eglQueryWaylandBufferWL_func)eglGetProcAddress("eglQueryWaylandBufferWL");
        // only bind if not already done
        if (waylandServer()->display()->eglDisplay() != eglDisplay()) {
            if (!eglBindWaylandDisplayWL(eglDisplay(), *(WaylandServer::self()->display()))) {
                eglUnbindWaylandDisplayWL = nullptr;
                eglQueryWaylandBufferWL = nullptr;
            } else {
                waylandServer()->display()->setEglDisplay(eglDisplay());
            }
        }
    }
}

void AbstractEglBackend::initClientExtensions()
{
    // Get the list of client extensions
    const char* clientExtensionsCString = eglQueryString(EGL_NO_DISPLAY, EGL_EXTENSIONS);
    const QByteArray clientExtensionsString = QByteArray::fromRawData(clientExtensionsCString, qstrlen(clientExtensionsCString));
    if (clientExtensionsString.isEmpty()) {
        // If eglQueryString() returned NULL, the implementation doesn't support
        // EGL_EXT_client_extensions. Expect an EGL_BAD_DISPLAY error.
        (void) eglGetError();
    }

    m_clientExtensions = clientExtensionsString.split(' ');
}

bool AbstractEglBackend::hasClientExtension(const QByteArray &ext) const
{
    return m_clientExtensions.contains(ext);
}

bool AbstractEglBackend::makeCurrent()
{
    if (QOpenGLContext *context = QOpenGLContext::currentContext()) {
        // Workaround to tell Qt that no QOpenGLContext is current
        context->doneCurrent();
    }
    const bool current = eglMakeCurrent(m_display, m_surface, m_surface, m_context);
    return current;
}

void AbstractEglBackend::doneCurrent()
{
    eglMakeCurrent(m_display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
}

bool AbstractEglBackend::isOpenGLES() const
{
    if (qstrcmp(qgetenv("KWIN_COMPOSE"), "O2ES") == 0) {
        return true;
    }
    return QOpenGLContext::openGLModuleType() == QOpenGLContext::LibGLES;
}

bool AbstractEglBackend::createContext()
{
    const QByteArray eglExtensions = eglQueryString(m_display, EGL_EXTENSIONS);
    const QList<QByteArray> extensions = eglExtensions.split(' ');
    const bool haveRobustness = extensions.contains(QByteArrayLiteral("EGL_EXT_create_context_robustness"));
    const bool haveCreateContext = extensions.contains(QByteArrayLiteral("EGL_KHR_create_context"));

    EGLContext ctx = EGL_NO_CONTEXT;
    if (isOpenGLES()) {
        if (haveCreateContext && haveRobustness) {
            const EGLint context_attribs[] = {
                EGL_CONTEXT_CLIENT_VERSION,                         2,
                EGL_CONTEXT_OPENGL_ROBUST_ACCESS_EXT,               EGL_TRUE,
                EGL_CONTEXT_OPENGL_RESET_NOTIFICATION_STRATEGY_EXT, EGL_LOSE_CONTEXT_ON_RESET_EXT,
                EGL_NONE
            };
            ctx = eglCreateContext(m_display, config(), EGL_NO_CONTEXT, context_attribs);
        }
        if (ctx == EGL_NO_CONTEXT) {
            const EGLint context_attribs[] = {
                EGL_CONTEXT_CLIENT_VERSION, 2,
                EGL_NONE
            };

            ctx = eglCreateContext(m_display, config(), EGL_NO_CONTEXT, context_attribs);
        }
    } else {
        // Try to create a 3.1 core context
        if (options->glCoreProfile() && haveCreateContext) {
            if (haveRobustness) {
                const int attribs[] = {
                    EGL_CONTEXT_MAJOR_VERSION_KHR,                      3,
                    EGL_CONTEXT_MINOR_VERSION_KHR,                      1,
                    EGL_CONTEXT_OPENGL_RESET_NOTIFICATION_STRATEGY_KHR, EGL_LOSE_CONTEXT_ON_RESET_KHR,
                    EGL_CONTEXT_FLAGS_KHR,                              EGL_CONTEXT_OPENGL_ROBUST_ACCESS_BIT_KHR,
                    EGL_NONE
                };
                ctx = eglCreateContext(m_display, config(), EGL_NO_CONTEXT, attribs);
            }
            if (ctx == EGL_NO_CONTEXT) {
                // try without robustness
                const EGLint attribs[] = {
                    EGL_CONTEXT_MAJOR_VERSION_KHR, 3,
                    EGL_CONTEXT_MINOR_VERSION_KHR, 1,
                    EGL_NONE
                };
                ctx = eglCreateContext(m_display, config(), EGL_NO_CONTEXT, attribs);
            }
        }

        if (ctx == EGL_NO_CONTEXT && haveRobustness && haveCreateContext) {
            const int attribs[] = {
                EGL_CONTEXT_FLAGS_KHR, EGL_CONTEXT_OPENGL_ROBUST_ACCESS_BIT_KHR,
                EGL_CONTEXT_OPENGL_RESET_NOTIFICATION_STRATEGY_KHR, EGL_LOSE_CONTEXT_ON_RESET_KHR,
                EGL_NONE
            };
            ctx = eglCreateContext(m_display, config(), EGL_NO_CONTEXT, attribs);
        }
        if (ctx == EGL_NO_CONTEXT) {
            // and last but not least: try without robustness
            const EGLint attribs[] = {
                EGL_NONE
            };
            ctx = eglCreateContext(m_display, config(), EGL_NO_CONTEXT, attribs);
        }
    }

    if (ctx == EGL_NO_CONTEXT) {
        qCCritical(KWIN_CORE) << "Create Context failed";
        return false;
    }
    m_context = ctx;
    return true;
}

void AbstractEglBackend::setEglDisplay(const EGLDisplay &display) {
    m_display = display;
    kwinApp()->platform()->setSceneEglDisplay(display);
}

AbstractEglTexture::AbstractEglTexture(SceneOpenGL::Texture *texture, AbstractEglBackend *backend)
    : SceneOpenGL::TexturePrivate()
    , q(texture)
    , m_backend(backend)
    , m_image(EGL_NO_IMAGE_KHR)
{
    m_target = GL_TEXTURE_2D;
}

AbstractEglTexture::~AbstractEglTexture()
{
    if (m_image != EGL_NO_IMAGE_KHR) {
        eglDestroyImageKHR(m_backend->eglDisplay(), m_image);
    }
}

OpenGLBackend *AbstractEglTexture::backend()
{
    return m_backend;
}

bool AbstractEglTexture::loadTexture(WindowPixmap *pixmap)
{
    const auto &buffer = pixmap->buffer();
    if (buffer.isNull()) {
        if (updateFromFBO(pixmap->fbo())) {
            return true;
        }
        return false;
    }
    // try Wayland loading
    if (auto s = pixmap->surface()) {
        s->resetTrackedDamage();
    }
    if (buffer->shmBuffer()) {
        return loadShmTexture(buffer);
    } else {
        return loadEglTexture(buffer);
    }
}

void AbstractEglTexture::updateTexture(WindowPixmap *pixmap)
{
    const auto &buffer = pixmap->buffer();
    if (buffer.isNull()) {
        const auto &fbo = pixmap->fbo();
        if (!fbo.isNull()) {
            if (m_texture != fbo->texture()) {
                updateFromFBO(fbo);
            }
            return;
        }
        return;
    }
    auto s = pixmap->surface();
    if (!buffer->shmBuffer()) {
        q->bind();
        EGLImageKHR image = attach(buffer);
        q->unbind();
        if (image != EGL_NO_IMAGE_KHR) {
            eglDestroyImageKHR(m_backend->eglDisplay(), m_image);
            m_image = image;
        }
        if (s) {
            s->resetTrackedDamage();
        }
        return;
    }
    // shm fallback
    const QImage &image = buffer->data();
    if (image.isNull() || !s) {
        return;
    }
    Q_ASSERT(image.size() == m_size);
    q->bind();
    const QRegion damage = s->trackedDamage();
    s->resetTrackedDamage();

    // TODO: this should be shared with GLTexture::update
    if (GLPlatform::instance()->isGLES()) {
        if (s_supportsARGB32 && (image.format() == QImage::Format_ARGB32 || image.format() == QImage::Format_ARGB32_Premultiplied)) {
            const QImage im = image.convertToFormat(QImage::Format_ARGB32_Premultiplied);
            for (const QRect &rect : damage.rects()) {
                glTexSubImage2D(m_target, 0, rect.x(), rect.y(), rect.width(), rect.height(),
                                GL_BGRA_EXT, GL_UNSIGNED_BYTE, im.copy(rect).bits());
            }
        } else {
            const QImage im = image.convertToFormat(QImage::Format_RGBA8888_Premultiplied);
            for (const QRect &rect : damage.rects()) {
                glTexSubImage2D(m_target, 0, rect.x(), rect.y(), rect.width(), rect.height(),
                                GL_RGBA, GL_UNSIGNED_BYTE, im.copy(rect).bits());
            }
        }
    } else {
        const QImage im = image.convertToFormat(QImage::Format_ARGB32_Premultiplied);
        for (const QRect &rect : damage.rects()) {
            glTexSubImage2D(m_target, 0, rect.x(), rect.y(), rect.width(), rect.height(),
                            GL_BGRA, GL_UNSIGNED_BYTE, im.copy(rect).bits());
        }
    }
    q->unbind();
}

bool AbstractEglTexture::loadShmTexture(const QPointer< KWayland::Server::BufferInterface > &buffer)
{
    const QImage &image = buffer->data();
    if (image.isNull()) {
        return false;
    }

    glGenTextures(1, &m_texture);
    q->setWrapMode(GL_CLAMP_TO_EDGE);
    q->setFilter(GL_LINEAR);
    q->bind();

    const QSize &size = image.size();
    // TODO: this should be shared with GLTexture(const QImage&, GLenum)
    GLenum format = 0;
    switch (image.format()) {
    case QImage::Format_ARGB32:
    case QImage::Format_ARGB32_Premultiplied:
        format = GL_RGBA8;
        break;
    case QImage::Format_RGB32:
        format = GL_RGB8;
        break;
    default:
        return false;
    }
    if (GLPlatform::instance()->isGLES()) {
        if (s_supportsARGB32 && format == GL_RGBA8) {
            const QImage im = image.convertToFormat(QImage::Format_ARGB32_Premultiplied);
            glTexImage2D(m_target, 0, GL_BGRA_EXT, im.width(), im.height(),
                         0, GL_BGRA_EXT, GL_UNSIGNED_BYTE, im.bits());
        } else {
            const QImage im = image.convertToFormat(QImage::Format_RGBA8888_Premultiplied);
            glTexImage2D(m_target, 0, GL_RGBA, im.width(), im.height(),
                         0, GL_RGBA, GL_UNSIGNED_BYTE, im.bits());
        }
    } else {
        glTexImage2D(m_target, 0, format, size.width(), size.height(), 0,
                    GL_BGRA, GL_UNSIGNED_BYTE, image.bits());
    }

    q->unbind();
    q->setYInverted(true);
    m_size = size;
    updateMatrix();
    return true;
}

bool AbstractEglTexture::loadEglTexture(const QPointer< KWayland::Server::BufferInterface > &buffer)
{
    if (!eglQueryWaylandBufferWL) {
        return false;
    }
    if (!buffer->resource()) {
        return false;
    }

    glGenTextures(1, &m_texture);
    q->setWrapMode(GL_CLAMP_TO_EDGE);
    q->setFilter(GL_LINEAR);
    q->bind();
    m_image = attach(buffer);
    q->unbind();

    if (EGL_NO_IMAGE_KHR == m_image) {
        qCDebug(KWIN_CORE) << "failed to create egl image";
        q->discard();
        return false;
    }

    return true;
}

EGLImageKHR AbstractEglTexture::attach(const QPointer< KWayland::Server::BufferInterface > &buffer)
{
    EGLint format, yInverted;
    eglQueryWaylandBufferWL(m_backend->eglDisplay(), buffer->resource(), EGL_TEXTURE_FORMAT, &format);
    if (format != EGL_TEXTURE_RGB && format != EGL_TEXTURE_RGBA) {
        qCDebug(KWIN_CORE) << "Unsupported texture format: " << format;
        return EGL_NO_IMAGE_KHR;
    }
    if (!eglQueryWaylandBufferWL(m_backend->eglDisplay(), buffer->resource(), EGL_WAYLAND_Y_INVERTED_WL, &yInverted)) {
        // if EGL_WAYLAND_Y_INVERTED_WL is not supported wl_buffer should be treated as if value were EGL_TRUE
        yInverted = EGL_TRUE;
    }

    const EGLint attribs[] = {
        EGL_WAYLAND_PLANE_WL, 0,
        EGL_NONE
    };
    EGLImageKHR image = eglCreateImageKHR(m_backend->eglDisplay(), EGL_NO_CONTEXT, EGL_WAYLAND_BUFFER_WL,
                                      (EGLClientBuffer)buffer->resource(), attribs);
    if (image != EGL_NO_IMAGE_KHR) {
        glEGLImageTargetTexture2DOES(GL_TEXTURE_2D, (GLeglImageOES)image);
        m_size = buffer->size();
        updateMatrix();
        q->setYInverted(yInverted);
    }
    return image;
}

bool AbstractEglTexture::updateFromFBO(const QSharedPointer<QOpenGLFramebufferObject> &fbo)
{
    if (fbo.isNull()) {
        return false;
    }
    m_texture = fbo->texture();
    m_size = fbo->size();
    q->setWrapMode(GL_CLAMP_TO_EDGE);
    q->setFilter(GL_LINEAR);
    q->setYInverted(false);
    updateMatrix();
    return true;
}

}

