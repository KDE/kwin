/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2015 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "drm_egl_backend.h"
// kwin
#include "core/renderdevice.h"
#include "core/syncobjtimeline.h"
#include "drm_abstract_output.h"
#include "drm_backend.h"
#include "drm_egl_layer.h"
#include "drm_gpu.h"
#include "drm_logging.h"
#include "drm_output.h"
#include "drm_pipeline.h"
#include "drm_virtual_egl_layer.h"
#include "drm_virtual_output.h"
// system
#include <drm_fourcc.h>
#include <gbm.h>
#include <unistd.h>

namespace KWin
{

EglGbmBackend::EglGbmBackend(DrmBackend *drmBackend)
    : m_backend(drmBackend)
{
    drmBackend->setRenderBackend(this);
}

EglGbmBackend::~EglGbmBackend()
{
    m_backend->releaseBuffers();
    cleanup();
    m_backend->setRenderBackend(nullptr);
}

bool EglGbmBackend::initializeEgl()
{
    if (!initClientExtensions()) {
        return false;
    }
    auto device = m_backend->primaryGpu()->renderDevice();
    if (!device) {
        device = createRenderDevice(m_backend->primaryGpu());
    }
    setRenderDevice(device);
    return device;
}

RenderDevice *EglGbmBackend::createRenderDevice(DrmGpu *gpu) const
{
    // Temporary EGL display for finding a compatible render node.
    // TODO find it in a different way!
    const auto gpuDisplay = EglDisplay::create(eglGetPlatformDisplayEXT(EGL_PLATFORM_GBM_KHR, gpu->drmDevice()->gbmDevice(), nullptr));
    if (!gpuDisplay) {
        return nullptr;
    } else if (gpuDisplay->renderNode().isEmpty()) {
        // as a fallback for software rendering, use the primary node
        gpu->setRenderDevice(RenderDevice::open(gpu->drmDevice()->path(), gpu->fd()));
    } else {
        gpu->setRenderDevice(RenderDevice::open(gpuDisplay->renderNode()));
    }
    return gpu->renderDevice();
}

bool EglGbmBackend::init()
{
    if (!initializeEgl()) {
        qCWarning(KWIN_DRM, "Could not initialize egl");
        return false;
    }

    if (!createContext()) {
        qCWarning(KWIN_DRM, "Could not initialize rendering context");
        return false;
    }
    initWayland();
    m_backend->createLayers();
    return true;
}

RenderDevice *EglGbmBackend::renderDeviceForGpu(DrmGpu *gpu)
{
    auto device = gpu->renderDevice();
    if (!device) {
        device = createRenderDevice(gpu);
    }
    return device;
}

std::shared_ptr<EglContext> EglGbmBackend::contextForGpu(DrmGpu *gpu)
{
    const auto device = renderDeviceForGpu(gpu);
    return device ? device->eglContext() : nullptr;
}

DrmDevice *EglGbmBackend::drmDevice() const
{
    return gpu()->drmDevice();
}

QList<OutputLayer *> EglGbmBackend::compatibleOutputLayers(BackendOutput *output)
{
    if (auto virtualOutput = qobject_cast<DrmVirtualOutput *>(output)) {
        return {virtualOutput->primaryLayer()};
    } else {
        return static_cast<DrmOutput *>(output)->pipeline()->gpu()->compatibleOutputLayers(output);
    }
}

std::unique_ptr<DrmPipelineLayer> EglGbmBackend::createDrmPlaneLayer(DrmPlane *plane)
{
    return std::make_unique<EglGbmLayer>(this, plane);
}

std::unique_ptr<DrmPipelineLayer> EglGbmBackend::createDrmPlaneLayer(DrmGpu *gpu, DrmPlane::TypeIndex type)
{
    return std::make_unique<EglGbmLayer>(this, gpu, type);
}

std::unique_ptr<DrmOutputLayer> EglGbmBackend::createLayer(DrmVirtualOutput *output)
{
    return std::make_unique<VirtualEglGbmLayer>(this, output);
}

DrmGpu *EglGbmBackend::gpu() const
{
    return m_backend->primaryGpu();
}

} // namespace KWin

#include "moc_drm_egl_backend.cpp"
