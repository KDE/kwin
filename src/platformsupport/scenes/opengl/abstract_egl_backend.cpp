/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2015 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "abstract_egl_backend.h"
#include "composite.h"
#include "egl_context_attribute_builder.h"
#include "options.h"
#include "platform.h"
#include "scene.h"
#include "wayland_server.h"
#include "abstract_wayland_output.h"
#include <KWaylandServer/display.h>
#include <KWaylandServer/rendererinterface.h>
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

AbstractEglBackend *AbstractEglBackend::s_primaryBackend = nullptr;

AbstractEglBackend::AbstractEglBackend()
    : QObject(nullptr)
    , OpenGLBackend()
{
    if (s_primaryBackend == nullptr) {
        setPrimaryBackend(this);
    }
    connect(Compositor::self(), &Compositor::aboutToDestroy, this, &AbstractEglBackend::teardown);
}

AbstractEglBackend::~AbstractEglBackend()
{
}

void AbstractEglBackend::teardown()
{
    if (waylandServer()) {
        waylandServer()->display()->rendererInterface()->setEglDisplay(nullptr);
    }
    destroyGlobalShareContext();
}

void AbstractEglBackend::cleanup()
{
    cleanupGL();
    finishWayland();
    doneCurrent();
    eglDestroyContext(m_display, m_context);
    cleanupSurfaces();
    eglReleaseThread();
    kwinApp()->platform()->setSceneEglContext(EGL_NO_CONTEXT);
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
    setSupportsNativeFence(hasExtension(QByteArrayLiteral("EGL_ANDROID_native_fence_sync")));
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

        if (useBufferAge != "0") {
            setSupportsBufferAge(true);
        }
    }

    if (hasExtension(QByteArrayLiteral("EGL_KHR_partial_update"))) {
        const QByteArray usePartialUpdate = qgetenv("KWIN_USE_PARTIAL_UPDATE");
        if (usePartialUpdate != "0") {
            setSupportsPartialUpdate(true);
        }
    }
    setSupportsSwapBuffersWithDamage(hasExtension(QByteArrayLiteral("EGL_EXT_swap_buffers_with_damage")));
}

void AbstractEglBackend::initWayland()
{
    if (!WaylandServer::self()) {
        return;
    }
    KWaylandServer::RendererInterface *rendererInterface = waylandServer()->display()->rendererInterface();
    // only bind if not already done
    if (rendererInterface->eglDisplay() == EGL_NO_DISPLAY) {
        rendererInterface->setEglDisplay(eglDisplay());
    }
    if (isOpenGLES()) {
        rendererInterface->setGraphicsApi(KWaylandServer::RendererInterface::GraphicsApi::OpenGLES);
    } else {
        rendererInterface->setGraphicsApi(KWaylandServer::RendererInterface::GraphicsApi::OpenGL);
    }
    rendererInterface->setSupportsARGB32(GLTexturePrivate::s_supportsARGB32);
}

void AbstractEglBackend::finishWayland()
{
    if (waylandServer()) {
        waylandServer()->display()->rendererInterface()->invalidateGraphics();
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
        if (options->glCoreProfile() && haveCreateContext) {
            if (haveRobustness && haveContextPriority) {
                auto robustCorePriority = std::make_unique<EglContextAttributeBuilder>();
                robustCorePriority->setVersion(3, 1);
                robustCorePriority->setRobust(true);
                robustCorePriority->setHighPriority(true);
                candidates.push_back(std::move(robustCorePriority));
            }
            if (haveRobustness) {
                auto robustCore = std::make_unique<EglContextAttributeBuilder>();
                robustCore->setVersion(3, 1);
                robustCore->setRobust(true);
                candidates.push_back(std::move(robustCore));
            }
            if (haveContextPriority) {
                auto corePriority = std::make_unique<EglContextAttributeBuilder>();
                corePriority->setVersion(3, 1);
                corePriority->setHighPriority(true);
                candidates.push_back(std::move(corePriority));
            }
            auto core = std::make_unique<EglContextAttributeBuilder>();
            core->setVersion(3, 1);
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
    if (isPrimary()) {
        kwinApp()->platform()->setSceneEglContext(m_context);
    }
    return true;
}

void AbstractEglBackend::setEglDisplay(const EGLDisplay &display) {
    m_display = display;
    if (isPrimary()) {
        kwinApp()->platform()->setSceneEglDisplay(display);
    }
}

void AbstractEglBackend::setConfig(const EGLConfig &config)
{
    m_config = config;
    if (isPrimary()) {
        kwinApp()->platform()->setSceneEglConfig(config);
    }
}

void AbstractEglBackend::setSurface(const EGLSurface &surface)
{
    m_surface = surface;
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
{
    m_target = GL_TEXTURE_2D;
}

AbstractEglTexture::~AbstractEglTexture()
{
}

OpenGLBackend *AbstractEglTexture::backend()
{
    return m_backend;
}

bool AbstractEglTexture::loadTexture(WindowPixmap *pixmap)
{
    KWaylandServer::ClientBufferRef bufferRef = pixmap->buffer();
    if (bufferRef.isNull()) {
        if (updateFromFBO(pixmap->fbo())) {
            return true;
        }
        if (loadInternalImageObject(pixmap)) {
            return true;
        }
        return false;
    }
    // try Wayland loading
    if (GLuint texture = bufferRef.toOpenGLTexture(0)) {
        m_texture = texture;
        m_size = bufferRef.size();
        m_foreign = true;
        q->setWrapMode(GL_CLAMP_TO_EDGE);
        q->setFilter(GL_LINEAR);
        q->setYInverted(bufferRef.origin() == KWaylandServer::ClientBufferRef::Origin::TopLeft);
        updateMatrix();
    } else {
        qCWarning(KWIN_OPENGL) << "Failed to update scene texture for" << pixmap->surface();
    }
    return !q->isNull();
}

void AbstractEglTexture::updateTexture(WindowPixmap *pixmap)
{
    KWaylandServer::ClientBufferRef bufferRef = pixmap->buffer();
    if (bufferRef.isNull()) {
        if (updateFromFBO(pixmap->fbo())) {
            return;
        }
        if (updateFromInternalImageObject(pixmap)) {
            return;
        }
        return;
    }

    if (GLuint texture = bufferRef.toOpenGLTexture(0)) {
        m_texture = texture;
        q->setYInverted(bufferRef.origin() == KWaylandServer::ClientBufferRef::Origin::TopLeft);
    } else {
        qCWarning(KWIN_OPENGL) << "Failed to update scene texture for" << pixmap->surface();
    }
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

bool AbstractEglTexture::loadInternalImageObject(WindowPixmap *pixmap)
{
    return createTextureImage(pixmap->internalImage());
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
