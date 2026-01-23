/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2006 Lubos Lunak <l.lunak@kde.org>
    SPDX-FileCopyrightText: 2009, 2010, 2011 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "opengl/eglbackend.h"
#include "compositor.h"
#include "core/drmdevice.h"
#include "core/outputbackend.h"
#include "core/renderdevice.h"
#include "main.h"
#include "opengl/eglimagetexture.h"
#include "opengl/eglutils_p.h"
#include "utils/common.h"
#include "utils/drm_format_helper.h"
#include "wayland/drmclientbuffer.h"
#include "wayland/linux_drm_syncobj_v1.h"
#include "wayland_server.h"

#include <QElapsedTimer>

#include <drm_fourcc.h>
#include <unistd.h>
#include <xf86drm.h>

namespace KWin
{

static std::unique_ptr<EglContext> s_globalShareContext;

EglBackend::EglBackend()
{
    connect(Compositor::self(), &Compositor::aboutToDestroy, this, &EglBackend::teardown);
}

CompositingType EglBackend::compositingType() const
{
    return OpenGLCompositing;
}

bool EglBackend::checkGraphicsReset()
{
    const auto context = openglContext();
    if (context != EglContext::currentContext()) {
        const bool success = context->makeCurrent();
        if (!success) {
            // not necessarily a graphics reset, but we can't really know
            // and need to re-create everything either way
            return true;
        }
    }
    const GLenum status = context->checkGraphicsResetStatus();
    if (Q_LIKELY(status == GL_NO_ERROR)) {
        return false;
    }

    switch (status) {
    case GL_GUILTY_CONTEXT_RESET:
        qCWarning(KWIN_OPENGL) << "A graphics reset attributable to the current GL context occurred.";
        break;
    case GL_INNOCENT_CONTEXT_RESET:
        qCWarning(KWIN_OPENGL) << "A graphics reset not attributable to the current GL context occurred.";
        break;
    case GL_UNKNOWN_CONTEXT_RESET:
        qCWarning(KWIN_OPENGL) << "A graphics reset of an unknown cause occurred.";
        break;
    default:
        break;
    }

    QElapsedTimer timer;
    timer.start();

    // Wait until the reset is completed or max one second
    while (timer.elapsed() < 10000 && context->checkGraphicsResetStatus() != GL_NO_ERROR) {
        usleep(50);
    }
    if (timer.elapsed() >= 10000) {
        qCWarning(KWIN_OPENGL) << "Waiting for glGetGraphicsResetStatus to return GL_NO_ERROR timed out!";
    }

    return true;
}

bool EglBackend::ensureGlobalShareContext()
{
    if (!s_globalShareContext) {
        s_globalShareContext = EglContext::create(m_renderDevice->eglDisplay(), EGL_NO_CONFIG_KHR, EGL_NO_CONTEXT);
    }
    if (s_globalShareContext) {
        kwinApp()->outputBackend()->setSceneEglGlobalShareContext(s_globalShareContext->handle());
        return true;
    } else {
        return false;
    }
}

void EglBackend::destroyGlobalShareContext()
{
    EglDisplay *const eglDisplay = kwinApp()->outputBackend()->sceneEglDisplayObject();
    if (!eglDisplay || !s_globalShareContext) {
        return;
    }
    s_globalShareContext.reset();
    kwinApp()->outputBackend()->setSceneEglGlobalShareContext(EGL_NO_CONTEXT);
}

void EglBackend::teardown()
{
    destroyGlobalShareContext();
}

void EglBackend::cleanup()
{
    m_context.reset();
}

void EglBackend::setRenderDevice(RenderDevice *device)
{
    m_renderDevice = device;
}

void EglBackend::initWayland()
{
    if (!WaylandServer::self()) {
        return;
    }
    DrmDevice *scanoutDevice = drmDevice();
    Q_ASSERT(scanoutDevice);

    QString renderNode = m_renderDevice->eglDisplay()->renderNode();
    if (renderNode.isEmpty()) {
        ::drmDevice *device = nullptr;
        if (drmGetDeviceFromDevId(scanoutDevice->deviceId(), 0, &device) != 0) {
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
        if (waylandServer()->drm()) {
            waylandServer()->drm()->setDevice(renderNode);
        }
    } else {
        qCWarning(KWIN_OPENGL) << "No render node have been found, not initializing wl-drm";
    }

    const auto formats = m_renderDevice->eglDisplay()->allSupportedDrmFormats();
    auto filterFormats = [this, &formats](std::optional<uint32_t> bpc, bool withExternalOnlyYUV) {
        QHash<uint32_t, QList<uint64_t>> set;
        for (auto it = formats.constBegin(); it != formats.constEnd(); it++) {
            const auto info = FormatInfo::get(it.key());
            if (bpc && (!info || bpc != info->bitsPerColor)) {
                continue;
            }

            const bool externalOnlySupported = withExternalOnlyYUV && info && info->yuvConversion();
            QList<uint64_t> modifiers = externalOnlySupported ? it->allModifiers : it->nonExternalOnlyModifiers;

            if (externalOnlySupported && !modifiers.isEmpty()) {
                if (auto yuv = info->yuvConversion()) {
                    for (auto plane : std::as_const(yuv->plane)) {
                        const auto planeModifiers = formats.value(plane.format).allModifiers;
                        modifiers.erase(std::remove_if(modifiers.begin(), modifiers.end(), [&planeModifiers](uint64_t mod) {
                            return !planeModifiers.contains(mod);
                        }),
                                        modifiers.end());
                    }
                }
            }
            for (const auto &tranche : std::as_const(m_tranches)) {
                if (modifiers.isEmpty()) {
                    break;
                }
                const auto trancheModifiers = tranche.formatTable.value(it.key());
                for (auto trancheModifier : trancheModifiers) {
                    modifiers.removeAll(trancheModifier);
                }
            }
            if (modifiers.isEmpty()) {
                continue;
            }
            set.insert(it.key(), modifiers);
        }
        return set;
    };

    auto includeShaderConversions = [](QHash<uint32_t, QList<uint64_t>> &&formats) -> QHash<uint32_t, QList<uint64_t>> {
        for (auto format : s_drmConversions.keys()) {
            auto &modifiers = formats[format];
            if (modifiers.isEmpty()) {
                modifiers = {DRM_FORMAT_MOD_LINEAR};
            }
        }
        return formats;
    };

    m_tranches.append({
        .device = m_renderDevice->eglDisplay()->renderDevNode().value_or(scanoutDevice->deviceId()),
        .flags = {},
        .formatTable = filterFormats(10, false),
    });
    m_tranches.append({
        .device = m_renderDevice->eglDisplay()->renderDevNode().value_or(scanoutDevice->deviceId()),
        .flags = {},
        .formatTable = filterFormats(8, false),
    });
    m_tranches.append({
        .device = m_renderDevice->eglDisplay()->renderDevNode().value_or(scanoutDevice->deviceId()),
        .flags = {},
        .formatTable = includeShaderConversions(filterFormats({}, true)),
    });

    LinuxDmaBufV1ClientBufferIntegration *dmabuf = waylandServer()->linuxDmabuf();
    dmabuf->setRenderBackend(this);
    dmabuf->setSupportedFormatsWithModifiers(m_tranches);

    waylandServer()->setRenderBackend(this);
}

bool EglBackend::initClientExtensions()
{
    // Get the list of client extensions
    const char *clientExtensionsCString = eglQueryString(EGL_NO_DISPLAY, EGL_EXTENSIONS);
    const QByteArray clientExtensionsString = QByteArray::fromRawData(clientExtensionsCString, qstrlen(clientExtensionsCString));
    if (clientExtensionsString.isEmpty()) {
        // If eglQueryString() returned NULL, the implementation doesn't support
        // EGL_EXT_client_extensions. Expect an EGL_BAD_DISPLAY error.
        EGLint error = eglGetError();
        if (error != EGL_SUCCESS) {
            qCWarning(KWIN_OPENGL) << "Error during eglQueryString " << getEglErrorString(error);
        }
    }

    m_clientExtensions = clientExtensionsString.split(' ');

    for (const QByteArray &extension : {QByteArrayLiteral("EGL_EXT_platform_base"), QByteArrayLiteral("EGL_KHR_platform_gbm")}) {
        if (!hasClientExtension(extension)) {
            qCWarning(KWIN_OPENGL, "Required client extension %s is not supported", qPrintable(extension));
            return false;
        }
    }
    return true;
}

bool EglBackend::hasClientExtension(const QByteArray &ext) const
{
    return m_clientExtensions.contains(ext);
}

bool EglBackend::isOpenGLES() const
{
    return EglDisplay::shouldUseOpenGLES();
}

bool EglBackend::createContext()
{
    if (!ensureGlobalShareContext()) {
        return false;
    }
    m_context = EglContext::create(m_renderDevice->eglDisplay(), EGL_NO_CONFIG_KHR, s_globalShareContext ? s_globalShareContext->handle() : EGL_NO_CONTEXT);
    return m_context != nullptr;
}

QList<LinuxDmaBufV1Feedback::Tranche> EglBackend::tranches() const
{
    return m_tranches;
}

EGLImageKHR EglBackend::importBufferAsImage(GraphicsBuffer *buffer)
{
    return m_renderDevice->importBufferAsImage(buffer);
}

EGLImageKHR EglBackend::importBufferAsImage(GraphicsBuffer *buffer, int plane, int format, const QSize &size)
{
    return m_renderDevice->importBufferAsImage(buffer, plane, format, size);
}

std::shared_ptr<GLTexture> EglBackend::importDmaBufAsTexture(const DmaBufAttributes &attributes) const
{
    return m_context->importDmaBufAsTexture(attributes);
}

bool EglBackend::testImportBuffer(GraphicsBuffer *buffer)
{
    const auto nonExternalOnly = m_renderDevice->eglDisplay()->nonExternalOnlySupportedDrmFormats();
    if (auto it = nonExternalOnly.find(buffer->dmabufAttributes()->format); it != nonExternalOnly.end() && it->contains(buffer->dmabufAttributes()->modifier)) {
        return m_renderDevice->importBufferAsImage(buffer) != EGL_NO_IMAGE_KHR;
    }
    // external_only buffers aren't used as a single EGLImage, import them separately
    const auto info = FormatInfo::get(buffer->dmabufAttributes()->format);
    if (!info || !info->yuvConversion()) {
        return false;
    }
    const auto planes = info->yuvConversion()->plane;
    if (buffer->dmabufAttributes()->planeCount != planes.size()) {
        return false;
    }
    for (int i = 0; i < planes.size(); i++) {
        if (!m_renderDevice->importBufferAsImage(buffer, i, planes[i].format, QSize(buffer->size().width() / planes[i].widthDivisor, buffer->size().height() / planes[i].heightDivisor))) {
            return false;
        }
    }
    return true;
}

QHash<uint32_t, QList<uint64_t>> EglBackend::supportedFormats() const
{
    return m_renderDevice->eglDisplay()->nonExternalOnlySupportedDrmFormats();
}

EglDisplay *EglBackend::eglDisplayObject() const
{
    return m_renderDevice->eglDisplay();
}

EglContext *EglBackend::openglContext() const
{
    return m_context.get();
}

std::shared_ptr<EglContext> EglBackend::openglContextRef() const
{
    return m_context;
}

} // namespace KWin

#include "moc_eglbackend.cpp"
