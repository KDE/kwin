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

OutputLayer *DrmQPainterBackend::primaryLayer(Output *output)
{
    return static_cast<DrmAbstractOutput *>(output)->primaryLayer();
}

OutputLayer *DrmQPainterBackend::cursorLayer(Output *output)
{
    return static_cast<DrmAbstractOutput *>(output)->cursorLayer();
}

std::shared_ptr<DrmPipelineLayer> DrmQPainterBackend::createDrmPlaneLayer(DrmPipeline *pipeline, DrmPlane::TypeIndex type)
{
    return std::make_shared<DrmQPainterLayer>(pipeline, type);
}

std::shared_ptr<DrmOutputLayer> DrmQPainterBackend::createLayer(DrmVirtualOutput *output)
{
    return std::make_shared<DrmVirtualQPainterLayer>(output);
}

}

#include "moc_drm_qpainter_backend.cpp"
