/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2015 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "platformsupport/scenes/opengl/abstract_egl_backend.h"
#include "composite.h"
#include "core/outputbackend.h"
#include "options.h"
#include "utils/common.h"
#include "utils/egl_context_attribute_builder.h"
#include "wayland/display.h"
#include "wayland_server.h"
// kwin libs
#include "libkwineffects/kwineglimagetexture.h"
#include "libkwineffects/kwinglplatform.h"
#include "libkwineffects/kwinglutils.h"
#include <kwineglutils_p.h>
// Qt
#include <QOpenGLContext>

#include <memory>

#include <drm_fourcc.h>

namespace KWin
{

typedef EGLBoolean (*eglQueryDmaBufFormatsEXT_func)(EGLDisplay dpy, EGLint max_formats, EGLint *formats, EGLint *num_formats);
typedef EGLBoolean (*eglQueryDmaBufModifiersEXT_func)(EGLDisplay dpy, EGLint format, EGLint max_modifiers, EGLuint64KHR *modifiers, EGLBoolean *external_only, EGLint *num_modifiers);
eglQueryDmaBufFormatsEXT_func eglQueryDmaBufFormatsEXT = nullptr;
eglQueryDmaBufModifiersEXT_func eglQueryDmaBufModifiersEXT = nullptr;

static std::unique_ptr<EglContext> s_globalShareContext;

static bool isOpenGLES_helper()
{
    if (qstrcmp(qgetenv("KWIN_COMPOSE"), "O2ES") == 0) {
        return true;
    }
    return QOpenGLContext::openGLModuleType() == QOpenGLContext::LibGLES;
}

AbstractEglBackend::AbstractEglBackend(dev_t deviceId)
    : m_deviceId(deviceId)
{
    connect(Compositor::self(), &Compositor::aboutToDestroy, this, &AbstractEglBackend::teardown);
}

AbstractEglBackend::~AbstractEglBackend()
{
}

bool AbstractEglBackend::ensureGlobalShareContext(EGLConfig config)
{
    if (!s_globalShareContext) {
        s_globalShareContext = EglContext::create(m_display, config, EGL_NO_CONTEXT);
    }
    if (s_globalShareContext) {
        kwinApp()->outputBackend()->setSceneEglGlobalShareContext(s_globalShareContext->handle());
        return true;
    } else {
        return false;
    }
}

void AbstractEglBackend::destroyGlobalShareContext()
{
    const ::EGLDisplay eglDisplay = kwinApp()->outputBackend()->sceneEglDisplay();
    if (eglDisplay == EGL_NO_DISPLAY || !s_globalShareContext) {
        return;
    }
    s_globalShareContext.reset();
    kwinApp()->outputBackend()->setSceneEglGlobalShareContext(EGL_NO_CONTEXT);
}

void AbstractEglBackend::teardown()
{
    if (m_functions.eglUnbindWaylandDisplayWL && m_display) {
        m_functions.eglUnbindWaylandDisplayWL(m_display->handle(), *(WaylandServer::self()->display()));
    }
    destroyGlobalShareContext();
}

void AbstractEglBackend::cleanup()
{
    for (const EGLImageKHR &image : m_importedBuffers) {
        eglDestroyImageKHR(m_display, image);
    }

    cleanupSurfaces();
    cleanupGL();
    m_context.reset();
}

void AbstractEglBackend::cleanupSurfaces()
{
    if (m_surface != EGL_NO_SURFACE) {
        eglDestroySurface(m_display->handle(), m_surface);
    }
}

void AbstractEglBackend::setEglDisplay(EglDisplay *display)
{
    m_display = display;
    setExtensions(m_display->extensions());
    setSupportsNativeFence(m_display->supportsNativeFence());
    setSupportsBufferAge(m_display->supportsBufferAge());
}

typedef void (*eglFuncPtr)();
static eglFuncPtr getProcAddress(const char *name)
{
    return eglGetProcAddress(name);
}

void AbstractEglBackend::initKWinGL()
{
    GLPlatform *glPlatform = GLPlatform::instance();
    glPlatform->detect(EglPlatformInterface);
    options->setGlPreferBufferSwap(options->glPreferBufferSwap()); // resolve autosetting
    if (options->glPreferBufferSwap() == Options::AutoSwapStrategy) {
        options->setGlPreferBufferSwap('e'); // for unknown drivers - should not happen
    }
    glPlatform->printResults();
    initGL(&getProcAddress);
}

static int bpcForFormat(uint32_t format)
{
    switch (format) {
    case DRM_FORMAT_XRGB8888:
    case DRM_FORMAT_XBGR8888:
    case DRM_FORMAT_RGBX8888:
    case DRM_FORMAT_BGRX8888:
    case DRM_FORMAT_ARGB8888:
    case DRM_FORMAT_ABGR8888:
    case DRM_FORMAT_RGBA8888:
    case DRM_FORMAT_BGRA8888:
    case DRM_FORMAT_RGB888:
    case DRM_FORMAT_BGR888:
        return 8;
    case DRM_FORMAT_XRGB2101010:
    case DRM_FORMAT_XBGR2101010:
    case DRM_FORMAT_RGBX1010102:
    case DRM_FORMAT_BGRX1010102:
    case DRM_FORMAT_ARGB2101010:
    case DRM_FORMAT_ABGR2101010:
    case DRM_FORMAT_RGBA1010102:
    case DRM_FORMAT_BGRA1010102:
        return 10;
    default:
        return -1;
    }
}

void AbstractEglBackend::initWayland()
{
    if (!WaylandServer::self()) {
        return;
    }
    if (hasExtension(QByteArrayLiteral("EGL_WL_bind_wayland_display"))) {
        m_functions.eglBindWaylandDisplayWL = (eglBindWaylandDisplayWL_func)eglGetProcAddress("eglBindWaylandDisplayWL");
        m_functions.eglUnbindWaylandDisplayWL = (eglUnbindWaylandDisplayWL_func)eglGetProcAddress("eglUnbindWaylandDisplayWL");
        m_functions.eglQueryWaylandBufferWL = (eglQueryWaylandBufferWL_func)eglGetProcAddress("eglQueryWaylandBufferWL");
        // only bind if not already done
        if (waylandServer()->display()->eglDisplay() != eglDisplay()) {
            if (!m_functions.eglBindWaylandDisplayWL(eglDisplay(), *(WaylandServer::self()->display()))) {
                m_functions.eglUnbindWaylandDisplayWL = nullptr;
                m_functions.eglQueryWaylandBufferWL = nullptr;
            } else {
                waylandServer()->display()->setEglDisplay(eglDisplay());
            }
        }
    }

    if (hasExtension(QByteArrayLiteral("EGL_EXT_image_dma_buf_import")) && hasExtension(QByteArrayLiteral("EGL_EXT_image_dma_buf_import_modifiers"))) {
        eglQueryDmaBufFormatsEXT = (eglQueryDmaBufFormatsEXT_func)eglGetProcAddress("eglQueryDmaBufFormatsEXT");
        eglQueryDmaBufModifiersEXT = (eglQueryDmaBufModifiersEXT_func)eglGetProcAddress("eglQueryDmaBufModifiersEXT");

        EGLint count = 0;
        EGLBoolean success = eglQueryDmaBufFormatsEXT(m_display, 0, nullptr, &count);

        if (!success || count == 0) {
            qCCritical(KWIN_OPENGL) << "eglQueryDmaBufFormatsEXT failed:" << getEglErrorString();
            return;
        }

        QVector<uint32_t> formats(count);
        if (!eglQueryDmaBufFormatsEXT(m_display, count, (EGLint *)formats.data(), &count)) {
            qCCritical(KWIN_OPENGL) << "eglQueryDmaBufFormatsEXT with count" << count << "failed:" << getEglErrorString();
            return;
        }

        for (auto format : std::as_const(formats)) {
            EGLint count = 0;
            const EGLBoolean success = eglQueryDmaBufModifiersEXT(m_display, format, 0, nullptr, nullptr, &count);
            if (success && count > 0) {
                QVector<uint64_t> modifiers(count);
                QVector<EGLBoolean> externalOnly(count);
                if (eglQueryDmaBufModifiersEXT(m_display, format, count, modifiers.data(), externalOnly.data(), &count)) {
                    for (int i = modifiers.size() - 1; i >= 0; i--) {
                        if (externalOnly[i]) {
                            modifiers.remove(i);
                            externalOnly.remove(i);
                        }
                    }
                    if (!modifiers.empty()) {
                        m_supportedFormats.insert(format, modifiers);
                    }
                    continue;
                }
            }
            m_supportedFormats.insert(format, {DRM_FORMAT_MOD_INVALID});
        }
        qCDebug(KWIN_OPENGL) << "EGL driver advertises" << m_supportedFormats.count() << "supported dmabuf formats";

        auto filterFormats = [this](int bpc) {
            QHash<uint32_t, QVector<uint64_t>> set;
            for (auto it = m_supportedFormats.constBegin(); it != m_supportedFormats.constEnd(); it++) {
                if (bpcForFormat(it.key()) == bpc) {
                    set.insert(it.key(), it.value());
                }
            }
            return set;
        };
        if (prefer10bpc()) {
            m_tranches.append({
                .device = deviceId(),
                .flags = {},
                .formatTable = filterFormats(10),
            });
        }
        m_tranches.append({
            .device = deviceId(),
            .flags = {},
            .formatTable = filterFormats(8),
        });
        m_tranches.append({
            .device = deviceId(),
            .flags = {},
            .formatTable = filterFormats(-1),
        });

        KWaylandServer::LinuxDmaBufV1ClientBufferIntegration *dmabuf = waylandServer()->linuxDmabuf();
        dmabuf->setRenderBackend(this);
        dmabuf->setSupportedFormatsWithModifiers(m_tranches);
    }
}

void AbstractEglBackend::initClientExtensions()
{
    // Get the list of client extensions
    const char *clientExtensionsCString = eglQueryString(EGL_NO_DISPLAY, EGL_EXTENSIONS);
    const QByteArray clientExtensionsString = QByteArray::fromRawData(clientExtensionsCString, qstrlen(clientExtensionsCString));
    if (clientExtensionsString.isEmpty()) {
        // If eglQueryString() returned NULL, the implementation doesn't support
        // EGL_EXT_client_extensions. Expect an EGL_BAD_DISPLAY error.
        (void)eglGetError();
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
    return m_context->makeCurrent(m_surface);
}

void AbstractEglBackend::doneCurrent()
{
    m_context->doneCurrent();
}

bool AbstractEglBackend::isOpenGLES() const
{
    return isOpenGLES_helper();
}

bool AbstractEglBackend::createContext(EGLConfig config)
{
    if (!ensureGlobalShareContext(config)) {
        return false;
    }
    m_context = EglContext::create(m_display, config, s_globalShareContext ? s_globalShareContext->handle() : EGL_NO_CONTEXT);
    return m_context != nullptr;
}

void AbstractEglBackend::setSurface(const EGLSurface &surface)
{
    m_surface = surface;
}

QVector<KWaylandServer::LinuxDmaBufV1Feedback::Tranche> AbstractEglBackend::tranches() const
{
    return m_tranches;
}

dev_t AbstractEglBackend::deviceId() const
{
    return m_deviceId;
}

bool AbstractEglBackend::prefer10bpc() const
{
    return false;
}

EGLImageKHR AbstractEglBackend::importBufferAsImage(KWaylandServer::LinuxDmaBufV1ClientBuffer *buffer)
{
    auto it = m_importedBuffers.constFind(buffer);
    if (Q_LIKELY(it != m_importedBuffers.constEnd())) {
        return *it;
    }

    EGLImageKHR image = importDmaBufAsImage(buffer->attributes());
    if (image != EGL_NO_IMAGE_KHR) {
        m_importedBuffers[buffer] = image;
        connect(buffer, &QObject::destroyed, this, [this, buffer]() {
            eglDestroyImageKHR(m_display, m_importedBuffers.take(buffer));
        });
    }

    return image;
}

EGLImageKHR AbstractEglBackend::importDmaBufAsImage(const DmaBufAttributes &dmabuf) const
{
    return m_display->importDmaBufAsImage(dmabuf);
}

std::shared_ptr<GLTexture> AbstractEglBackend::importDmaBufAsTexture(const DmaBufAttributes &attributes) const
{
    return m_context->importDmaBufAsTexture(attributes);
}

bool AbstractEglBackend::testImportBuffer(KWaylandServer::LinuxDmaBufV1ClientBuffer *buffer)
{
    return importBufferAsImage(buffer) != EGL_NO_IMAGE_KHR;
}

QHash<uint32_t, QVector<uint64_t>> AbstractEglBackend::supportedFormats() const
{
    return m_supportedFormats;
}

::EGLDisplay AbstractEglBackend::eglDisplay() const
{
    return m_display->handle();
}

::EGLContext AbstractEglBackend::context() const
{
    return m_context->handle();
}

EGLSurface AbstractEglBackend::surface() const
{
    return m_surface;
}

EGLConfig AbstractEglBackend::config() const
{
    return m_context->config();
}

EglDisplay *AbstractEglBackend::eglDisplayObject() const
{
    return m_display;
}

EglContext *AbstractEglBackend::contextObject()
{
    return m_context.get();
}
}
