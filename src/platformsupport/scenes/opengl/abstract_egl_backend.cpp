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

static EGLContext s_globalShareContext = EGL_NO_CONTEXT;

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

EGLContext AbstractEglBackend::ensureGlobalShareContext()
{
    if (kwinApp()->outputBackend()->sceneEglGlobalShareContext() != EGL_NO_CONTEXT) {
        return kwinApp()->outputBackend()->sceneEglGlobalShareContext();
    }

    s_globalShareContext = createContextInternal(EGL_NO_CONTEXT);
    kwinApp()->outputBackend()->setSceneEglGlobalShareContext(s_globalShareContext);
    return s_globalShareContext;
}

void AbstractEglBackend::destroyGlobalShareContext()
{
    const EGLDisplay eglDisplay = kwinApp()->outputBackend()->sceneEglDisplay();
    if (eglDisplay == EGL_NO_DISPLAY || s_globalShareContext == EGL_NO_CONTEXT) {
        return;
    }
    eglDestroyContext(eglDisplay, s_globalShareContext);
    s_globalShareContext = EGL_NO_CONTEXT;
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
    eglDestroyContext(m_display.display(), m_context);
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
    const bool current = eglMakeCurrent(m_display.display(), m_surface, m_surface, m_context);
    return current;
}

void AbstractEglBackend::doneCurrent()
{
    eglMakeCurrent(m_display.display(), EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
}

bool AbstractEglBackend::isOpenGLES() const
{
    return isOpenGLES_helper();
}

bool AbstractEglBackend::createContext()
{
    EGLContext globalShareContext = ensureGlobalShareContext();
    if (globalShareContext == EGL_NO_CONTEXT) {
        return false;
    }
    m_context = createContextInternal(globalShareContext);
    if (m_context == EGL_NO_CONTEXT) {
        return false;
    }
    return true;
}

EGLContext AbstractEglBackend::createContextInternal(EGLContext sharedContext)
{
    const bool haveRobustness = hasExtension(QByteArrayLiteral("EGL_EXT_create_context_robustness"));
    const bool haveCreateContext = hasExtension(QByteArrayLiteral("EGL_KHR_create_context"));
    const bool haveContextPriority = hasExtension(QByteArrayLiteral("EGL_IMG_context_priority"));
    const bool haveResetOnVideoMemoryPurge = hasExtension(QByteArrayLiteral("EGL_NV_robustness_video_memory_purge"));

    std::vector<std::unique_ptr<AbstractOpenGLContextAttributeBuilder>> candidates;
    if (isOpenGLES()) {
        if (haveCreateContext && haveRobustness && haveContextPriority && haveResetOnVideoMemoryPurge) {
            auto glesRobustPriority = std::make_unique<EglOpenGLESContextAttributeBuilder>();
            glesRobustPriority->setResetOnVideoMemoryPurge(true);
            glesRobustPriority->setVersion(2);
            glesRobustPriority->setRobust(true);
            glesRobustPriority->setHighPriority(true);
            candidates.push_back(std::move(glesRobustPriority));
        }

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
        if (haveCreateContext) {
            if (haveRobustness && haveContextPriority && haveResetOnVideoMemoryPurge) {
                auto robustCorePriority = std::make_unique<EglContextAttributeBuilder>();
                robustCorePriority->setResetOnVideoMemoryPurge(true);
                robustCorePriority->setVersion(3, 1);
                robustCorePriority->setRobust(true);
                robustCorePriority->setHighPriority(true);
                candidates.push_back(std::move(robustCorePriority));
            }
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
        ctx = eglCreateContext(m_display.display(), config(), sharedContext, attribs.data());
        if (ctx != EGL_NO_CONTEXT) {
            qCDebug(KWIN_OPENGL) << "Created EGL context with attributes:" << (*it).get();
            break;
        }
    }

    if (ctx == EGL_NO_CONTEXT) {
        qCCritical(KWIN_OPENGL) << "Create Context failed";
    }
    return ctx;
}

void AbstractEglBackend::setConfig(const EGLConfig &config)
{
    m_config = config;
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
    QVector<EGLint> attribs;
    attribs.reserve(6 + dmabuf.planeCount * 10 + 1);

    attribs
        << EGL_WIDTH << dmabuf.width
        << EGL_HEIGHT << dmabuf.height
        << EGL_LINUX_DRM_FOURCC_EXT << dmabuf.format;

    attribs
        << EGL_DMA_BUF_PLANE0_FD_EXT << dmabuf.fd[0].get()
        << EGL_DMA_BUF_PLANE0_OFFSET_EXT << dmabuf.offset[0]
        << EGL_DMA_BUF_PLANE0_PITCH_EXT << dmabuf.pitch[0];
    if (dmabuf.modifier != DRM_FORMAT_MOD_INVALID) {
        attribs
            << EGL_DMA_BUF_PLANE0_MODIFIER_LO_EXT << EGLint(dmabuf.modifier & 0xffffffff)
            << EGL_DMA_BUF_PLANE0_MODIFIER_HI_EXT << EGLint(dmabuf.modifier >> 32);
    }

    if (dmabuf.planeCount > 1) {
        attribs
            << EGL_DMA_BUF_PLANE1_FD_EXT << dmabuf.fd[1].get()
            << EGL_DMA_BUF_PLANE1_OFFSET_EXT << dmabuf.offset[1]
            << EGL_DMA_BUF_PLANE1_PITCH_EXT << dmabuf.pitch[1];
        if (dmabuf.modifier != DRM_FORMAT_MOD_INVALID) {
            attribs
                << EGL_DMA_BUF_PLANE1_MODIFIER_LO_EXT << EGLint(dmabuf.modifier & 0xffffffff)
                << EGL_DMA_BUF_PLANE1_MODIFIER_HI_EXT << EGLint(dmabuf.modifier >> 32);
        }
    }

    if (dmabuf.planeCount > 2) {
        attribs
            << EGL_DMA_BUF_PLANE2_FD_EXT << dmabuf.fd[2].get()
            << EGL_DMA_BUF_PLANE2_OFFSET_EXT << dmabuf.offset[2]
            << EGL_DMA_BUF_PLANE2_PITCH_EXT << dmabuf.pitch[2];
        if (dmabuf.modifier != DRM_FORMAT_MOD_INVALID) {
            attribs
                << EGL_DMA_BUF_PLANE2_MODIFIER_LO_EXT << EGLint(dmabuf.modifier & 0xffffffff)
                << EGL_DMA_BUF_PLANE2_MODIFIER_HI_EXT << EGLint(dmabuf.modifier >> 32);
        }
    }

    if (dmabuf.planeCount > 3) {
        attribs
            << EGL_DMA_BUF_PLANE3_FD_EXT << dmabuf.fd[3].get()
            << EGL_DMA_BUF_PLANE3_OFFSET_EXT << dmabuf.offset[3]
            << EGL_DMA_BUF_PLANE3_PITCH_EXT << dmabuf.pitch[3];
        if (dmabuf.modifier != DRM_FORMAT_MOD_INVALID) {
            attribs
                << EGL_DMA_BUF_PLANE3_MODIFIER_LO_EXT << EGLint(dmabuf.modifier & 0xffffffff)
                << EGL_DMA_BUF_PLANE3_MODIFIER_HI_EXT << EGLint(dmabuf.modifier >> 32);
        }
    }

    attribs << EGL_NONE;

    return eglCreateImageKHR(m_display.display(), EGL_NO_CONTEXT, EGL_LINUX_DMA_BUF_EXT, nullptr, attribs.data());
}

std::shared_ptr<GLTexture> AbstractEglBackend::importDmaBufAsTexture(const DmaBufAttributes &attributes) const
{
    EGLImageKHR image = importDmaBufAsImage(attributes);
    if (image != EGL_NO_IMAGE_KHR) {
        return std::make_shared<EGLImageTexture>(eglDisplay(), image, GL_RGBA8, QSize(attributes.width, attributes.height));
    } else {
        qCWarning(KWIN_OPENGL) << "Failed to record frame: Error creating EGLImageKHR - " << getEglErrorString();
        return nullptr;
    }
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
}
