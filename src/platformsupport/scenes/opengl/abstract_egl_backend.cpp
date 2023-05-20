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
#include "wayland/drmclientbuffer.h"
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
#include <xf86drm.h>

namespace KWin
{

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
    destroyGlobalShareContext();
}

void AbstractEglBackend::cleanup()
{
    for (const EGLImageKHR &image : m_importedBuffers) {
        eglDestroyImageKHR(m_display->handle(), image);
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

    if (m_deviceId) {
        QString renderNode = m_display->renderNode();
        if (renderNode.isEmpty()) {
            drmDevice *device = nullptr;
            if (drmGetDeviceFromDevId(deviceId(), 0, &device) != 0) {
                qCWarning(KWIN_OPENGL) << "drmGetDeviceFromDevId() failed:" << strerror(errno);
            } else {
                if (device->available_nodes & (1 << DRM_NODE_RENDER)) {
                    renderNode = QString::fromLocal8Bit(device->nodes[DRM_NODE_RENDER]);
                } else if (device->available_nodes & (1 << DRM_NODE_PRIMARY)) {
                    qCWarning(KWIN_OPENGL) << "No render nodes have been found, falling back to primary node";
                    renderNode = QString::fromLocal8Bit(device->nodes[DRM_NODE_PRIMARY]);
                }
                drmFreeDevice(&device);
            }
        }

        if (!renderNode.isEmpty()) {
            waylandServer()->drm()->setDevice(renderNode);
        } else {
            qCWarning(KWIN_OPENGL) << "No render node have been found, not initializing wl-drm";
        }
    }

    auto filterFormats = [this](int bpc) {
        const auto formats = m_display->supportedDrmFormats();
        QHash<uint32_t, QVector<uint64_t>> set;
        for (auto it = formats.constBegin(); it != formats.constEnd(); it++) {
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

EGLImageKHR AbstractEglBackend::importBufferAsImage(GraphicsBuffer *buffer)
{
    auto it = m_importedBuffers.constFind(buffer);
    if (Q_LIKELY(it != m_importedBuffers.constEnd())) {
        return *it;
    }

    Q_ASSERT(buffer->dmabufAttributes());
    EGLImageKHR image = importDmaBufAsImage(*buffer->dmabufAttributes());
    if (image != EGL_NO_IMAGE_KHR) {
        m_importedBuffers[buffer] = image;
        connect(buffer, &QObject::destroyed, this, [this, buffer]() {
            eglDestroyImageKHR(m_display->handle(), m_importedBuffers.take(buffer));
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

bool AbstractEglBackend::testImportBuffer(GraphicsBuffer *buffer)
{
    return importBufferAsImage(buffer) != EGL_NO_IMAGE_KHR;
}

QHash<uint32_t, QVector<uint64_t>> AbstractEglBackend::supportedFormats() const
{
    return m_display->supportedDrmFormats();
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
