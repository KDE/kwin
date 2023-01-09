/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2015 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "abstract_egl_backend.h"
#include "composite.h"
#include "core/output.h"
#include "core/outputbackend.h"
#include "dmabuftexture.h"
#include "egl_dmabuf.h"
#include "options.h"
#include "utils/common.h"
#include "utils/egl_context_attribute_builder.h"
#include "wayland/display.h"
#include "wayland_server.h"
// kwin libs
#include <kwineglimagetexture.h>
#include <kwineglutils_p.h>
#include <kwinglplatform.h>
#include <kwinglutils.h>
// Qt
#include <QOpenGLContext>

#include <memory>

#include <drm_fourcc.h>

namespace KWin
{

static KWinEglContext s_globalShareContext;

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
    delete m_dmaBuf;
}

bool AbstractEglBackend::ensureGlobalShareContext(EGLConfig config)
{
    if (kwinApp()->outputBackend()->sceneEglGlobalShareContext() != EGL_NO_CONTEXT) {
        return true;
    }

    if (!s_globalShareContext.init(&m_display, config, EGL_NO_CONTEXT)) {
        return false;
    }
    kwinApp()->outputBackend()->setSceneEglGlobalShareContext(s_globalShareContext.context());
    return true;
}

void AbstractEglBackend::destroyGlobalShareContext()
{
    const EGLDisplay eglDisplay = kwinApp()->outputBackend()->sceneEglDisplay();
    if (eglDisplay == EGL_NO_DISPLAY || !s_globalShareContext.isValid()) {
        return;
    }
    s_globalShareContext = {};
    kwinApp()->outputBackend()->setSceneEglGlobalShareContext(EGL_NO_CONTEXT);
}

void AbstractEglBackend::teardown()
{
    if (m_functions.eglUnbindWaylandDisplayWL && m_display.isValid()) {
        m_functions.eglUnbindWaylandDisplayWL(m_display.display(), *(WaylandServer::self()->display()));
    }
    destroyGlobalShareContext();
}

void AbstractEglBackend::cleanup()
{
    cleanupSurfaces();
    cleanupGL();
    doneCurrent();
    m_context.destroy();
    eglReleaseThread();
}

void AbstractEglBackend::cleanupSurfaces()
{
    if (m_surface != EGL_NO_SURFACE) {
        eglDestroySurface(m_display.display(), m_surface);
    }
}

bool AbstractEglBackend::initEglAPI(EGLDisplay display)
{
    if (!m_display.init(display)) {
        return false;
    }
    kwinApp()->outputBackend()->setSceneEglDisplay(display);
    setExtensions(m_display.extensions());
    setSupportsNativeFence(hasExtension(QByteArrayLiteral("EGL_ANDROID_native_fence_sync")));
    setSupportsBufferAge(m_display.supportsBufferAge());
    setSupportsPartialUpdate(m_display.supportsPartialUpdate());
    setSupportsSwapBuffersWithDamage(m_display.supportsSwapBuffersWithDamage());
    return true;
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

    Q_ASSERT(!m_dmaBuf);
    m_dmaBuf = EglDmabuf::factory(this);
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
    return m_context.makeCurrent(m_surface);
}

void AbstractEglBackend::doneCurrent()
{
    m_context.doneCurrent();
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
    return m_context.init(&m_display, config, s_globalShareContext.context());
}

void AbstractEglBackend::setSurface(const EGLSurface &surface)
{
    m_surface = surface;
}

std::shared_ptr<GLTexture> AbstractEglBackend::textureForOutput(Output *requestedOutput) const
{
    std::shared_ptr<GLTexture> texture(new GLTexture(GL_RGBA8, requestedOutput->pixelSize()));
    GLFramebuffer renderTarget(texture.get());
    renderTarget.blitFromFramebuffer(QRect(0, texture->height(), texture->width(), -texture->height()));
    return texture;
}

dev_t AbstractEglBackend::deviceId() const
{
    return m_deviceId;
}

bool AbstractEglBackend::prefer10bpc() const
{
    return false;
}

EglDmabuf *AbstractEglBackend::dmabuf() const
{
    return m_dmaBuf;
}

EGLImageKHR AbstractEglBackend::importDmaBufAsImage(const DmaBufAttributes &dmabuf) const
{
    return m_context.importDmaBufAsImage(dmabuf);
}

std::shared_ptr<GLTexture> AbstractEglBackend::importDmaBufAsTexture(const DmaBufAttributes &attributes) const
{
    return m_context.importDmaBufAsTexture(attributes);
}

QHash<uint32_t, QVector<uint64_t>> AbstractEglBackend::supportedFormats() const
{
    if (m_dmaBuf) {
        return m_dmaBuf->supportedFormats();
    } else {
        return RenderBackend::supportedFormats();
    }
}

EGLDisplay AbstractEglBackend::eglDisplay() const
{
    return m_display.display();
}

EGLConfig AbstractEglBackend::config() const
{
    return m_context.config();
}

EGLContext AbstractEglBackend::context() const
{
    return m_context.context();
}

KWinEglContext *AbstractEglBackend::contextObject()
{
    return &m_context;
}
}
