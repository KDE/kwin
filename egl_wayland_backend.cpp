
/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2013 Martin Gräßlin <mgraesslin@kde.org>

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
#define WL_EGL_PLATFORM 1
#include "egl_wayland_backend.h"
// kwin
#include "composite.h"
#include "options.h"
#include "wayland_backend.h"
#include "wayland_server.h"
#include <KWayland/Client/surface.h>
// kwin libs
#include <kwinglplatform.h>
// KDE
#include <KWayland/Server/buffer_interface.h>
#include <KWayland/Server/display.h>
// Qt
#include <QOpenGLContext>

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

EglWaylandBackend::EglWaylandBackend()
    : QObject(NULL)
    , OpenGLBackend()
    , m_context(EGL_NO_CONTEXT)
    , m_bufferAge(0)
    , m_wayland(Wayland::WaylandBackend::self())
    , m_overlay(NULL)
{
    if (!m_wayland) {
        setFailed("Wayland Backend has not been created");
        return;
    }
    qCDebug(KWIN_CORE) << "Connected to Wayland display?" << (m_wayland->display() ? "yes" : "no" );
    if (!m_wayland->display()) {
        setFailed("Could not connect to Wayland compositor");
        return;
    }
    connect(m_wayland, SIGNAL(shellSurfaceSizeChanged(QSize)), SLOT(overlaySizeChanged(QSize)));
    initializeEgl();
    init();
    // Egl is always direct rendering
    setIsDirectRendering(true);

    qCWarning(KWIN_CORE) << "Using Wayland rendering backend";
    qCWarning(KWIN_CORE) << "This is a highly experimental backend, do not use for productive usage!";
    qCWarning(KWIN_CORE) << "Please do not report any issues you might encounter when using this backend!";
}

EglWaylandBackend::~EglWaylandBackend()
{
    if (eglUnbindWaylandDisplayWL && m_display != EGL_NO_DISPLAY) {
        eglUnbindWaylandDisplayWL(m_display, *(WaylandServer::self()->display()));
    }
    cleanupGL();
    doneCurrent();
    eglDestroyContext(m_display, m_context);
    eglDestroySurface(m_display, m_surface);
    eglTerminate(m_display);
    eglReleaseThread();
    if (m_overlay) {
        wl_egl_window_destroy(m_overlay);
    }
}

bool EglWaylandBackend::initializeEgl()
{
    // Get the list of client extensions
    const char* clientExtensionsCString = eglQueryString(EGL_NO_DISPLAY, EGL_EXTENSIONS);
    const QByteArray clientExtensionsString = QByteArray::fromRawData(clientExtensionsCString, qstrlen(clientExtensionsCString));
    if (clientExtensionsString.isEmpty()) {
        // If eglQueryString() returned NULL, the implementation doesn't support
        // EGL_EXT_client_extensions. Expect an EGL_BAD_DISPLAY error.
        (void) eglGetError();
    }

    const QList<QByteArray> clientExtensions = clientExtensionsString.split(' ');

    // Use eglGetPlatformDisplayEXT() to get the display pointer
    // if the implementation supports it.
    m_havePlatformBase = clientExtensions.contains(QByteArrayLiteral("EGL_EXT_platform_base"));
    if (m_havePlatformBase) {
        // Make sure that the wayland platform is supported
        if (!clientExtensions.contains(QByteArrayLiteral("EGL_EXT_platform_wayland")))
            return false;

        m_display = eglGetPlatformDisplayEXT(EGL_PLATFORM_WAYLAND_EXT, m_wayland->display(), nullptr);
    } else {
        m_display = eglGetDisplay(m_wayland->display());
    }

    if (m_display == EGL_NO_DISPLAY)
        return false;

    EGLint major, minor;
    if (eglInitialize(m_display, &major, &minor) == EGL_FALSE)
        return false;
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

void EglWaylandBackend::init()
{
    if (!initRenderingContext()) {
        setFailed("Could not initialize rendering context");
        return;
    }

    initEGL();
    GLPlatform *glPlatform = GLPlatform::instance();
    glPlatform->detect(EglPlatformInterface);
    glPlatform->printResults();
    initGL(EglPlatformInterface);

    setSupportsBufferAge(false);

    if (hasGLExtension(QByteArrayLiteral("EGL_EXT_buffer_age"))) {
        const QByteArray useBufferAge = qgetenv("KWIN_USE_BUFFER_AGE");

        if (useBufferAge != "0")
            setSupportsBufferAge(true);
    }

    if (hasGLExtension(QByteArrayLiteral("EGL_WL_bind_wayland_display"))) {
        eglBindWaylandDisplayWL = (eglBindWaylandDisplayWL_func)eglGetProcAddress("eglBindWaylandDisplayWL");
        eglUnbindWaylandDisplayWL = (eglUnbindWaylandDisplayWL_func)eglGetProcAddress("eglUnbindWaylandDisplayWL");
        eglQueryWaylandBufferWL = (eglQueryWaylandBufferWL_func)eglGetProcAddress("eglQueryWaylandBufferWL");
        if (!eglBindWaylandDisplayWL(m_display, *(WaylandServer::self()->display()))) {
            eglUnbindWaylandDisplayWL = nullptr;
            eglQueryWaylandBufferWL = nullptr;
        }
    }
}

bool EglWaylandBackend::initRenderingContext()
{
    initBufferConfigs();

#ifdef KWIN_HAVE_OPENGLES
    const EGLint context_attribs[] = {
        EGL_CONTEXT_CLIENT_VERSION, 2,
        EGL_NONE
    };

    m_context = eglCreateContext(m_display, m_config, EGL_NO_CONTEXT, context_attribs);
#else
    const EGLint context_attribs_31_core[] = {
        EGL_CONTEXT_MAJOR_VERSION_KHR, 3,
        EGL_CONTEXT_MINOR_VERSION_KHR, 1,
        EGL_CONTEXT_FLAGS_KHR,         EGL_CONTEXT_OPENGL_FORWARD_COMPATIBLE_BIT_KHR,
        EGL_NONE
    };

    const EGLint context_attribs_legacy[] = {
        EGL_NONE
    };

    const char* eglExtensionsCString = eglQueryString(m_display, EGL_EXTENSIONS);
    const QList<QByteArray> extensions = QByteArray::fromRawData(eglExtensionsCString, qstrlen(eglExtensionsCString)).split(' ');

    // Try to create a 3.1 core context
    if (options->glCoreProfile() && extensions.contains(QByteArrayLiteral("EGL_KHR_create_context")))
        m_context = eglCreateContext(m_display, m_config, EGL_NO_CONTEXT, context_attribs_31_core);

    if (m_context == EGL_NO_CONTEXT)
        m_context = eglCreateContext(m_display, m_config, EGL_NO_CONTEXT, context_attribs_legacy);
#endif

    if (m_context == EGL_NO_CONTEXT) {
        qCCritical(KWIN_CORE) << "Create Context failed";
        return false;
    }

    if (!m_wayland->surface()) {
        return false;
    }

    const QSize &size = m_wayland->shellSurfaceSize();
    auto s = m_wayland->surface();
    connect(s, &KWayland::Client::Surface::frameRendered, Compositor::self(), &Compositor::bufferSwapComplete);
    m_overlay = wl_egl_window_create(*s, size.width(), size.height());
    if (!m_overlay) {
        qCCritical(KWIN_CORE) << "Creating Wayland Egl window failed";
        return false;
    }

    if (m_havePlatformBase)
        m_surface = eglCreatePlatformWindowSurfaceEXT(m_display, m_config, (void *) m_overlay, nullptr);
    else
        m_surface = eglCreateWindowSurface(m_display, m_config, m_overlay, nullptr);

    if (m_surface == EGL_NO_SURFACE) {
        qCCritical(KWIN_CORE) << "Create Window Surface failed";
        return false;
    }

    return makeContextCurrent();
}

bool EglWaylandBackend::makeContextCurrent()
{
    if (eglMakeCurrent(m_display, m_surface, m_surface, m_context) == EGL_FALSE) {
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

bool EglWaylandBackend::initBufferConfigs()
{
    const EGLint config_attribs[] = {
        EGL_SURFACE_TYPE,         EGL_WINDOW_BIT,
        EGL_RED_SIZE,             1,
        EGL_GREEN_SIZE,           1,
        EGL_BLUE_SIZE,            1,
        EGL_ALPHA_SIZE,           0,
#ifdef KWIN_HAVE_OPENGLES
        EGL_RENDERABLE_TYPE,      EGL_OPENGL_ES2_BIT,
#else
        EGL_RENDERABLE_TYPE,      EGL_OPENGL_BIT,
#endif
        EGL_CONFIG_CAVEAT,        EGL_NONE,
        EGL_NONE,
    };

    EGLint count;
    EGLConfig configs[1024];
    if (eglChooseConfig(m_display, config_attribs, configs, 1, &count) == EGL_FALSE) {
        qCCritical(KWIN_CORE) << "choose config failed";
        return false;
    }
    if (count != 1) {
        qCCritical(KWIN_CORE) << "choose config did not return a config" << count;
        return false;
    }
    m_config = configs[0];

    return true;
}

void EglWaylandBackend::present()
{
    m_wayland->surface()->setupFrameCallback();
    Compositor::self()->aboutToSwapBuffers();

    if (supportsBufferAge()) {
        eglSwapBuffers(m_display, m_surface);
        eglQuerySurface(m_display, m_surface, EGL_BUFFER_AGE_EXT, &m_bufferAge);
        setLastDamage(QRegion());
        return;
    } else {
        eglSwapBuffers(m_display, m_surface);
        setLastDamage(QRegion());
    }
}

void EglWaylandBackend::screenGeometryChanged(const QSize &size)
{
    Q_UNUSED(size)
    // no backend specific code needed
    // TODO: base implementation in OpenGLBackend

    // The back buffer contents are now undefined
    m_bufferAge = 0;
}

SceneOpenGL::TexturePrivate *EglWaylandBackend::createBackendTexture(SceneOpenGL::Texture *texture)
{
    return new EglWaylandTexture(texture, this);
}

QRegion EglWaylandBackend::prepareRenderingFrame()
{
    if (!lastDamage().isEmpty())
        present();
    QRegion repaint;
    if (supportsBufferAge())
        repaint = accumulatedDamageHistory(m_bufferAge);
    eglWaitNative(EGL_CORE_NATIVE_ENGINE);
    startRenderTimer();
    return repaint;
}

void EglWaylandBackend::endRenderingFrame(const QRegion &renderedRegion, const QRegion &damagedRegion)
{
    if (damagedRegion.isEmpty()) {
        setLastDamage(QRegion());

        // If the damaged region of a window is fully occluded, the only
        // rendering done, if any, will have been to repair a reused back
        // buffer, making it identical to the front buffer.
        //
        // In this case we won't post the back buffer. Instead we'll just
        // set the buffer age to 1, so the repaired regions won't be
        // rendered again in the next frame.
        if (!renderedRegion.isEmpty())
            glFlush();

        m_bufferAge = 1;
        return;
    }

    setLastDamage(renderedRegion);

    if (!blocksForRetrace()) {
        // This also sets lastDamage to empty which prevents the frame from
        // being posted again when prepareRenderingFrame() is called.
        present();
    } else {
        // Make sure that the GPU begins processing the command stream
        // now and not the next time prepareRenderingFrame() is called.
        glFlush();
    }

    // Save the damaged region to history
    if (supportsBufferAge())
        addToDamageHistory(damagedRegion);
}

bool EglWaylandBackend::makeCurrent()
{
    if (QOpenGLContext *context = QOpenGLContext::currentContext()) {
        // Workaround to tell Qt that no QOpenGLContext is current
        context->doneCurrent();
    }
    const bool current = eglMakeCurrent(m_display, m_surface, m_surface, m_context);
    return current;
}

void EglWaylandBackend::doneCurrent()
{
    eglMakeCurrent(m_display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
}

void EglWaylandBackend::overlaySizeChanged(const QSize &size)
{
    wl_egl_window_resize(m_overlay, size.width(), size.height(), 0, 0);
}

bool EglWaylandBackend::usesOverlayWindow() const
{
    return false;
}

/************************************************
 * EglTexture
 ************************************************/

EglWaylandTexture::EglWaylandTexture(KWin::SceneOpenGL::Texture *texture, KWin::EglWaylandBackend *backend)
    : SceneOpenGL::TexturePrivate()
    , q(texture)
    , m_backend(backend)
{
    m_target = GL_TEXTURE_2D;
}

EglWaylandTexture::~EglWaylandTexture()
{
    if (m_image != EGL_NO_IMAGE_KHR) {
        eglDestroyImageKHR(m_backend->m_display, m_image);
    }
}

OpenGLBackend *EglWaylandTexture::backend()
{
    return m_backend;
}

bool EglWaylandTexture::loadTexture(WindowPixmap *pixmap)
{
    const auto &buffer = pixmap->buffer();
    if (buffer.isNull()) {
        return false;
    }
    if (buffer->shmBuffer()) {
        return loadShmTexture(buffer);
    } else {
        return loadEglTexture(buffer);
    }
}

bool EglWaylandTexture::loadShmTexture(const QPointer< KWayland::Server::BufferInterface > &buffer)
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

bool EglWaylandTexture::loadEglTexture(const QPointer< KWayland::Server::BufferInterface > &buffer)
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

void EglWaylandTexture::updateTexture(WindowPixmap *pixmap)
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
            eglDestroyImageKHR(m_backend->m_display, m_image);
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

EGLImageKHR EglWaylandTexture::attach(const QPointer< KWayland::Server::BufferInterface > &buffer)
{
    EGLint format, width, height, yInverted;
    eglQueryWaylandBufferWL(m_backend->m_display, buffer->resource(), EGL_TEXTURE_FORMAT, &format);
    if (format != EGL_TEXTURE_RGB && format != EGL_TEXTURE_RGBA) {
        qCDebug(KWIN_CORE) << "Unsupported texture format: " << format;
        return EGL_NO_IMAGE_KHR;
    }
    eglQueryWaylandBufferWL(m_backend->m_display, buffer->resource(), EGL_WAYLAND_Y_INVERTED_WL, &yInverted);
    eglQueryWaylandBufferWL(m_backend->m_display, buffer->resource(), EGL_WIDTH, &width);
    eglQueryWaylandBufferWL(m_backend->m_display, buffer->resource(), EGL_HEIGHT, &height);

    const EGLint attribs[] = {
        EGL_WAYLAND_PLANE_WL, 0,
        EGL_NONE
    };
    EGLImageKHR image = eglCreateImageKHR(m_backend->m_display, EGL_NO_CONTEXT, EGL_WAYLAND_BUFFER_WL,
                                      (EGLClientBuffer)buffer->resource(), attribs);
    if (image != EGL_NO_IMAGE_KHR) {
        glEGLImageTargetTexture2DOES(GL_TEXTURE_2D, (GLeglImageOES)image);
        m_size = QSize(width, height);
        updateMatrix();
        q->setYInverted(yInverted);
    }
    return image;
}

} // namespace
