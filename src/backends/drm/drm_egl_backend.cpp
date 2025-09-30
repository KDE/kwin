/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2015 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "drm_egl_backend.h"
// kwin
#include "core/gpumanager.h"
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
#include "main.h"

// system
#include <drm_fourcc.h>
#include <gbm.h>
#include <unistd.h>

namespace KWin
{

EglGbmBackend::EglGbmBackend(DrmBackend *drmBackend)
    : EglBackend(kwinApp()->gpuManager()->findCompatibleDevice(drmBackend->primaryGpu()->drmDevice())->device)
    , m_backend(drmBackend)
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
    initClientExtensions();
    setEglDisplay(eglDisplayForDevice(m_backend->primaryGpu()->drmDevice()));
    m_backend->setSceneEglDisplayObject(eglDisplayObject());
    return true;
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
    return createContext() && openglContext()->makeCurrent();
}

DrmDevice *EglGbmBackend::scanoutDevice() const
{
    return gpu()->drmDevice();
}

QList<OutputLayer *> EglGbmBackend::compatibleOutputLayers(Output *output)
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
