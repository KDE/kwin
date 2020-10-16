/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2015 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "abstract_egl_backend.h"
#include "egl_dmabuf.h"
#include "kwineglext.h"
#include "texture.h"
#include "composite.h"
#include "egl_context_attribute_builder.h"
#include "options.h"
#include "platform.h"
#include "scene.h"
#include "wayland_server.h"
#include "abstract_wayland_output.h"
#include <KWaylandServer/buffer_interface.h>
#include <KWaylandServer/display.h>
#include <KWaylandServer/surface_interface.h>
// kwin libs
#include <logging.h>
#include <kwinglplatform.h>
#include <kwinglutils.h>
// Qt
#include <QOpenGLContext>
#include <QOpenGLFramebufferObject>

#include <memory>

namespace KWin
{

typedef GLboolean(*eglBindWaylandDisplayWL_func)(EGLDisplay dpy, wl_display *display);
typedef GLboolean(*eglUnbindWaylandDisplayWL_func)(EGLDisplay dpy, wl_display *display);
typedef GLboolean(*eglQueryWaylandBufferWL_func)(EGLDisplay dpy, struct wl_resource *buffer, EGLint attribute, EGLint *value);
eglBindWaylandDisplayWL_func eglBindWaylandDisplayWL = nullptr;
eglUnbindWaylandDisplayWL_func eglUnbindWaylandDisplayWL = nullptr;
eglQueryWaylandBufferWL_func eglQueryWaylandBufferWL = nullptr;

static EGLContext s_globalShareContext = EGL_NO_CONTEXT;

static bool isOpenGLES_helper()
{
    if (qstrcmp(qgetenv("KWIN_COMPOSE"), "O2ES") == 0) {
        return true;
    }
    return QOpenGLContext::openGLModuleType() == QOpenGLContext::LibGLES;
}

static bool ensureGlobalShareContext()
{
    const EGLDisplay eglDisplay = kwinApp()->platform()->sceneEglDisplay();
    const EGLConfig eglConfig = kwinApp()->platform()->sceneEglConfig();

    if (s_globalShareContext != EGL_NO_CONTEXT) {
        return true;
    }

    std::vector<int> attribs;
    if (isOpenGLES_helper()) {
        EglOpenGLESContextAttributeBuilder builder;
        builder.setVersion(2);
        attribs = builder.build();
    } else {
        EglContextAttributeBuilder builder;
        attribs = builder.build();
    }

    s_globalShareContext = eglCreateContext(eglDisplay, eglConfig, EGL_NO_CONTEXT, attribs.data());
    if (s_globalShareContext == EGL_NO_CONTEXT) {
        qCWarning(KWIN_OPENGL, "Failed to create global share context: 0x%x", eglGetError());
    }

    kwinApp()->platform()->setSceneEglGlobalShareContext(s_globalShareContext);

    return s_globalShareContext != EGL_NO_CONTEXT;
}

static void destroyGlobalShareContext()
{
    const EGLDisplay eglDisplay = kwinApp()->platform()->sceneEglDisplay();
    if (eglDisplay == EGL_NO_DISPLAY || s_globalShareContext == EGL_NO_CONTEXT) {
        return;
    }
    eglDestroyContext(eglDisplay, s_globalShareContext);
    s_globalShareContext = EGL_NO_CONTEXT;
    kwinApp()->platform()->setSceneEglGlobalShareContext(EGL_NO_CONTEXT);
}

AbstractEglBackend::AbstractEglBackend()
    : QObject(nullptr)
    , OpenGLBackend()
{
    connect(Compositor::self(), &Compositor::aboutToDestroy, this, &AbstractEglBackend::teardown);
}

AbstractEglBackend::~AbstractEglBackend()
{
    delete m_dmaBuf;
}

void AbstractEglBackend::teardown()
{
    if (eglUnbindWaylandDisplayWL && m_display != EGL_NO_DISPLAY) {
        eglUnbindWaylandDisplayWL(m_display, *(WaylandServer::self()->display()));
    }
    destroyGlobalShareContext();
}

void AbstractEglBackend::cleanup()
{
    cleanupGL();
    doneCurrent();
    eglDestroyContext(m_display, m_context);
    cleanupSurfaces();
    eglReleaseThread();
    kwinApp()->platform()->setSceneEglContext(EGL_NO_CONTEXT);
    kwinApp()->platform()->setSceneEglSurface(EGL_NO_SURFACE);
    kwinApp()->platform()->setSceneEglConfig(nullptr);
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
    const QByteArray eglExtensions = eglQueryString(m_display, EGL_EXTENSIONS);
    setExtensions(eglExtensions.split(' '));
    setSupportsSurfacelessContext(hasExtension(QByteArrayLiteral("EGL_KHR_surfaceless_context")));
    return true;
}

typedef void (*eglFuncPtr)();
static eglFuncPtr getProcAddress(const char* name)
{
    return eglGetProcAddress(name);
}

void AbstractEglBackend::initKWinGL()
{
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

    if (hasExtension(QByteArrayLiteral("EGL_EXT_buffer_age"))) {
        const QByteArray useBufferAge = qgetenv("KWIN_USE_BUFFER_AGE");

        if (useBufferAge != "0")
            setSupportsBufferAge(true);
    }

    setSupportsPartialUpdate(hasExtension(QByteArrayLiteral("EGL_KHR_partial_update")));
    setSupportsSwapBuffersWithDamage(hasExtension(QByteArrayLiteral("EGL_EXT_swap_buffers_with_damage")));
}

void AbstractEglBackend::initWayland()
{
    if (!WaylandServer::self()) {
        return;
    }
    if (hasExtension(QByteArrayLiteral("EGL_WL_bind_wayland_display"))) {
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

    Q_ASSERT(!m_dmaBuf);
    m_dmaBuf = EglDmabuf::factory(this);
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
    return isOpenGLES_helper();
}

bool AbstractEglBackend::createContext()
{
    if (!ensureGlobalShareContext()) {
        return false;
    }

    const bool haveRobustness = hasExtension(QByteArrayLiteral("EGL_EXT_create_context_robustness"));
    const bool haveCreateContext = hasExtension(QByteArrayLiteral("EGL_KHR_create_context"));
    const bool haveContextPriority = hasExtension(QByteArrayLiteral("EGL_IMG_context_priority"));

    std::vector<std::unique_ptr<AbstractOpenGLContextAttributeBuilder>> candidates;
    if (isOpenGLES()) {
        if (haveCreateContext && haveRobustness && haveContextPriority) {
            auto glesRobustPriority = std::unique_ptr<AbstractOpenGLContextAttributeBuilder>(new EglOpenGLESContextAttributeBuilder);
            glesRobustPriority->setVersion(2);
            glesRobustPriority->setRobust(true);
            glesRobustPriority->setHighPriority(true);
            candidates.push_back(std::move(glesRobustPriority));
        }
        if (haveCreateContext && haveRobustness) {
            auto glesRobust = std::unique_ptr<AbstractOpenGLContextAttributeBuilder>(new EglOpenGLESContextAttributeBuilder);
            glesRobust->setVersion(2);
            glesRobust->setRobust(true);
            candidates.push_back(std::move(glesRobust));
        }
        if (haveContextPriority) {
            auto glesPriority = std::unique_ptr<AbstractOpenGLContextAttributeBuilder>(new EglOpenGLESContextAttributeBuilder);
            glesPriority->setVersion(2);
            glesPriority->setHighPriority(true);
            candidates.push_back(std::move(glesPriority));
        }
        auto gles = std::unique_ptr<AbstractOpenGLContextAttributeBuilder>(new EglOpenGLESContextAttributeBuilder);
        gles->setVersion(2);
        candidates.push_back(std::move(gles));
    } else {
        if (options->glCoreProfile() && haveCreateContext) {
            if (haveRobustness && haveContextPriority) {
                auto robustCorePriority = std::unique_ptr<AbstractOpenGLContextAttributeBuilder>(new EglContextAttributeBuilder);
                robustCorePriority->setVersion(3, 1);
                robustCorePriority->setRobust(true);
                robustCorePriority->setHighPriority(true);
                candidates.push_back(std::move(robustCorePriority));
            }
            if (haveRobustness) {
                auto robustCore = std::unique_ptr<AbstractOpenGLContextAttributeBuilder>(new EglContextAttributeBuilder);
                robustCore->setVersion(3, 1);
                robustCore->setRobust(true);
                candidates.push_back(std::move(robustCore));
            }
            if (haveContextPriority) {
                auto corePriority = std::unique_ptr<AbstractOpenGLContextAttributeBuilder>(new EglContextAttributeBuilder);
                corePriority->setVersion(3, 1);
                corePriority->setHighPriority(true);
                candidates.push_back(std::move(corePriority));
            }
            auto core = std::unique_ptr<AbstractOpenGLContextAttributeBuilder>(new EglContextAttributeBuilder);
            core->setVersion(3, 1);
            candidates.push_back(std::move(core));
        }
        if (haveRobustness && haveCreateContext && haveContextPriority) {
            auto robustPriority = std::unique_ptr<AbstractOpenGLContextAttributeBuilder>(new EglContextAttributeBuilder);
            robustPriority->setRobust(true);
            robustPriority->setHighPriority(true);
            candidates.push_back(std::move(robustPriority));
        }
        if (haveRobustness && haveCreateContext) {
            auto robust = std::unique_ptr<AbstractOpenGLContextAttributeBuilder>(new EglContextAttributeBuilder);
            robust->setRobust(true);
            candidates.push_back(std::move(robust));
        }
        candidates.emplace_back(new EglContextAttributeBuilder);
    }

    EGLContext ctx = EGL_NO_CONTEXT;
    for (auto it = candidates.begin(); it != candidates.end(); it++) {
        const auto attribs = (*it)->build();
        ctx = eglCreateContext(m_display, config(), s_globalShareContext, attribs.data());
        if (ctx != EGL_NO_CONTEXT) {
            qCDebug(KWIN_OPENGL) << "Created EGL context with attributes:" << (*it).get();
            break;
        }
    }

    if (ctx == EGL_NO_CONTEXT) {
        qCCritical(KWIN_OPENGL) << "Create Context failed";
        return false;
    }
    m_context = ctx;
    kwinApp()->platform()->setSceneEglContext(m_context);
    return true;
}

void AbstractEglBackend::setEglDisplay(const EGLDisplay &display) {
    m_display = display;
    kwinApp()->platform()->setSceneEglDisplay(display);
}

void AbstractEglBackend::setConfig(const EGLConfig &config)
{
    m_config = config;
    kwinApp()->platform()->setSceneEglConfig(config);
}

void AbstractEglBackend::setSurface(const EGLSurface &surface)
{
    m_surface = surface;
    kwinApp()->platform()->setSceneEglSurface(surface);
}

QSharedPointer<GLTexture> AbstractEglBackend::textureForOutput(AbstractOutput *requestedOutput) const
{
    QSharedPointer<GLTexture> texture(new GLTexture(GL_RGBA8, requestedOutput->pixelSize()));
    GLRenderTarget renderTarget(*texture);

    const QRect geo = requestedOutput->geometry();
    QRect invGeo(geo.left(), geo.bottom(), geo.width(), -geo.height());
    renderTarget.blitFromFramebuffer(invGeo);
    return texture;
}

AbstractEglTexture::AbstractEglTexture(SceneOpenGLTexture *texture, AbstractEglBackend *backend)
    : SceneOpenGLTexturePrivate()
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
    // FIXME: Refactor this method.

    const auto &buffer = pixmap->buffer();
    if (buffer.isNull()) {
        if (updateFromFBO(pixmap->fbo())) {
            return true;
        }
        if (loadInternalImageObject(pixmap)) {
            return true;
        }
        return false;
    }
    // try Wayland loading
    if (auto s = pixmap->surface()) {
        s->resetTrackedDamage();
    }
    if (buffer->linuxDmabufBuffer()) {
        return loadDmabufTexture(buffer);
    } else if (buffer->shmBuffer()) {
        return loadShmTexture(buffer);
    }
    return loadEglTexture(buffer);
}

void AbstractEglTexture::updateTexture(WindowPixmap *pixmap)
{
    // FIXME: Refactor this method.

    const auto &buffer = pixmap->buffer();
    if (buffer.isNull()) {
        if (updateFromFBO(pixmap->fbo())) {
            return;
        }
        if (updateFromInternalImageObject(pixmap)) {
            return;
        }
        return;
    }
    auto s = pixmap->surface();
    if (EglDmabufBuffer *dmabuf = static_cast<EglDmabufBuffer *>(buffer->linuxDmabufBuffer())) {
        q->bind();
        glEGLImageTargetTexture2DOES(GL_TEXTURE_2D, (GLeglImageOES) dmabuf->images()[0]);   //TODO
        q->unbind();
        if (m_image != EGL_NO_IMAGE_KHR) {
            eglDestroyImageKHR(m_backend->eglDisplay(), m_image);
        }
        m_image = EGL_NO_IMAGE_KHR; // The wl_buffer has ownership of the image
        // The origin in a dmabuf-buffer is at the upper-left corner, so the meaning
        // of Y-inverted is the inverse of OpenGL.
        q->setYInverted(!(dmabuf->flags() & KWaylandServer::LinuxDmabufUnstableV1Interface::YInverted));
        if (s) {
            s->resetTrackedDamage();
        }
        return;
    }
    if (!buffer->shmBuffer()) {
        q->bind();
        EGLImageKHR image = attach(buffer);
        q->unbind();
        if (image != EGL_NO_IMAGE_KHR) {
            if (m_image != EGL_NO_IMAGE_KHR) {
                eglDestroyImageKHR(m_backend->eglDisplay(), m_image);
            }
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
    const QRegion damage = s->mapToBuffer(s->trackedDamage());
    s->resetTrackedDamage();

    // TODO: this should be shared with GLTexture::update
    createTextureSubImage(image, damage);
}

bool AbstractEglTexture::createTextureImage(const QImage &image)
{
    if (image.isNull()) {
        return false;
    }

    glGenTextures(1, &m_texture);
    q->setFilter(GL_LINEAR);
    q->setWrapMode(GL_CLAMP_TO_EDGE);

    const QSize &size = image.size();
    q->bind();
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

void AbstractEglTexture::createTextureSubImage(const QImage &image, const QRegion &damage)
{
    q->bind();
    if (GLPlatform::instance()->isGLES()) {
        if (s_supportsARGB32 && (image.format() == QImage::Format_ARGB32 || image.format() == QImage::Format_ARGB32_Premultiplied)) {
            const QImage im = image.convertToFormat(QImage::Format_ARGB32_Premultiplied);
            for (const QRect &rect : damage) {
                glTexSubImage2D(m_target, 0, rect.x(), rect.y(), rect.width(), rect.height(),
                                GL_BGRA_EXT, GL_UNSIGNED_BYTE, im.copy(rect).bits());
            }
        } else {
            const QImage im = image.convertToFormat(QImage::Format_RGBA8888_Premultiplied);
            for (const QRect &rect : damage) {
                glTexSubImage2D(m_target, 0, rect.x(), rect.y(), rect.width(), rect.height(),
                                GL_RGBA, GL_UNSIGNED_BYTE, im.copy(rect).bits());
            }
        }
    } else {
        const QImage im = image.convertToFormat(QImage::Format_ARGB32_Premultiplied);
        for (const QRect &rect : damage) {
            glTexSubImage2D(m_target, 0, rect.x(), rect.y(), rect.width(), rect.height(),
                            GL_BGRA, GL_UNSIGNED_BYTE, im.copy(rect).bits());
        }
    }
    q->unbind();
}

bool AbstractEglTexture::loadShmTexture(const QPointer< KWaylandServer::BufferInterface > &buffer)
{
    return createTextureImage(buffer->data());
}

bool AbstractEglTexture::loadEglTexture(const QPointer< KWaylandServer::BufferInterface > &buffer)
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
        qCDebug(KWIN_OPENGL) << "failed to create egl image";
        q->discard();
        return false;
    }

    return true;
}

bool AbstractEglTexture::loadDmabufTexture(const QPointer< KWaylandServer::BufferInterface > &buffer)
{
    auto *dmabuf = static_cast<EglDmabufBuffer *>(buffer->linuxDmabufBuffer());
    if (!dmabuf || dmabuf->images()[0] == EGL_NO_IMAGE_KHR) {
        qCritical(KWIN_OPENGL) << "Invalid dmabuf-based wl_buffer";
        q->discard();
        return false;
    }

    Q_ASSERT(m_image == EGL_NO_IMAGE_KHR);

    glGenTextures(1, &m_texture);
    q->setWrapMode(GL_CLAMP_TO_EDGE);
    q->setFilter(GL_NEAREST);
    q->bind();
    glEGLImageTargetTexture2DOES(GL_TEXTURE_2D, (GLeglImageOES) dmabuf->images()[0]);
    q->unbind();

    m_size = dmabuf->size();
    q->setYInverted(!(dmabuf->flags() & KWaylandServer::LinuxDmabufUnstableV1Interface::YInverted));
    updateMatrix();

    return true;
}

bool AbstractEglTexture::loadInternalImageObject(WindowPixmap *pixmap)
{
    return createTextureImage(pixmap->internalImage());
}

EGLImageKHR AbstractEglTexture::attach(const QPointer< KWaylandServer::BufferInterface > &buffer)
{
    EGLint format, yInverted;
    eglQueryWaylandBufferWL(m_backend->eglDisplay(), buffer->resource(), EGL_TEXTURE_FORMAT, &format);
    if (format != EGL_TEXTURE_RGB && format != EGL_TEXTURE_RGBA) {
        qCDebug(KWIN_OPENGL) << "Unsupported texture format: " << format;
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

static QRegion scale(const QRegion &region, qreal scaleFactor)
{
    if (scaleFactor == 1) {
        return region;
    }

    QRegion scaled;
    for (const QRect &rect : region) {
        scaled += QRect(rect.topLeft() * scaleFactor, rect.size() * scaleFactor);
    }
    return scaled;
}

bool AbstractEglTexture::updateFromInternalImageObject(WindowPixmap *pixmap)
{
    const QImage image = pixmap->internalImage();
    if (image.isNull()) {
        return false;
    }

    if (m_size != image.size()) {
        glDeleteTextures(1, &m_texture);
        return loadInternalImageObject(pixmap);
    }

    createTextureSubImage(image, scale(pixmap->toplevel()->damage(), image.devicePixelRatio()));

    return true;
}

}
