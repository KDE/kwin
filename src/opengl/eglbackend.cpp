/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2006 Lubos Lunak <l.lunak@kde.org>
    SPDX-FileCopyrightText: 2009, 2010, 2011 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "opengl/eglbackend.h"
#include "compositor.h"
#include "core/backendoutput.h"
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
#include "vulkan/vulkan_device.h"
#include "wayland/linux_drm_syncobj_v1.h"
#include "wayland_server.h"

#include <QElapsedTimer>

#include <drm_fourcc.h>
#include <unistd.h>
#include <xf86drm.h>

namespace KWin
{

EglBackend::EglBackend()
{
    connect(GpuManager::s_self.get(), &GpuManager::renderDeviceAdded, this, &EglBackend::updateDmabufTranches);
    connect(GpuManager::s_self.get(), &GpuManager::renderDeviceRemoved, this, &EglBackend::updateDmabufTranches);
}

CompositingType EglBackend::compositingType() const
{
    return OpenGLCompositing;
}

bool EglBackend::checkGraphicsReset(RenderDevice *device)
{
    const auto context = device->eglContext();
    const bool success = context && context->makeCurrent();
    if (!success) {
        // not necessarily a graphics reset, but we can't really know
        // and need to re-create everything either way
        return true;
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

void EglBackend::updateDmabufTranches()
{
    auto filterFormats = [this](RenderDevice *device, std::optional<uint32_t> bpc, bool withExternalOnlyYUV) {
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

            const bool externalOnlySupported = withExternalOnlyYUV && info && info->yuvConversion();
            ModifierList modifiers = externalOnlySupported ? *it : nonExternalOnly[it.key()];

            if (externalOnlySupported && !modifiers.empty()) {
                if (auto yuv = info->yuvConversion()) {
                    for (auto plane : std::as_const(yuv->plane)) {
                        const auto planeModifiers = allFormats.value(plane.format);
                        erase_if(modifiers, [&planeModifiers](uint64_t mod) {
                            return !planeModifiers.contains(mod);
                        });
                    }
                }
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

    auto includeShaderConversions = [](FormatModifierMap &&formats) -> FormatModifierMap {
        for (auto format : FormatInfo::s_drmConversions.keys()) {
            auto &modifiers = formats[format];
            if (modifiers.empty()) {
                modifiers = {DRM_FORMAT_MOD_LINEAR};
            }
        }
        return formats;
    };

    m_tranches.clear();

    // put the "main" device first, with EGL format+modifiers
    const auto main = Compositor::self()->primaryDevice();
    m_tranches.append({
        .device = main->deviceId(),
        .flags = LinuxDmaBufV1Feedback::TrancheFlag::Sampling,
        .formatTable = filterFormats(main, 10, false),
    });
    m_tranches.append({
        .device = main->deviceId(),
        .flags = LinuxDmaBufV1Feedback::TrancheFlag::Sampling,
        .formatTable = filterFormats(main, 8, false),
    });
    m_tranches.append({
        .device = main->deviceId(),
        .flags = LinuxDmaBufV1Feedback::TrancheFlag::Sampling,
        .formatTable = includeShaderConversions(filterFormats(main, std::nullopt, true)),
    });

    // Other GPUs come afterwards, in no particular order.
    // Until the copy code can handle them, YUV formats are excluded from this
    const auto &devices = GpuManager::s_self->renderDevices();
    for (const auto &device : devices) {
        if (device.get() == main) {
            continue;
        }
        m_tranches.append({
            .device = device->deviceId(),
            .flags = LinuxDmaBufV1Feedback::TrancheFlag::Sampling,
            .formatTable = filterFormats(device.get(), 10, false),
        });
        m_tranches.append({
            .device = device->deviceId(),
            .flags = LinuxDmaBufV1Feedback::TrancheFlag::Sampling,
            .formatTable = filterFormats(device.get(), 8, false),
        });
        m_tranches.push_back({
            .device = device->deviceId(),
            .flags = LinuxDmaBufV1Feedback::TrancheFlag::Sampling,
            .formatTable = filterFormats(device.get(), std::nullopt, false),
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

    const auto info = FormatInfo::get(buffer->dmabufAttributes()->format);
    if (info && info->yuvConversion()) {
        // YUV buffers need to be imported plane for plane
        const auto planes = info->yuvConversion()->plane;
        if (buffer->dmabufAttributes()->planeCount != planes.size()) {
            return false;
        }
        for (int i = 0; i < planes.size(); i++) {
            if (!device->eglDisplay()->importBufferAsImage(buffer, i, planes[i].format, QSize(buffer->size().width() / planes[i].widthDivisor, buffer->size().height() / planes[i].heightDivisor))) {
                return false;
            }
        }
        return true;
    }

    // other formats can be imported normally
    return device->eglDisplay()->importBufferAsImage(buffer) != EGL_NO_IMAGE_KHR;
}

RenderDevice *EglBackend::renderDevice(BackendOutput *output) const
{
    return Compositor::self()->primaryDevice();
}

FormatModifierMap EglBackend::supportedFormats(RenderDevice *device) const
{
    return device->eglDisplay()->nonExternalOnlySupportedDrmFormats();
}

} // namespace KWin

#include "moc_eglbackend.cpp"
