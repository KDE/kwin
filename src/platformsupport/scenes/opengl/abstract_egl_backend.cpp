/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2015 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "platformsupport/scenes/opengl/abstract_egl_backend.h"
#include "compositor.h"
#include "core/drmdevice.h"
#include "core/outputbackend.h"
#include "main.h"
#include "opengl/egl_context_attribute_builder.h"
#include "utils/common.h"
#include "wayland/drmclientbuffer.h"
#include "wayland/linux_drm_syncobj_v1.h"
#include "wayland_server.h"
// kwin libs
#include "opengl/eglimagetexture.h"
#include "opengl/eglutils_p.h"
#include "opengl/glplatform.h"
#include "opengl/glutils.h"
#include "utils/drm_format_helper.h"
// Qt
#include <QOpenGLContext>

#include <memory>

#include <drm_fourcc.h>
#include <xf86drm.h>

namespace KWin
{

static std::unique_ptr<EglContext> s_globalShareContext;

AbstractEglBackend::AbstractEglBackend()
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
    EglDisplay *const eglDisplay = kwinApp()->outputBackend()->sceneEglDisplayObject();
    if (!eglDisplay || !s_globalShareContext) {
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
        m_display->destroyImage(image);
    }

    cleanupSurfaces();
    m_context.reset();
}

void AbstractEglBackend::cleanupSurfaces()
{
}

void AbstractEglBackend::setEglDisplay(EglDisplay *display)
{
    m_display = display;
    setExtensions(m_display->extensions());
    setSupportsNativeFence(m_display->supportsNativeFence());
    setSupportsBufferAge(m_display->supportsBufferAge());
}

void AbstractEglBackend::initWayland()
{
    if (!WaylandServer::self()) {
        return;
    }

    if (DrmDevice *scanoutDevice = drmDevice()) {
        QString renderNode = m_display->renderNode();
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
            waylandServer()->drm()->setDevice(renderNode);
        } else {
            qCWarning(KWIN_OPENGL) << "No render node have been found, not initializing wl-drm";
        }

        const auto formats = m_display->allSupportedDrmFormats();
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
            .device = m_display->renderDevNode().value_or(scanoutDevice->deviceId()),
            .flags = {},
            .formatTable = filterFormats(10, false),
        });
        m_tranches.append({
            .device = m_display->renderDevNode().value_or(scanoutDevice->deviceId()),
            .flags = {},
            .formatTable = filterFormats(8, false),
        });
        m_tranches.append({
            .device = m_display->renderDevNode().value_or(scanoutDevice->deviceId()),
            .flags = {},
            .formatTable = includeShaderConversions(filterFormats({}, true)),
        });

        LinuxDmaBufV1ClientBufferIntegration *dmabuf = waylandServer()->linuxDmabuf();
        dmabuf->setRenderBackend(this);
        dmabuf->setSupportedFormatsWithModifiers(m_tranches);
    }
    waylandServer()->setRenderBackend(this);
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
    return m_context->makeCurrent();
}

void AbstractEglBackend::doneCurrent()
{
    m_context->doneCurrent();
}

bool AbstractEglBackend::isOpenGLES() const
{
    return EglDisplay::shouldUseOpenGLES();
}

bool AbstractEglBackend::createContext(EGLConfig config)
{
    if (!ensureGlobalShareContext(config)) {
        return false;
    }
    m_context = EglContext::create(m_display, config, s_globalShareContext ? s_globalShareContext->handle() : EGL_NO_CONTEXT);
    return m_context != nullptr;
}

QList<LinuxDmaBufV1Feedback::Tranche> AbstractEglBackend::tranches() const
{
    return m_tranches;
}

EGLImageKHR AbstractEglBackend::importBufferAsImage(GraphicsBuffer *buffer, int plane, int format, const QSize &size)
{
    std::pair key(buffer, plane);
    auto it = m_importedBuffers.constFind(key);
    if (Q_LIKELY(it != m_importedBuffers.constEnd())) {
        return *it;
    }

    Q_ASSERT(buffer->dmabufAttributes());
    EGLImageKHR image = importDmaBufAsImage(*buffer->dmabufAttributes(), plane, format, size);
    if (image != EGL_NO_IMAGE_KHR) {
        m_importedBuffers[key] = image;
        connect(buffer, &QObject::destroyed, this, [this, key]() {
            m_display->destroyImage(m_importedBuffers.take(key));
        });
    } else {
        qCWarning(KWIN_OPENGL) << "failed to import dmabuf" << buffer;
    }

    return image;
}

EGLImageKHR AbstractEglBackend::importBufferAsImage(GraphicsBuffer *buffer)
{
    auto key = std::pair(buffer, 0);
    auto it = m_importedBuffers.constFind(key);
    if (Q_LIKELY(it != m_importedBuffers.constEnd())) {
        return *it;
    }

    Q_ASSERT(buffer->dmabufAttributes());
    EGLImageKHR image = importDmaBufAsImage(*buffer->dmabufAttributes());
    if (image != EGL_NO_IMAGE_KHR) {
        m_importedBuffers[key] = image;
        connect(buffer, &QObject::destroyed, this, [this, key]() {
            m_display->destroyImage(m_importedBuffers.take(key));
        });
    } else {
        qCWarning(KWIN_OPENGL) << "failed to import dmabuf" << buffer;
    }

    return image;
}

EGLImageKHR AbstractEglBackend::importDmaBufAsImage(const DmaBufAttributes &dmabuf) const
{
    return m_display->importDmaBufAsImage(dmabuf);
}

EGLImageKHR AbstractEglBackend::importDmaBufAsImage(const DmaBufAttributes &dmabuf, int plane, int format, const QSize &size) const
{
    return m_display->importDmaBufAsImage(dmabuf, plane, format, size);
}

std::shared_ptr<GLTexture> AbstractEglBackend::importDmaBufAsTexture(const DmaBufAttributes &attributes) const
{
    return m_context->importDmaBufAsTexture(attributes);
}

bool AbstractEglBackend::testImportBuffer(GraphicsBuffer *buffer)
{
    const auto nonExternalOnly = m_display->nonExternalOnlySupportedDrmFormats();
    if (auto it = nonExternalOnly.find(buffer->dmabufAttributes()->format); it != nonExternalOnly.end() && it->contains(buffer->dmabufAttributes()->modifier)) {
        return importBufferAsImage(buffer) != EGL_NO_IMAGE_KHR;
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
        if (!importBufferAsImage(buffer, i, planes[i].format, QSize(buffer->size().width() / planes[i].widthDivisor, buffer->size().height() / planes[i].heightDivisor))) {
            return false;
        }
    }
    return true;
}

QHash<uint32_t, QList<uint64_t>> AbstractEglBackend::supportedFormats() const
{
    return m_display->nonExternalOnlySupportedDrmFormats();
}

EGLConfig AbstractEglBackend::config() const
{
    return m_context->config();
}

EglDisplay *AbstractEglBackend::eglDisplayObject() const
{
    return m_display;
}

EglContext *AbstractEglBackend::openglContext() const
{
    return m_context.get();
}

std::shared_ptr<EglContext> AbstractEglBackend::openglContextRef() const
{
    return m_context;
}
}

#include "moc_abstract_egl_backend.cpp"
