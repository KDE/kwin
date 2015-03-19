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
#if HAVE_WAYLAND
#include "wayland_server.h"
#include <KWayland/Server/buffer_interface.h>
#include <KWayland/Server/display.h>
#endif
// kwin libs
#include <kwinglplatform.h>
// Qt
#include <QOpenGLContext>

namespace KWin
{

#if HAVE_WAYLAND
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
#endif

AbstractEglBackend::AbstractEglBackend()
    : OpenGLBackend()
{
}

AbstractEglBackend::~AbstractEglBackend() = default;

void AbstractEglBackend::cleanup()
{
#if HAVE_WAYLAND
    if (eglUnbindWaylandDisplayWL && eglDisplay() != EGL_NO_DISPLAY) {
        eglUnbindWaylandDisplayWL(eglDisplay(), *(WaylandServer::self()->display()));
    }
#endif
    cleanupGL();
    doneCurrent();
    eglDestroyContext(m_display, m_context);
    eglDestroySurface(m_display, m_surface);
    eglTerminate(m_display);
    eglReleaseThread();
}

bool AbstractEglBackend::initEglAPI()
{
    EGLint major, minor;
    if (eglInitialize(m_display, &major, &minor) == EGL_FALSE) {
        return false;
    }
    EGLint error = eglGetError();
    if (error != EGL_SUCCESS) {
        qCWarning(KWIN_CORE) << "Error during eglInitialize " << error;
        return false;
    }
    qCDebug(KWIN_CORE) << "Egl Initialize succeeded";

#ifdef KWIN_HAVE_OPENGLES
    eglBindAPI(EGL_OPENGL_ES_API);
#else
    if (eglBindAPI(EGL_OPENGL_API) == EGL_FALSE) {
        qCCritical(KWIN_CORE) << "bind OpenGL API failed";
        return false;
    }
#endif
    qCDebug(KWIN_CORE) << "EGL version: " << major << "." << minor;
    return true;
}

void AbstractEglBackend::initKWinGL()
{
    initEGL();
    GLPlatform *glPlatform = GLPlatform::instance();
    glPlatform->detect(EglPlatformInterface);
    if (GLPlatform::instance()->driver() == Driver_Intel)
        options->setUnredirectFullscreen(false); // bug #252817
    options->setGlPreferBufferSwap(options->glPreferBufferSwap()); // resolve autosetting
    if (options->glPreferBufferSwap() == Options::AutoSwapStrategy)
        options->setGlPreferBufferSwap('e'); // for unknown drivers - should not happen
    glPlatform->printResults();
    initGL(EglPlatformInterface);
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
#if HAVE_WAYLAND
    if (!WaylandServer::self()) {
        return;
    }
    if (hasGLExtension(QByteArrayLiteral("EGL_WL_bind_wayland_display"))) {
        eglBindWaylandDisplayWL = (eglBindWaylandDisplayWL_func)eglGetProcAddress("eglBindWaylandDisplayWL");
        eglUnbindWaylandDisplayWL = (eglUnbindWaylandDisplayWL_func)eglGetProcAddress("eglUnbindWaylandDisplayWL");
        eglQueryWaylandBufferWL = (eglQueryWaylandBufferWL_func)eglGetProcAddress("eglQueryWaylandBufferWL");
        if (!eglBindWaylandDisplayWL(eglDisplay(), *(WaylandServer::self()->display()))) {
            eglUnbindWaylandDisplayWL = nullptr;
            eglQueryWaylandBufferWL = nullptr;
        }
    }
#endif
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

AbstractEglTexture::AbstractEglTexture(SceneOpenGL::Texture *texture, AbstractEglBackend *backend)
    : SceneOpenGL::TexturePrivate()
    , q(texture)
    , m_backend(backend)
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
#if HAVE_WAYLAND
    const auto &buffer = pixmap->buffer();
    if (buffer.isNull()) {
        // try X11 loading
        return loadTexture(pixmap->pixmap(), pixmap->toplevel()->size());
    }
    // try Wayland loading
    if (buffer->shmBuffer()) {
        return loadShmTexture(buffer);
    } else {
        return loadEglTexture(buffer);
    }
#else
    return loadTexture(pixmap->pixmap(), pixmap->toplevel()->size());
#endif
}

bool AbstractEglTexture::loadTexture(xcb_pixmap_t pix, const QSize &size)
{
    if (pix == XCB_NONE)
        return false;

    glGenTextures(1, &m_texture);
    q->setWrapMode(GL_CLAMP_TO_EDGE);
    q->setFilter(GL_LINEAR);
    q->bind();
    const EGLint attribs[] = {
        EGL_IMAGE_PRESERVED_KHR, EGL_TRUE,
        EGL_NONE
    };
    m_image = eglCreateImageKHR(m_backend->eglDisplay(), EGL_NO_CONTEXT, EGL_NATIVE_PIXMAP_KHR,
                                          (EGLClientBuffer)pix, attribs);

    if (EGL_NO_IMAGE_KHR == m_image) {
        qCDebug(KWIN_CORE) << "failed to create egl image";
        q->unbind();
        q->discard();
        return false;
    }
    glEGLImageTargetTexture2DOES(GL_TEXTURE_2D, (GLeglImageOES)m_image);
    q->unbind();
    q->setYInverted(true);
    m_size = size;
    updateMatrix();
    return true;
}

void AbstractEglTexture::updateTexture(WindowPixmap *pixmap)
{
    const auto &buffer = pixmap->buffer();
    if (buffer.isNull()) {
        return;
    }
    if (!buffer->shmBuffer()) {
        q->bind();
        EGLImageKHR image = attach(buffer);
        q->unbind();
        if (image != EGL_NO_IMAGE_KHR) {
            eglDestroyImageKHR(m_backend->eglDisplay(), m_image);
            m_image = image;
        }
        return;
    }
    // shm fallback
    if (GLPlatform::instance()->isGLES()) {
        // FIXME
        return;
    }
    const QImage &image = buffer->data();
    if (image.isNull()) {
        return;
    }
    Q_ASSERT(image.size() == m_size);
    q->bind();
    const QRegion &damage = pixmap->toplevel()->damage();

    // TODO: this should be shared with GLTexture::update
    const QImage im = image.convertToFormat(QImage::Format_ARGB32_Premultiplied);
    for (const QRect &rect : damage.rects()) {
        glTexSubImage2D(m_target, 0, rect.x(), rect.y(), rect.width(), rect.height(),
                        GL_BGRA, GL_UNSIGNED_BYTE, im.copy(rect).bits());
    }
    q->unbind();
}

#if HAVE_WAYLAND
bool AbstractEglTexture::loadShmTexture(const QPointer< KWayland::Server::BufferInterface > &buffer)
{
    if (GLPlatform::instance()->isGLES()) {
        // FIXME
        return false;
    }
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
    glTexImage2D(m_target, 0, format, size.width(), size.height(), 0,
                 GL_BGRA, GL_UNSIGNED_BYTE, image.bits());

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
    EGLint format, width, height, yInverted;
    eglQueryWaylandBufferWL(m_backend->eglDisplay(), buffer->resource(), EGL_TEXTURE_FORMAT, &format);
    if (format != EGL_TEXTURE_RGB && format != EGL_TEXTURE_RGBA) {
        qCDebug(KWIN_CORE) << "Unsupported texture format: " << format;
        return EGL_NO_IMAGE_KHR;
    }
    eglQueryWaylandBufferWL(m_backend->eglDisplay(), buffer->resource(), EGL_WAYLAND_Y_INVERTED_WL, &yInverted);
    eglQueryWaylandBufferWL(m_backend->eglDisplay(), buffer->resource(), EGL_WIDTH, &width);
    eglQueryWaylandBufferWL(m_backend->eglDisplay(), buffer->resource(), EGL_HEIGHT, &height);

    const EGLint attribs[] = {
        EGL_WAYLAND_PLANE_WL, 0,
        EGL_NONE
    };
    EGLImageKHR image = eglCreateImageKHR(m_backend->eglDisplay(), EGL_NO_CONTEXT, EGL_WAYLAND_BUFFER_WL,
                                      (EGLClientBuffer)buffer->resource(), attribs);
    if (image != EGL_NO_IMAGE_KHR) {
        glEGLImageTargetTexture2DOES(GL_TEXTURE_2D, (GLeglImageOES)image);
        m_size = QSize(width, height);
        updateMatrix();
        q->setYInverted(yInverted);
    }
    return image;
}
#endif

}

