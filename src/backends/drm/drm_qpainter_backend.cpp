/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2015 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "drm_qpainter_backend.h"
#include "drm_backend.h"
#include "drm_gpu.h"
#include "drm_output.h"
#include "drm_pipeline.h"
#include "drm_qpainter_layer.h"
#include "drm_virtual_output.h"

#include <drm_fourcc.h>

namespace KWin
{

DrmQPainterBackend::DrmQPainterBackend(DrmBackend *backend)
    : QPainterBackend()
    , m_backend(backend)
{
    m_backend->setRenderBackend(this);
    m_backend->createLayers();
}

DrmQPainterBackend::~DrmQPainterBackend()
{
    m_backend->releaseBuffers();
    m_backend->setRenderBackend(nullptr);
}

DrmDevice *DrmQPainterBackend::drmDevice() const
{
    return m_backend->primaryGpu()->drmDevice();
}

QList<OutputLayer *> DrmQPainterBackend::compatibleOutputLayers(Output *output)
{
    if (auto virtualOutput = qobject_cast<DrmVirtualOutput *>(output)) {
        return {virtualOutput->primaryLayer()};
    } else {
        return static_cast<DrmOutput *>(output)->pipeline()->gpu()->compatibleOutputLayers(output);
    }
}

std::unique_ptr<DrmPipelineLayer> DrmQPainterBackend::createDrmPlaneLayer(DrmPlane *plane)
{
    return std::make_unique<DrmQPainterLayer>(plane);
}

std::unique_ptr<DrmPipelineLayer> DrmQPainterBackend::createDrmPlaneLayer(DrmGpu *gpu, DrmPlane::TypeIndex type)
{
    return std::make_unique<DrmQPainterLayer>(type);
}

std::unique_ptr<DrmOutputLayer> DrmQPainterBackend::createLayer(DrmVirtualOutput *output)
{
    return std::make_unique<DrmVirtualQPainterLayer>(output);
}
}

#include "moc_drm_qpainter_backend.cpp"
