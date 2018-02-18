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
#include "texture.h"
#include "composite.h"
#include "egl_context_attribute_builder.h"
#include "options.h"
#include "platform.h"
#include "scene.h"
#include "wayland_server.h"
#include <KWayland/Server/buffer_interface.h>
#include <KWayland/Server/display.h>
#include <KWayland/Server/surface_interface.h>
// kwin libs
#include <logging.h>
#include <kwinglplatform.h>
#include <kwinglutils.h>
// Qt
#include <QOpenGLContext>
#include <QOpenGLFramebufferObject>

#include <unistd.h>

#include <memory>

namespace KWin
{

typedef GLboolean(*eglBindWaylandDisplayWL_func)(EGLDisplay dpy, wl_display *display);
typedef GLboolean(*eglUnbindWaylandDisplayWL_func)(EGLDisplay dpy, wl_display *display);
typedef GLboolean(*eglQueryWaylandBufferWL_func)(EGLDisplay dpy, struct wl_resource *buffer, EGLint attribute, EGLint *value);
eglBindWaylandDisplayWL_func eglBindWaylandDisplayWL = nullptr;
eglUnbindWaylandDisplayWL_func eglUnbindWaylandDisplayWL = nullptr;
eglQueryWaylandBufferWL_func eglQueryWaylandBufferWL = nullptr;

typedef EGLBoolean (*eglQueryDmaBufFormatsEXT_func) (EGLDisplay dpy, EGLint max_formats, EGLint *formats, EGLint *num_formats);
typedef EGLBoolean (*eglQueryDmaBufModifiersEXT_func) (EGLDisplay dpy, EGLint format, EGLint max_modifiers, EGLuint64KHR *modifiers, EGLBoolean *external_only, EGLint *num_modifiers);
eglQueryDmaBufFormatsEXT_func eglQueryDmaBufFormatsEXT = nullptr;
eglQueryDmaBufModifiersEXT_func eglQueryDmaBufModifiersEXT = nullptr;

#ifndef EGL_WAYLAND_BUFFER_WL
#define EGL_WAYLAND_BUFFER_WL                   0x31D5
#endif
#ifndef EGL_WAYLAND_PLANE_WL
#define EGL_WAYLAND_PLANE_WL                    0x31D6
#endif
#ifndef EGL_WAYLAND_Y_INVERTED_WL
#define EGL_WAYLAND_Y_INVERTED_WL               0x31DB
#endif

#ifndef EGL_EXT_image_dma_buf_import
#define EGL_LINUX_DMA_BUF_EXT                     0x3270
#define EGL_LINUX_DRM_FOURCC_EXT                  0x3271
#define EGL_DMA_BUF_PLANE0_FD_EXT                 0x3272
#define EGL_DMA_BUF_PLANE0_OFFSET_EXT             0x3273
#define EGL_DMA_BUF_PLANE0_PITCH_EXT              0x3274
#define EGL_DMA_BUF_PLANE1_FD_EXT                 0x3275
#define EGL_DMA_BUF_PLANE1_OFFSET_EXT             0x3276
#define EGL_DMA_BUF_PLANE1_PITCH_EXT              0x3277
#define EGL_DMA_BUF_PLANE2_FD_EXT                 0x3278
#define EGL_DMA_BUF_PLANE2_OFFSET_EXT             0x3279
#define EGL_DMA_BUF_PLANE2_PITCH_EXT              0x327A
#define EGL_YUV_COLOR_SPACE_HINT_EXT              0x327B
#define EGL_SAMPLE_RANGE_HINT_EXT                 0x327C
#define EGL_YUV_CHROMA_HORIZONTAL_SITING_HINT_EXT 0x327D
#define EGL_YUV_CHROMA_VERTICAL_SITING_HINT_EXT   0x327E
#define EGL_ITU_REC601_EXT                        0x327F
#define EGL_ITU_REC709_EXT                        0x3280
#define EGL_ITU_REC2020_EXT                       0x3281
#define EGL_YUV_FULL_RANGE_EXT                    0x3282
#define EGL_YUV_NARROW_RANGE_EXT                  0x3283
#define EGL_YUV_CHROMA_SITING_0_EXT               0x3284
#define EGL_YUV_CHROMA_SITING_0_5_EXT             0x3285
#endif // EGL_EXT_image_dma_buf_import

#ifndef EGL_EXT_image_dma_buf_import_modifiers
#define EGL_DMA_BUF_PLANE3_FD_EXT                 0x3440
#define EGL_DMA_BUF_PLANE3_OFFSET_EXT             0x3441
#define EGL_DMA_BUF_PLANE3_PITCH_EXT              0x3442
#define EGL_DMA_BUF_PLANE0_MODIFIER_LO_EXT        0x3443
#define EGL_DMA_BUF_PLANE0_MODIFIER_HI_EXT        0x3444
#define EGL_DMA_BUF_PLANE1_MODIFIER_LO_EXT        0x3445
#define EGL_DMA_BUF_PLANE1_MODIFIER_HI_EXT        0x3446
#define EGL_DMA_BUF_PLANE2_MODIFIER_LO_EXT        0x3447
#define EGL_DMA_BUF_PLANE2_MODIFIER_HI_EXT        0x3448
#define EGL_DMA_BUF_PLANE3_MODIFIER_LO_EXT        0x3449
#define EGL_DMA_BUF_PLANE3_MODIFIER_HI_EXT        0x344A
#endif // EGL_EXT_image_dma_buf_import_modifiers

AbstractEglBackend::AbstractEglBackend()
    : QObject(nullptr)
    , OpenGLBackend()
{
    connect(Compositor::self(), &Compositor::aboutToDestroy, this, &AbstractEglBackend::unbindWaylandDisplay);
}

AbstractEglBackend::~AbstractEglBackend()
{
    for (auto *dmabuf : qAsConst(m_dmabufBuffers)) {
        dmabuf->destroyImage();
    }
}

void AbstractEglBackend::unbindWaylandDisplay()
{
    if (eglUnbindWaylandDisplayWL && m_display != EGL_NO_DISPLAY) {
        eglUnbindWaylandDisplayWL(m_display, *(WaylandServer::self()->display()));
    }
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

    if (hasExtension(QByteArrayLiteral("EGL_EXT_image_dma_buf_import_modifiers"))) {
        eglQueryDmaBufFormatsEXT = (eglQueryDmaBufFormatsEXT_func) eglGetProcAddress("eglQueryDmaBufFormatsEXT");
        eglQueryDmaBufModifiersEXT = (eglQueryDmaBufModifiersEXT_func) eglGetProcAddress("eglQueryDmaBufModifiersEXT");
    }

    m_haveDmabufImport = hasExtension(QByteArrayLiteral("EGL_EXT_image_dma_buf_import"));
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
    const bool haveRobustness = hasExtension(QByteArrayLiteral("EGL_EXT_create_context_robustness"));
    const bool haveCreateContext = hasExtension(QByteArrayLiteral("EGL_KHR_create_context"));

    std::vector<std::unique_ptr<AbstractOpenGLContextAttributeBuilder>> candidates;
    if (isOpenGLES()) {
        if (haveCreateContext && haveRobustness) {
            auto glesRobust = std::unique_ptr<AbstractOpenGLContextAttributeBuilder>(new EglOpenGLESContextAttributeBuilder);
            glesRobust->setVersion(2);
            glesRobust->setRobust(true);
            candidates.push_back(std::move(glesRobust));
        }
        auto gles = std::unique_ptr<AbstractOpenGLContextAttributeBuilder>(new EglOpenGLESContextAttributeBuilder);
        gles->setVersion(2);
        candidates.push_back(std::move(gles));
    } else {
        if (options->glCoreProfile() && haveCreateContext) {
            if (haveRobustness) {
                auto robustCore = std::unique_ptr<AbstractOpenGLContextAttributeBuilder>(new EglContextAttributeBuilder);
                robustCore->setVersion(3, 1);
                robustCore->setRobust(true);
                candidates.push_back(std::move(robustCore));
            }
            auto core = std::unique_ptr<AbstractOpenGLContextAttributeBuilder>(new EglContextAttributeBuilder);
            core->setVersion(3, 1);
            candidates.push_back(std::move(core));
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
        ctx = eglCreateContext(m_display, config(), EGL_NO_CONTEXT, attribs.data());
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

void AbstractEglBackend::aboutToDestroy(EglDmabufBuffer *buffer)
{
    m_dmabufBuffers.removeOne(buffer);
}

QVector<uint32_t> AbstractEglBackend::supportedDrmFormats()
{
    if (!m_haveDmabufImport || eglQueryDmaBufFormatsEXT == nullptr)
        return QVector<uint32_t>();

    EGLint count = 0;
    EGLBoolean success = eglQueryDmaBufFormatsEXT(m_display, 0, NULL, &count);

    if (success && count > 0) {
        QVector<uint32_t> formats(count);
        if (eglQueryDmaBufFormatsEXT(m_display, count, (EGLint *) formats.data(), &count)) {
            return formats;
        }
    }

    return QVector<uint32_t>();
}

QVector<uint64_t> AbstractEglBackend::supportedDrmModifiers(uint32_t format)
{
    if (!m_haveDmabufImport || eglQueryDmaBufModifiersEXT == nullptr)
        return QVector<uint64_t>();

    EGLint count = 0;
    EGLBoolean success = eglQueryDmaBufModifiersEXT(m_display, format, 0, NULL, NULL, &count);

    if (success && count > 0) {
        QVector<uint64_t> modifiers(count);
        if (eglQueryDmaBufModifiersEXT(m_display, format, count, modifiers.data(), NULL, &count)) {
            return modifiers;
        }
    }

    return QVector<uint64_t>();
}

KWayland::Server::LinuxDmabuf::Buffer *AbstractEglBackend::importDmabufBuffer(const QVector<KWayland::Server::LinuxDmabuf::Plane> &planes,
                                                                              uint32_t format,
                                                                              const QSize &size,
                                                                              KWayland::Server::LinuxDmabuf::Flags flags)
{
    if (!m_haveDmabufImport)
        return nullptr;

    // FIXME: Add support for multi-planar images
    if (planes.count() != 1)
        return nullptr;

    const EGLint attribs[] = {
        EGL_WIDTH,                          size.width(),
        EGL_HEIGHT,                         size.height(),
        EGL_LINUX_DRM_FOURCC_EXT,           EGLint(format),
        EGL_DMA_BUF_PLANE0_FD_EXT,          planes[0].fd,
        EGL_DMA_BUF_PLANE0_OFFSET_EXT,      EGLint(planes[0].offset),
        EGL_DMA_BUF_PLANE0_PITCH_EXT,       EGLint(planes[0].stride),
        EGL_DMA_BUF_PLANE0_MODIFIER_LO_EXT, EGLint(planes[0].modifier & 0xffffffff),
        EGL_DMA_BUF_PLANE0_MODIFIER_HI_EXT, EGLint(planes[0].modifier >> 32),
        EGL_NONE
    };

    // Note that the EGLImage does NOT take onwership of the file descriptors
    EGLImage image = eglCreateImageKHR(m_display, EGL_NO_CONTEXT, EGL_LINUX_DMA_BUF_EXT, (EGLClientBuffer) nullptr, attribs);
    if (image == EGL_NO_IMAGE_KHR)
        return nullptr;

    EglDmabufBuffer *buffer = new EglDmabufBuffer(image, planes, format, size, flags, this);

    // We keep a list of the buffers we have imported so we can clean up the EGL images
    // from the AbstractEglBackend destructor.
    m_dmabufBuffers.append(buffer);

    return buffer;
}



// --------------------------------------------------------------------



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
    if (buffer->linuxDmabufBuffer()) {
        return loadDmabufTexture(buffer);
    } else if (buffer->shmBuffer()) {
        return loadShmTexture(buffer);
    }

    return loadEglTexture(buffer);
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
    if (EglDmabufBuffer *dmabuf = static_cast<EglDmabufBuffer *>(buffer->linuxDmabufBuffer())) {
        q->bind();
        glEGLImageTargetTexture2DOES(GL_TEXTURE_2D, (GLeglImageOES) dmabuf->image());
        q->unbind();
        if (m_image != EGL_NO_IMAGE_KHR) {
            eglDestroyImageKHR(m_backend->eglDisplay(), m_image);
        }
        m_image = EGL_NO_IMAGE_KHR; // The wl_buffer has ownership of the image
        const bool yInverted = dmabuf->flags() & KWayland::Server::LinuxDmabuf::YInverted;
        if (m_size != dmabuf->size() || yInverted != q->isYInverted()) {
            m_size = dmabuf->size();
            q->setYInverted(yInverted);
        }
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
    q->bind();
    const QRegion damage = s->trackedDamage();
    s->resetTrackedDamage();
    auto scale = s->scale(); //damage is normalised, so needs converting up to match texture

    // TODO: this should be shared with GLTexture::update
    if (GLPlatform::instance()->isGLES()) {
        if (s_supportsARGB32 && (image.format() == QImage::Format_ARGB32 || image.format() == QImage::Format_ARGB32_Premultiplied)) {
            const QImage im = image.convertToFormat(QImage::Format_ARGB32_Premultiplied);
            for (const QRect &rect : damage.rects()) {
                auto scaledRect = QRect(rect.x() * scale, rect.y() * scale, rect.width() * scale, rect.height() * scale);
                glTexSubImage2D(m_target, 0, scaledRect.x(), scaledRect.y(), scaledRect.width(), scaledRect.height(),
                                GL_BGRA_EXT, GL_UNSIGNED_BYTE, im.copy(scaledRect).bits());
            }
        } else {
            const QImage im = image.convertToFormat(QImage::Format_RGBA8888_Premultiplied);
            for (const QRect &rect : damage.rects()) {
                auto scaledRect = QRect(rect.x() * scale, rect.y() * scale, rect.width() * scale, rect.height() * scale);
                glTexSubImage2D(m_target, 0, scaledRect.x(), scaledRect.y(), scaledRect.width(), scaledRect.height(),
                                GL_RGBA, GL_UNSIGNED_BYTE, im.copy(scaledRect).bits());
            }
        }
    } else {
        const QImage im = image.convertToFormat(QImage::Format_ARGB32_Premultiplied);
        for (const QRect &rect : damage.rects()) {
            auto scaledRect = QRect(rect.x() * scale, rect.y() * scale, rect.width() * scale, rect.height() * scale);
            glTexSubImage2D(m_target, 0, scaledRect.x(), scaledRect.y(), scaledRect.width(), scaledRect.height(),
                            GL_BGRA, GL_UNSIGNED_BYTE, im.copy(scaledRect).bits());
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
        qCDebug(KWIN_OPENGL) << "failed to create egl image";
        q->discard();
        return false;
    }

    return true;
}

bool AbstractEglTexture::loadDmabufTexture(const QPointer< KWayland::Server::BufferInterface > &buffer)
{
    EglDmabufBuffer *dmabuf = static_cast<EglDmabufBuffer *>(buffer->linuxDmabufBuffer());
    if (!dmabuf || dmabuf->image() == EGL_NO_IMAGE_KHR) {
        qCritical(KWIN_OPENGL) << "Invalid dmabuf-based wl_buffer";
        q->discard();
        return false;
    }

    assert(m_image == EGL_NO_IMAGE_KHR);

    glGenTextures(1, &m_texture);
    q->setWrapMode(GL_CLAMP_TO_EDGE);
    q->setFilter(GL_NEAREST);
    q->bind();
    glEGLImageTargetTexture2DOES(GL_TEXTURE_2D, (GLeglImageOES) dmabuf->image());
    q->unbind();

    m_size = dmabuf->size();
    q->setYInverted(dmabuf->flags() & KWayland::Server::LinuxDmabuf::YInverted);

    return true;
}

EGLImageKHR AbstractEglTexture::attach(const QPointer< KWayland::Server::BufferInterface > &buffer)
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



// --------------------------------------------------------------------



EglDmabufBuffer::EglDmabufBuffer(EGLImage image,
                                 const QVector<KWayland::Server::LinuxDmabuf::Plane> &planes,
                                 uint32_t format,
                                 const QSize &size,
                                 KWayland::Server::LinuxDmabuf::Flags flags,
                                 AbstractEglBackend *backend)
    : KWayland::Server::LinuxDmabuf::Buffer(format, size),
      m_backend(backend),
      m_image(image),
      m_planes(planes),
      m_flags(flags)
{
}

EglDmabufBuffer::~EglDmabufBuffer()
{
    if (m_backend) {
        m_backend->aboutToDestroy(this);

        assert(m_image != EGL_NO_IMAGE_KHR);
        eglDestroyImageKHR(m_backend->eglDisplay(), m_image);
    }

    // Close all open file descriptors
    for (int i = 0; i < m_planes.count(); i++) {
        if (m_planes[i].fd != -1)
            ::close(m_planes[i].fd);
        m_planes[i].fd = -1;
    }
}

void EglDmabufBuffer::destroyImage()
{
    assert(m_image != EGL_NO_IMAGE_KHR);
    eglDestroyImageKHR(m_backend->eglDisplay(), m_image);
    m_image = EGL_NO_IMAGE_KHR;
    m_backend = nullptr;
}


} // namespace KWin

