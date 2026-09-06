/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2006 Lubos Lunak <l.lunak@kde.org>
    SPDX-FileCopyrightText: 2009, 2010, 2011 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "opengl/eglbackend.h"
#include "compositor.h"
#include "core/drm_formats.h"
#include "core/drmdevice.h"
#include "core/gpumanager.h"
#include "core/graphicsbuffer.h"
#include "core/outputbackend.h"
#include "core/renderdevice.h"
#include "main.h"
#include "opengl/eglimagetexture.h"
#include "opengl/eglutils_p.h"
#include "utils/common.h"
#include "utils/envvar.h"
#include "vulkan/vulkan_device.h"
#include "wayland/linux_drm_syncobj_v1.h"
#include "wayland_server.h"

#include <QElapsedTimer>

#include <drm_fourcc.h>
#include <unistd.h>
#include <xf86drm.h>

namespace KWin
{

EglBackend::EglBackend(RenderDevice *device)
    : m_renderDevice(device)
{
    connect(GpuManager::s_self.get(), &GpuManager::renderDeviceAdded, this, &EglBackend::updateDmabufTranches);
    connect(GpuManager::s_self.get(), &GpuManager::renderDeviceRemoved, this, &EglBackend::updateDmabufTranches);
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

void EglBackend::cleanup()
{
    m_context.reset();
}

void EglBackend::initWayland()
{
    updateDmabufTranches();
}

static const auto s_dmabufV6Env = environmentVariableBoolValue("KWIN_ALLOW_DMABUF_V6");

void EglBackend::updateDmabufTranches()
{
    auto filterFormats = [this](RenderDevice *device, std::optional<uint32_t> bpc) {
        FormatModifierMap set;
        auto allFormats = device->eglDisplay()->allSupportedDrmFormats();
        auto nonExternalOnly = device->eglDisplay()->nonExternalOnlySupportedDrmFormats();
        if (device->vulkanDevice() && !device->vulkanDevice()->isSoftwareRenderer()) {
            allFormats = allFormats.intersected(device->vulkanDevice()->transferFormats());
            nonExternalOnly = nonExternalOnly.intersected(device->vulkanDevice()->transferFormats());
        }
        for (auto it = allFormats.constBegin(); it != allFormats.constEnd(); it++) {
            const auto info = FormatInfo::get(it.key());
            if (bpc && (!info || bpc != info->bitsPerColor)) {
                continue;
            }

            ModifierList modifiers = *it;
            // Work around Xwayland breaking with the Nvidia driver
            // if linear is advertised by the compositor, since it uses
            // all advertised modifiers (for 8 bpc rgb formats) for rendering
            if (!nonExternalOnly[it.key()].contains(DRM_FORMAT_MOD_LINEAR)) {
                modifiers.removeOne(DRM_FORMAT_MOD_LINEAR);
            }

            for (const auto &tranche : std::as_const(m_tranches)) {
                if (modifiers.empty()) {
                    break;
                }
                if (tranche.device != device->deviceId()) {
                    continue;
                }
                const auto trancheModifiers = tranche.formatTable.value(it.key());
                for (auto trancheModifier : trancheModifiers) {
                    modifiers.erase(trancheModifier);
                }
            }
            if (modifiers.empty()) {
                continue;
            }
            set.insert(it.key(), modifiers);
        }
        return set;
    };

    m_tranches.clear();

    // put the "main" device first, with EGL format+modifiers
    m_tranches.append({
        .device = m_renderDevice->deviceId(),
        .flags = LinuxDmaBufV1Feedback::TrancheFlag::Sampling,
        .formatTable = filterFormats(m_renderDevice, 10),
    });
    m_tranches.append({
        .device = m_renderDevice->deviceId(),
        .flags = LinuxDmaBufV1Feedback::TrancheFlag::Sampling,
        .formatTable = filterFormats(m_renderDevice, 8),
    });
    m_tranches.append({
        .device = m_renderDevice->deviceId(),
        .flags = LinuxDmaBufV1Feedback::TrancheFlag::Sampling,
        .formatTable = filterFormats(m_renderDevice, std::nullopt),
    });

    // Other GPUs come afterwards, in no particular order.
    // Until the copy code can handle them, YUV formats are excluded from this
    const auto &devices = GpuManager::s_self->renderDevices();
    for (const auto &device : devices) {
        if (device.get() == m_renderDevice) {
            continue;
        }
        if (!s_dmabufV6Env.value_or(true)) {
            continue;
        }
        m_tranches.append({
            .device = device->deviceId(),
            .flags = LinuxDmaBufV1Feedback::TrancheFlag::Sampling,
            .formatTable = filterFormats(device.get(), 10),
        });
        m_tranches.append({
            .device = device->deviceId(),
            .flags = LinuxDmaBufV1Feedback::TrancheFlag::Sampling,
            .formatTable = filterFormats(device.get(), 8),
        });
        m_tranches.push_back({
            .device = device->deviceId(),
            .flags = LinuxDmaBufV1Feedback::TrancheFlag::Sampling,
            .formatTable = filterFormats(device.get(), std::nullopt),
        });
    }

    LinuxDmaBufV1ClientBufferIntegration *dmabuf = waylandServer()->linuxDmabuf();
    dmabuf->setRenderBackend(this);
    dmabuf->setSupportedFormatsWithModifiers(m_tranches);
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

bool EglBackend::createContext()
{
    m_context = m_renderDevice->eglContext();
    return m_context != nullptr;
}

QList<LinuxDmaBufV1Feedback::Tranche> EglBackend::tranches() const
{
    return m_tranches;
}

bool EglBackend::testImportBuffer(GraphicsBuffer *buffer, dev_t targetDevice)
{
    RenderDevice *device = GpuManager::self()->compatibleRenderDevice(targetDevice);
    if (!device) {
        return false;
    }

    if (device != m_renderDevice && device->vulkanDevice() && device->vulkanDevice()->transferFormats().containsFormat(buffer->dmabufAttributes()->format, buffer->dmabufAttributes()->modifier)) {
        if (device->vulkanDevice()->importBuffer(buffer, VK_IMAGE_USAGE_TRANSFER_SRC_BIT)) {
            return true;
        }
        // allow falling back to EGL
    }

    // NOTE for YUV buffers this uses whatever default values
    // the OpenGL driver chooses, since we don't know yet which
    // parameters the client will use with the buffer later on
    return device->eglDisplay()->importBufferAsImage(buffer) != EGL_NO_IMAGE_KHR;
}

FormatModifierMap EglBackend::supportedFormats() const
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

RenderDevice *EglBackend::renderDevice() const
{
    return m_renderDevice;
}

} // namespace KWin

#include "moc_eglbackend.cpp"
