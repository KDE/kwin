/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2015 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "drm_egl_backend.h"
// kwin
#include "core/syncobjtimeline.h"
#include "drm_abstract_output.h"
#include "drm_backend.h"
#include "drm_egl_layer.h"
#include "drm_gpu.h"
#include "drm_logging.h"
#include "drm_output.h"
#include "drm_pipeline.h"
#include "drm_virtual_egl_layer.h"
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
    connect(m_backend, &DrmBackend::gpuRemoved, this, [this](DrmGpu *gpu) {
        m_contexts.erase(gpu->eglDisplay());
    });
}

EglGbmBackend::~EglGbmBackend()
{
    m_backend->releaseBuffers();
    const auto outputs = m_backend->outputs();
    for (const auto output : outputs) {
        if (auto drmOutput = dynamic_cast<DrmOutput *>(output)) {
            drmOutput->pipeline()->setLayers(nullptr, nullptr);
        }
    }
    m_contexts.clear();
    cleanup();
    m_backend->setRenderBackend(nullptr);
}

bool EglGbmBackend::initializeEgl()
{
    initClientExtensions();
    auto display = m_backend->primaryGpu()->eglDisplay();

    // Use eglGetPlatformDisplayEXT() to get the display pointer
    // if the implementation supports it.
    if (!display) {
        display = createEglDisplay(m_backend->primaryGpu());
        if (!display) {
            return false;
        }
    }
    setEglDisplay(display);
    return true;
}

EglDisplay *EglGbmBackend::createEglDisplay(DrmGpu *gpu) const
{
    for (const QByteArray &extension : {QByteArrayLiteral("EGL_EXT_platform_base"), QByteArrayLiteral("EGL_KHR_platform_gbm")}) {
        if (!hasClientExtension(extension)) {
            qCWarning(KWIN_DRM) << extension << "client extension is not supported by the platform";
            return nullptr;
        }
    }

    gpu->setEglDisplay(EglDisplay::create(eglGetPlatformDisplayEXT(EGL_PLATFORM_GBM_KHR, gpu->drmDevice()->gbmDevice(), nullptr)));
    return gpu->eglDisplay();
}

void EglGbmBackend::init()
{
    if (!initializeEgl()) {
        setFailed("Could not initialize egl");
        return;
    }

    if (!initRenderingContext()) {
        setFailed("Could not initialize rendering context");
        return;
    }
    initWayland();
    m_backend->createLayers();
}

bool EglGbmBackend::initRenderingContext()
{
    return createContext(EGL_NO_CONFIG_KHR) && openglContext()->makeCurrent();
}

EglDisplay *EglGbmBackend::displayForGpu(DrmGpu *gpu)
{
    if (gpu == m_backend->primaryGpu()) {
        return eglDisplayObject();
    }
    auto display = gpu->eglDisplay();
    if (!display) {
        display = createEglDisplay(gpu);
    }
    return display;
}

std::shared_ptr<EglContext> EglGbmBackend::contextForGpu(DrmGpu *gpu)
{
    if (gpu == m_backend->primaryGpu()) {
        return m_context;
    }
    auto display = gpu->eglDisplay();
    if (!display) {
        display = createEglDisplay(gpu);
        if (!display) {
            return nullptr;
        }
    }
    auto &context = m_contexts[display];
    if (const auto c = context.lock()) {
        return c;
    }
    const auto ret = std::shared_ptr<EglContext>(EglContext::create(display, EGL_NO_CONFIG_KHR, EGL_NO_CONTEXT));
    context = ret;
    return ret;
}

void EglGbmBackend::resetContextForGpu(DrmGpu *gpu)
{
    m_contexts.erase(gpu->eglDisplay());
}

DrmDevice *EglGbmBackend::drmDevice() const
{
    return gpu()->drmDevice();
}

bool EglGbmBackend::present(Output *output, const std::shared_ptr<OutputFrame> &frame)
{
    return static_cast<DrmAbstractOutput *>(output)->present(frame);
}

void EglGbmBackend::repairPresentation(Output *output)
{
    // read back drm properties, most likely our info is out of date somehow
    // or we need a modeset
    QTimer::singleShot(0, m_backend, &DrmBackend::updateOutputs);
}

OutputLayer *EglGbmBackend::primaryLayer(Output *output)
{
    return static_cast<DrmAbstractOutput *>(output)->primaryLayer();
}

OutputLayer *EglGbmBackend::cursorLayer(Output *output)
{
    return static_cast<DrmAbstractOutput *>(output)->cursorLayer();
}

std::pair<std::shared_ptr<KWin::GLTexture>, ColorDescription> EglGbmBackend::textureForOutput(Output *output) const
{
    const auto drmOutput = static_cast<DrmAbstractOutput *>(output);
    if (const auto virtualLayer = dynamic_cast<VirtualEglGbmLayer *>(drmOutput->primaryLayer())) {
        return std::make_pair(virtualLayer->texture(), virtualLayer->colorDescription());
    }
    const auto layer = static_cast<EglGbmLayer *>(drmOutput->primaryLayer());
    return std::make_pair(layer->texture(), layer->colorDescription());
}

std::shared_ptr<DrmPipelineLayer> EglGbmBackend::createDrmPlaneLayer(DrmPipeline *pipeline, DrmPlane::TypeIndex type)
{
    return std::make_shared<EglGbmLayer>(this, pipeline, type);
}

std::shared_ptr<DrmOutputLayer> EglGbmBackend::createLayer(DrmVirtualOutput *output)
{
    return std::make_shared<VirtualEglGbmLayer>(this, output);
}

DrmGpu *EglGbmBackend::gpu() const
{
    return m_backend->primaryGpu();
}

} // namespace KWin

#include "moc_drm_egl_backend.cpp"
